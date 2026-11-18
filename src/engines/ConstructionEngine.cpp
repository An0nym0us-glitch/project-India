#include "ConstructionEngine.h"
#include "catalog/Catalog.h"
#include "gis/GeoDatabase.h"
#include "gis/GeoMath.h"
#include <QSet>
#include <algorithm>

namespace isim {

double ConstructionEngine::effectiveCapex(const SimContext &ctx, const FacilityDefinition &d, double scale)
{
    double m = ctx.mods.mul("capex.mult." + d.category);
    for (const auto &t : d.tags) m *= ctx.mods.mul("capex.mult.tag." + t);
    return d.capexCr * scale * m;
}

double ConstructionEngine::publicShareFor(const SimContext &ctx, const FacilityDefinition &d)
{
    double s = d.publicOwned ? 1.0 + ctx.mods.add("capex.publicShare.public")
                             : ctx.p("fiscal.privateFacilityPublicShare") + ctx.mods.add("capex.publicShare.private");
    for (const auto &t : d.tags) s += ctx.mods.add("capex.publicShare.tag." + t);
    return std::clamp(s, 0.0, 1.0);
}

PlacementResult ConstructionEngine::place(SimContext &ctx, const PlacementRequest &req)
{
    PlacementResult r;
    const auto *d = ctx.cat.facility(req.defId);
    if (!d) { r.error = "Unknown facility type " + req.defId; return r; }
    PlacedFacility f;
    f.defId = d->id;
    f.label = req.label;
    if (d->isLine()) {
        if (req.path.size() < 2) { r.error = "A line facility needs at least two points."; return r; }
        f.path = req.path;
        f.location = req.path.first();
        f.lengthKm = geo::polylineLengthKm(req.path);
        if (f.lengthKm < 0.5) { r.error = "Line is too short."; return r; }
        f.scale = f.lengthKm;
        // districts touched: sample every ~5 km along the line
        QSet<int> ds;
        for (int i = 1; i < req.path.size(); ++i) {
            const double seg = geo::haversineKm(req.path[i - 1], req.path[i]);
            const int steps = std::max(1, int(seg / 5.0));
            for (int s = 0; s <= steps; ++s) {
                const QPointF p = req.path[i - 1] + (req.path[i] - req.path[i - 1]) * (double(s) / steps);
                const int di = ctx.geo.districtAt(p);
                if (di >= 0 && !ds.contains(di)) { ds.insert(di); f.districts << di; }
            }
        }
        if (f.districts.isEmpty()) { r.error = "The line does not pass through Indian territory."; return r; }
        f.district = f.districts.first();
    } else {
        f.location = req.location;
        f.district = ctx.geo.districtAt(req.location);
        if (f.district < 0) { r.error = "Location is outside India's land territory."; return r; }
        f.districts = {f.district};
        f.scale = 1.0;
    }
    // hard physical constraints
    if (d->maxCoastKm >= 0 && !d->isLine()) {
        const double km = ctx.geo.distanceToCoastKm(f.location);
        if (km > d->maxCoastKm) { r.error = QStringLiteral("%1 must be within %2 km of the coast (this site: %3 km).").arg(d->name).arg(d->maxCoastKm).arg(km, 0, 'f', 0); return r; }
    }
    if (d->needsMountain && !d->isLine() && !ctx.geo.isMountainous(f.location)) { r.error = d->name + " requires mountainous terrain."; return r; }

    f.uid = ctx.w.nextUid++;
    f.placedMonth = ctx.w.month;
    f.capexCr = effectiveCapex(ctx, *d, f.scale);
    f.publicShare = publicShareFor(ctx, *d);
    f.status = FacilityStatus::UnderConstruction;
    ctx.w.facilities << f;
    ctx.net.dirty = true;
    r.ok = true;
    r.uid = f.uid;
    r.capexCr = f.capexCr;
    r.publicCostCr = f.capexCr * f.publicShare;
    const QString where = ctx.geo.districts()[f.district].name + ", " + ctx.geo.districts()[f.district].stateName;
    ctx.w.notify(Notification::Construction, QStringLiteral("Construction started: %1 in %2 (₹%3 crore%4).")
        .arg(d->name, where).arg(f.capexCr, 0, 'f', 0).arg(d->isLine() ? QStringLiteral(", %1 km").arg(f.lengthKm, 0, 'f', 0) : QString()));
    return r;
}

bool ConstructionEngine::demolish(SimContext &ctx, int uid)
{
    for (int i = 0; i < ctx.w.facilities.size(); ++i) {
        if (ctx.w.facilities[i].uid != uid) continue;
        const auto *d = ctx.def(ctx.w.facilities[i]);
        ctx.w.notify(Notification::Construction, QStringLiteral("Removed: %1 (sunk costs are not recovered).").arg(d ? d->name : ctx.w.facilities[i].defId));
        ctx.w.facilities.remove(i);
        ctx.net.crossingCache.remove(uid);
        ctx.net.dirty = true;
        return true;
    }
    return false;
}

void ConstructionEngine::step(SimContext &ctx, double &publicPaid, double &privatePaid)
{
    publicPaid = privatePaid = 0;
    const double speed = ctx.mods.mul("construction.speed");
    // private capital constraint: monthly pool
    const double pool = ctx.w.stats.gdpCr * ctx.p("macro.privateSavingsRate") * ctx.mods.mul("private.investment") / 12.0;
    double privateDemand = 0;
    for (const auto &f : ctx.w.facilities) {
        if (f.status != FacilityStatus::UnderConstruction) continue;
        const auto *d = ctx.def(f);
        if (!d) continue;
        privateDemand += f.capexCr * (1 - f.publicShare) * speed / std::max(1, d->constructionMonths);
    }
    const double privateFactor = privateDemand > 0 ? std::min(1.0, pool / privateDemand) : 1.0;
    ctx.w.budget.privateCapitalAvailableCrYr = pool * 12;
    for (auto &f : ctx.w.facilities) {
        if (f.status != FacilityStatus::UnderConstruction) continue;
        const auto *d = ctx.def(f);
        if (!d) continue;
        // privately financed share progresses only as fast as private capital allows
        const double factor = f.publicShare + (1 - f.publicShare) * privateFactor;
        const double dp = std::min(1.0 - f.progress, speed * factor / std::max(1, d->constructionMonths));
        f.progress += dp;
        publicPaid += f.capexCr * f.publicShare * dp;
        privatePaid += f.capexCr * (1 - f.publicShare) * dp;
        if (f.progress >= 0.999) {
            f.progress = 1.0;
            f.status = FacilityStatus::Operating;
            f.completedMonth = ctx.w.month;
            ctx.net.dirty = true;
            if (!ctx.fastMode) {
                const auto &dist = ctx.geo.districts()[f.district];
                ctx.w.notify(Notification::Construction, QStringLiteral("Commissioned: %1 in %2, %3.").arg(d->name, dist.name, dist.stateName));
            }
        }
    }
    if (privateFactor < 0.999 && !ctx.fastMode && ctx.w.month % 6 == 0)
        ctx.w.notify(Notification::Economy, QStringLiteral("Private capital is constrained: privately financed projects are progressing at %1% of planned speed.").arg(privateFactor * 100, 0, 'f', 0));
}

} // namespace isim
