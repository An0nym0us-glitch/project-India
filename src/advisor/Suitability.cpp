#include "Suitability.h"
#include "catalog/Catalog.h"
#include "engines/Aqi.h"
#include "engines/ConstructionEngine.h"
#include "engines/EconomyEngine.h"
#include "engines/NetworkEngine.h"
#include "engines/UtilityEngine.h"
#include "gis/GeoDatabase.h"
#include "gis/GeoMath.h"
#include <QLocale>
#include <algorithm>
#include <cmath>

namespace isim {

QString statusName(SuitabilityStatus s)
{
    switch (s) {
    case SuitabilityStatus::Recommended: return QStringLiteral("Recommended");
    case SuitabilityStatus::FeasibleWithGaps: return QStringLiteral("Feasible, with gaps to address");
    case SuitabilityStatus::NotRecommended: return QStringLiteral("Not recommended for immediate construction");
    case SuitabilityStatus::NotFeasible: return QStringLiteral("Not feasible at this location");
    }
    return {};
}

QString SuitabilityEngine::facilityForSkill(int skill)
{
    switch (skill) {
    case Skilled: return QStringLiteral("vocational_institute");
    case Engineer: return QStringLiteral("engineering_college");
    case Scientist: return QStringLiteral("research_university");
    case Medical: return QStringLiteral("medical_college");
    default: return QStringLiteral("secondary_school");
    }
}

QStringList SuitabilityEngine::facilitiesForTechDomain(const SimContext &ctx, const QString &domain, int max)
{
    // rank research facilities by points per ₹1,000 crore
    QVector<QPair<double, QString>> r;
    for (const auto &f : ctx.cat.facilities()) {
        const double pts = f.researchPoints.value(domain);
        if (pts <= 0) continue;
        r.append({pts / std::max(1.0, f.capexCr) * 1000.0 * (f.tags.contains("research") ? 1.5 : 1.0), f.id});
    }
    std::sort(r.begin(), r.end(), [](auto &a, auto &b) { return a.first > b.first; });
    QStringList out;
    for (int i = 0; i < r.size() && out.size() < max; ++i) out << r[i].second;
    return out;
}

QString SuitabilityEngine::cheapestWithTag(const SimContext &ctx, const QString &tag)
{
    const FacilityDefinition *best = nullptr;
    for (const auto &f : ctx.cat.facilities())
        if (f.tags.contains(tag) && (!best || f.capexCr < best->capexCr)) best = &f;
    return best ? best->id : QString();
}

static QString cr(double v)
{
    if (v >= 100000) return QStringLiteral("₹%1 lakh crore").arg(v / 100000, 0, 'f', 2);
    return QStringLiteral("₹%1 crore").arg(QLocale(QLocale::English).toString(v, 'f', 0));
}

SuitabilityReport SuitabilityEngine::assess(SimContext &ctx, const FacilityDefinition &d, const QPointF &loc, const QVector<QPointF> &path)
{
    SuitabilityReport r;
    auto &w = ctx.w;
    const auto &geo = ctx.geo;
    auto add = [&](const QString &area, CheckLevel lv, const QString &detail) { r.checks.append({area, lv, detail}); };
    auto prereq = [&](const QString &fid, const QString &why) {
        for (const auto &p : r.prerequisites) if (p.facilityId == fid) return;
        const auto *fd = ctx.cat.facility(fid);
        r.prerequisites.append({fid, fd ? fd->name : fid, why});
    };
    QPointF at = loc;
    double scale = 1.0;
    if (d.isLine() && path.size() >= 2) { at = path.first(); r.lengthKm = geo::polylineLengthKm(path); scale = r.lengthKm; }
    r.district = geo.districtAt(at);
    if (r.district < 0) {
        r.status = SuitabilityStatus::NotFeasible;
        r.statusText = statusName(r.status);
        r.reasons << "Location is outside India's land territory.";
        add("Location", CheckLevel::Blocker, "Outside Indian territory or in the sea.");
        return r;
    }
    const auto &dist = geo.districts()[r.district];
    const auto &ds = w.districts[r.district];
    r.location = dist.name + ", " + dist.stateName;
    r.capexCr = ConstructionEngine::effectiveCapex(ctx, d, scale);
    r.publicCostCr = r.capexCr * ConstructionEngine::publicShareFor(ctx, d);
    bool blocker = false;

    // ---- physical geography ----
    if (d.maxCoastKm >= 0) {
        const double km = geo.distanceToCoastKm(at);
        if (km > d.maxCoastKm) { blocker = true; add("Geography", CheckLevel::Blocker, QStringLiteral("Needs a coastal site within %1 km; this site is %2 km from the coast.").arg(d.maxCoastKm).arg(km, 0, 'f', 0)); r.reasons << "Not a coastal site"; }
        else add("Geography", CheckLevel::Ok, QStringLiteral("Coastal site (%1 km from coast).").arg(km, 0, 'f', 1));
    }
    if (d.maxRiverKm >= 0 && !d.isLine()) {
        const double km = geo.distanceToRiverKm(at);
        if (km > d.maxRiverKm) {
            const bool hard = d.tags.contains("hydro") || d.tags.contains("bridge") || d.tags.contains("waterway_terminal");
            if (hard) blocker = true;
            add("Water source", hard ? CheckLevel::Blocker : CheckLevel::Problem,
                QStringLiteral("Needs a major river within %1 km; nearest mapped major river is %2 km away (Natural Earth river layer shows major rivers only).").arg(d.maxRiverKm).arg(km > 1e6 ? 9999 : km, 0, 'f', 0));
            r.reasons << "No major river nearby";
        } else add("Water source", CheckLevel::Ok, QStringLiteral("Major river %1 km away.").arg(km, 0, 'f', 0));
    }
    if (d.needsMountain) {
        if (!geo.isMountainous(at)) { blocker = true; add("Terrain", CheckLevel::Blocker, "Requires mountainous terrain with elevation head (Himalayan/hill states or mapped ranges)."); r.reasons << "Terrain unsuitable"; }
        else add("Terrain", CheckLevel::Ok, "Mountainous terrain.");
    }
    if (!d.suitableStates.isEmpty()) {
        if (!d.suitableStates.contains(dist.stateName)) {
            const bool geology = d.tags.contains("mining");
            if (geology) blocker = true;
            add("Resource suitability", geology ? CheckLevel::Blocker : CheckLevel::Problem,
                QStringLiteral("%1 is not among the states identified as suitable (%2). Suitable: %3.").arg(dist.stateName, d.suitableStatesSource, d.suitableStates.join(", ")));
            r.reasons << "Region lacks the natural resource";
        } else add("Resource suitability", CheckLevel::Ok, QStringLiteral("%1 is identified as suitable (%2).").arg(dist.stateName, d.suitableStatesSource));
    }
    if (d.tags.contains("solar") && geo.isDesert(at)) add("Resource suitability", CheckLevel::Ok, "High-irradiance arid region (Thar).");
    if (d.needsUrban) {
        const double urbanShare = ds.population > 0 ? ds.urbanPopulation / ds.population : 0;
        if (ds.urbanPopulation < 300000 && urbanShare < 0.3)
            add("Urban context", CheckLevel::Warning, QStringLiteral("District is mostly rural (%1% urban); urban-type development may be under-occupied.").arg(urbanShare * 100, 0, 'f', 0));
        else add("Urban context", CheckLevel::Ok, QStringLiteral("Urban population ≈%1 lakh.").arg(ds.urbanPopulation / 1e5, 0, 'f', 1));
    }

    // ---- electricity ----
    double powerFactor = 1.0;
    if (d.needsGrid && d.powerDemandMW > 0) {
        const int isl = ctx.net.powerIslandOfDistrict.value(r.district, -1);
        if (isl < 0) {
            powerFactor = 0.0;
            add("Electricity", CheckLevel::Problem, QStringLiteral("No grid connection in %1 district: needs %2 MW but there is no substation/distribution network connected to generation.").arg(dist.name).arg(d.powerDemandMW * scale, 0, 'f', 1));
            r.reasons << "No electricity grid in the district";
            prereq("substation", "Electricity reaches a district only through a substation or distribution network.");
            prereq(dist.stateName == "Rajasthan" || dist.stateName == "Gujarat" ? "solar_farm" : "coal_plant_fgd", QStringLiteral("Generation is needed to supply %1 MW.").arg(d.powerDemandMW * scale, 0, 'f', 0));
        } else {
            const double served = ds.powerDemandMW > 0 ? ds.powerServedMW / ds.powerDemandMW : 0;
            powerFactor = served;
            const double spare = std::max(0.0, ds.distributionMW - ds.powerServedMW);
            if (served < 0.9) {
                add("Electricity", CheckLevel::Problem, QStringLiteral("Grid serves only %1% of current district demand; adding %2 MW worsens the deficit.").arg(served * 100, 0, 'f', 0).arg(d.powerDemandMW, 0, 'f', 0));
                r.reasons << "Electricity shortage";
                prereq("solar_farm", "Additional generation capacity is needed in this grid island.");
            } else if (spare < d.powerDemandMW) {
                add("Electricity", CheckLevel::Warning, QStringLiteral("District distribution capacity has %1 MW spare; facility needs %2 MW.").arg(spare, 0, 'f', 0).arg(d.powerDemandMW, 0, 'f', 0));
                prereq("distribution_network", "Distribution capacity must grow with new large loads.");
            } else add("Electricity", CheckLevel::Ok, QStringLiteral("Grid connected; %1% of demand served.").arg(served * 100, 0, 'f', 0));
            if (d.minGridReliability > 0) {
                const double rel = w.stats.gridReliability;
                if (rel < d.minGridReliability) {
                    add("Power reliability", CheckLevel::Problem, QStringLiteral("Requires ≥%1% supply reliability; national grid reliability is %2%.").arg(d.minGridReliability * 100, 0, 'f', 1).arg(rel * 100, 0, 'f', 1));
                    r.reasons << "Insufficient power reliability";
                    prereq("battery_storage", "Storage firms supply and improves reliability for sensitive loads.");
                    prereq("gas_plant", "Flexible generation improves reliability.");
                    powerFactor *= std::max(0.1, rel / d.minGridReliability) * 0.6;
                }
            }
        }
    }
    // ---- water ----
    if (d.needsWater) {
        const int wn = ctx.net.waterNetOfDistrict.value(r.district, -1);
        const double frac = ds.waterDemandMLD > 0 ? ds.waterSuppliedMLD / ds.waterDemandMLD : 0;
        if (wn < 0 || frac < 0.8) {
            add("Water", CheckLevel::Warning, QStringLiteral("Needs %1 MLD; piped supply covers %2% of district demand, so the plant would rely partly on self-supply (groundwater).").arg(d.waterDemandMLD * scale, 0, 'f', 1).arg(frac * 100, 0, 'f', 0));
            prereq(geo.distanceToCoastKm(at) < 10 ? "desalination" : "water_treatment", "A reliable treated-water source for industrial use.");
        } else add("Water", CheckLevel::Ok, QStringLiteral("Piped water supply covers %1% of district demand.").arg(frac * 100, 0, 'f', 0));
    }

    // ---- workforce ----
    const int st = dist.stateIndex;
    double staffing = 1.0;
    if (d.operationsJobs.total() > 0 && st >= 0) {
        SkillVector demand;
        for (const auto &f : w.facilities) {
            if (f.district < 0 || geo.districts()[f.district].stateIndex != st || !f.isOperating()) continue;
            const auto *fd = ctx.def(f);
            if (fd) demand += fd->operationsJobs.scaled(fd->isLine() ? f.scale : 1.0);
        }
        const auto pool = w.skills.byState.value(st);
        const auto national = w.skills.national();
        double avg = 0, mn = 1;
        QStringList shortages;
        for (int k = Skilled; k < SkillCount; ++k) {
            const double need = d.operationsJobs[k] * scale;
            if (need <= 0) continue;
            const double avail = std::max(0.0, pool[k] - demand[k]) + ctx.p("skills.mobility") * std::max(0.0, national[k] - pool[k]) * 0.2;
            const double fill = std::min(1.0, avail / need);
            avg += fill * need; mn = std::min(mn, fill);
            if (fill < 0.9) {
                shortages << QStringLiteral("%1: need %2, ≈%3 available").arg(skillName(k)).arg(need, 0, 'f', 0).arg(avail, 0, 'f', 0);
                prereq(facilityForSkill(k), QStringLiteral("Trains %1 — the scarcest input for this facility in %2.").arg(skillName(k).toLower(), dist.stateName));
            }
        }
        const double skilledNeed = d.operationsJobs.total() - d.operationsJobs[Unskilled];
        if (skilledNeed > 0) staffing = 0.5 * avg / skilledNeed + 0.5 * mn;
        if (!shortages.isEmpty()) { add("Workforce", staffing < 0.6 ? CheckLevel::Problem : CheckLevel::Warning, "Specialised workforce gaps — " + shortages.join("; ") + "."); r.reasons << "Insufficient specialised workforce"; }
        else add("Workforce", CheckLevel::Ok, QStringLiteral("%1 operations jobs can be staffed from the %2 workforce.").arg(d.operationsJobs.total() * scale, 0, 'f', 0).arg(dist.stateName));
    }

    // ---- technology maturity ----
    QStringList techWhy;
    const double techF = EconomyEngine::techFactor(ctx, d, &techWhy);
    if (!techWhy.isEmpty()) {
        add("Technology maturity", techF < 0.2 ? CheckLevel::Problem : CheckLevel::Warning, techWhy.join("; ") + QStringLiteral(". Expected output factor: %1%.").arg(techF * 100, 0, 'f', 0));
        r.reasons << "Technology ecosystem not mature enough";
        for (auto it = d.techRequired.begin(); it != d.techRequired.end(); ++it) {
            if (w.techLevel.value(it.key()) >= it.value()) continue;
            const auto *dom = ctx.cat.techDomain(it.key());
            for (const auto &fid : facilitiesForTechDomain(ctx, it.key(), 2))
                prereq(fid, QStringLiteral("Raises %1 maturity (now level %2, need %3) through research and trained specialists.").arg(dom ? dom->name : it.key()).arg(w.techLevel.value(it.key())).arg(it.value()));
            // facilities that the next level requires
            if (dom) {
                const int next = w.techLevel.value(it.key()) + 1;
                if (next < dom->levels.size())
                    for (const auto &tag : dom->levels[next].requiredFacilityTags)
                        if (EconomyEngine::countTagged(ctx, tag, {}, 0) == 0) {
                            const QString fid = cheapestWithTag(ctx, tag);
                            if (!fid.isEmpty()) prereq(fid, QStringLiteral("%1 level %2 ('%3') requires an operating '%4' facility.").arg(dom->name).arg(next).arg(dom->levels[next].name, tag));
                        }
            }
        }
    } else if (!d.techRequired.isEmpty()) add("Technology maturity", CheckLevel::Ok, "Required technology maturity is available.");

    // ---- supporting industries ----
    QStringList supWhy;
    const double supF = EconomyEngine::supportFactor(ctx, d, at, &supWhy);
    if (!supWhy.isEmpty()) {
        add("Supporting industries", CheckLevel::Warning, supWhy.join("; "));
        r.reasons << "Missing supporting industries / suppliers";
        for (const auto &s : d.supporting)
            if (EconomyEngine::countTagged(ctx, s.tag, at, s.radiusKm) < s.count) {
                const QString fid = cheapestWithTag(ctx, s.tag);
                if (!fid.isEmpty()) prereq(fid, s.why);
            }
    } else if (!d.supporting.isEmpty()) add("Supporting industries", CheckLevel::Ok, "Supporting facilities are present.");

    // ---- transport access ----
    double transport = 1.0;
    if (!d.isLine()) {
        PlacedFacility tmp;
        tmp.location = at;
        const double roadKm = NetworkEngine::distanceToNetwork(ctx, at, "road");
        tmp.roadAccess = roadKm <= ctx.p("transport.accessRadiusKm");
        tmp.fibreAccess = NetworkEngine::distanceToNetwork(ctx, at, "fibre") <= ctx.p("transport.fibreRadiusKm");
        for (const auto &f : w.facilities) {
            const auto *fd = ctx.def(f);
            if (!fd || !f.isOperating() || fd->isLine()) continue;
            const double km = geo::haversineKm(f.location, at);
            if (fd->nodeOf.contains("rail") && km <= ctx.p("transport.railNodeRadiusKm")) tmp.railAccess = true;
            if (fd->tags.contains("seaport") && km <= ctx.p("transport.portRadiusKm")) tmp.portAccess = true;
            if (fd->tags.contains("airport") && km <= ctx.p("transport.airRadiusKm")) tmp.airAccess = true;
            if (fd->tags.contains("fibre_node") && km <= ctx.p("transport.fibreRadiusKm")) tmp.fibreAccess = true;
        }
        QStringList missing;
        transport = EconomyEngine::transportScore(d, tmp, &missing);
        if (!missing.isEmpty()) {
            add("Transport & connectivity", CheckLevel::Problem, "Missing access: " + missing.join(", ") + (tmp.roadAccess ? QString() : QStringLiteral(" (nearest road %1 km)").arg(roadKm > 1e6 ? 9999 : roadKm, 0, 'f', 0)) + ". Disconnected facilities produce less.");
            r.reasons << "Not connected to required transport networks";
            for (const auto &m : missing) {
                if (m == "road") prereq("national_highway", "Road access is needed to move workers, inputs and products.");
                if (m == "rail") { prereq("railway_track", "Bulk inputs (coal, ore) and freight need rail."); prereq("rail_junction", "A rail freight terminal connects the facility to the rail network."); }
                if (m == "port" || m == "sea") prereq(geo.distanceToCoastKm(at) < 50 ? "seaport" : "dry_port", "Export/import access through a port.");
                if (m == "airport") prereq("domestic_airport", "High-value electronics and staff rely on air connectivity.");
                if (m == "fibre") { prereq("fibre_backbone", "High-capacity data connectivity."); prereq("telecom_node", "A fibre node near the site."); }
            }
        } else if (!d.transportNeeds.isEmpty()) add("Transport & connectivity", CheckLevel::Ok, "Required transport access is available.");
    } else {
        // line facilities: river crossings need bridges
        int crossings = 0;
        for (int i = 1; i < path.size(); ++i) crossings += geo.riverCrossings(path[i - 1], path[i]).size();
        if (crossings > 0 && (d.network == "road" || d.network == "rail" || d.network == "metro")) {
            add("River crossings", CheckLevel::Warning, QStringLiteral("Crosses %1 mapped river(s). Place a Major bridge at each crossing, otherwise the line is not connected.").arg(crossings));
            prereq("bridge", "Each river crossing needs a bridge for the line to function.");
        }
        if (d.network == "road" || d.network == "rail") {
            const double near = std::min(NetworkEngine::distanceToNetwork(ctx, path.first(), d.network), NetworkEngine::distanceToNetwork(ctx, path.last(), d.network));
            if (w.facilities.size() > 0 && near > ctx.p("transport.snapKm") * 2 && near < 1e17)
                add("Network connection", CheckLevel::Warning, QStringLiteral("Neither end connects to the existing %1 network (nearest %2 km). Isolated segments give fewer benefits.").arg(d.network).arg(near, 0, 'f', 0));
        }
        if (d.network == "rail") add("Network nodes", CheckLevel::Ok, "Add railway stations or freight terminals along the line so facilities can use it.");
    }

    // ---- raw materials ----
    double inputs = 1.0;
    QStringList scarce;
    for (const auto &in : d.inputs) {
        if (in.qtyPerYear <= 0) continue;
        const auto *g = ctx.cat.good(in.good);
        if (!g) continue;
        const double dom = w.goodsSupply.value(in.good), dem = w.goodsDemand.value(in.good) + in.qtyPerYear * scale;
        const double domShare = dem > 0 ? std::min(1.0, dom / dem) : 0;
        const double avail = std::min(1.0, domShare + (1 - domShare) * g->importability * ctx.mods.mul("import.share"));
        inputs = std::min(inputs, avail);
        if (domShare < 0.5) {
            scarce << QStringLiteral("%1 (%2% domestic, import availability %3%)").arg(g->name).arg(domShare * 100, 0, 'f', 0).arg(g->importability * 100, 0, 'f', 0);
            if (g->importability < 0.8) {
                const auto producers = ctx.cat.producersOf(in.good);
                if (!producers.isEmpty()) prereq(producers.first()->id, QStringLiteral("%1 has limited import availability (%2). %3").arg(g->name).arg(g->importability * 100, 0, 'f', 0).arg(g->note));
            }
        }
    }
    if (!scarce.isEmpty()) add("Raw materials & components", inputs < 0.6 ? CheckLevel::Problem : CheckLevel::Warning, "Import-dependent inputs: " + scarce.join("; ") + ".");
    else if (!d.inputs.isEmpty()) add("Raw materials & components", CheckLevel::Ok, "Inputs are available domestically.");
    if (inputs < 0.6) r.reasons << "Critical inputs are scarce";

    // ---- land ----
    const double landShare = (d.landHa * scale) / std::max(1.0, dist.areaKm2 * 100.0);
    if (landShare > 0.05) add("Land", CheckLevel::Warning, QStringLiteral("Needs %1 ha, %2% of the district's area; expect acquisition and displacement issues.").arg(d.landHa * scale, 0, 'f', 0).arg(landShare * 100, 0, 'f', 1));
    else if (d.landHa > 0) add("Land", CheckLevel::Ok, QStringLiteral("Needs %1 ha (%2% of district area).").arg(d.landHa * scale, 0, 'f', 0).arg(landShare * 100, 0, 'f', 2));

    // ---- environment (rough local box-model increment) ----
    const double pmEq = d.emPM25 * scale + 0.3 * d.emSO2 * scale * 0.02 / 0.04 + 0.15 * d.emNOx * scale * 0.05 / 0.09;
    if (pmEq > 0) {
        // C = E / (u * H * W): annual-mean wind 2 m/s, mixing height 1,000 m, crosswind width 28 km (assumption)
        r.localPm25IncrementUg = pmEq * 1e12 / 3.156e7 / (2.0 * 1000.0 * 28000.0);
        const double newPm = ds.pm25 + r.localPm25IncrementUg;
        const auto now = ctx.env.initialised() ? CpcbAqi::categoryFor(ds.aqi) : QString("unknown");
        add("Air quality", r.localPm25IncrementUg > 10 ? CheckLevel::Problem : (r.localPm25IncrementUg > 2 ? CheckLevel::Warning : CheckLevel::Ok),
            QStringLiteral("Estimated local PM2.5 increase ≈%1 µg/m³ (district now %2 µg/m³, AQI category '%3' → about %4 µg/m³). Rough screening estimate.")
                .arg(r.localPm25IncrementUg, 0, 'f', 1).arg(ds.pm25, 0, 'f', 0).arg(now).arg(newPm, 0, 'f', 0));
    }
    if (d.emCO2 > 0) add("Climate", d.emCO2 * scale > 1e6 ? CheckLevel::Warning : CheckLevel::Ok, QStringLiteral("CO₂: ≈%1 Mt per year at full output.").arg(d.emCO2 * scale / 1e6, 0, 'f', 2));

    // ---- budget ----
    if (r.publicCostCr > w.budget.treasuryCr)
        add("Budget", CheckLevel::Warning, QStringLiteral("Public cost %1 exceeds the treasury balance %2; the difference will be borrowed.").arg(cr(r.publicCostCr), cr(w.budget.treasuryCr)));
    else add("Budget", CheckLevel::Ok, QStringLiteral("Public cost %1 (treasury %2).").arg(cr(r.publicCostCr), cr(w.budget.treasuryCr)));
    if (r.capexCr > r.publicCostCr + 1)
        add("Private capital", CheckLevel::Ok, QStringLiteral("%1 financed privately; progress depends on private capital availability (%2/yr).").arg(cr(r.capexCr - r.publicCostCr), cr(w.budget.privateCapitalAvailableCrYr)));

    // ---- overall ----
    r.estimatedEfficiency = std::clamp(std::min({staffing, inputs, powerFactor}) * std::sqrt(transport) * techF * supF, 0.0, 1.0);
    if (blocker) r.status = SuitabilityStatus::NotFeasible;
    else if (r.estimatedEfficiency < 0.35 || techF < 0.5) r.status = SuitabilityStatus::NotRecommended;
    else if (r.estimatedEfficiency < 0.75) r.status = SuitabilityStatus::FeasibleWithGaps;
    else r.status = SuitabilityStatus::Recommended;
    r.statusText = statusName(r.status);
    r.reasons.removeDuplicates();
    // regional suitability summary
    QStringList reg;
    reg << QStringLiteral("%1 district: population ≈%2 lakh, %3% urban, unemployment %4%.").arg(dist.name).arg(ds.population / 1e5, 0, 'f', 1)
               .arg(ds.population > 0 ? ds.urbanPopulation / ds.population * 100 : 0, 0, 'f', 0).arg(ds.labourForce > 0 ? ds.unemployed / ds.labourForce * 100 : 0, 0, 'f', 1);
    if (ds.roadAccess) reg << QStringLiteral("Road network present (%1 km).").arg(ds.roadKm, 0, 'f', 0);
    r.regionalSuitability = reg.join(' ');
    return r;
}

} // namespace isim
