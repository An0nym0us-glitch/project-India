#include "NetworkEngine.h"
#include "catalog/Catalog.h"
#include "gis/GeoDatabase.h"
#include "gis/GeoMath.h"
#include <numeric>

namespace isim {

const FacilityDefinition *SimContext::def(const PlacedFacility &f) const { return cat.facility(f.defId); }

namespace {
struct UnionFind {
    QVector<int> p;
    explicit UnionFind(int n) : p(n) { std::iota(p.begin(), p.end(), 0); }
    int find(int x) { while (p[x] != x) { p[x] = p[p[x]]; x = p[x]; } return x; }
    void unite(int a, int b) { a = find(a); b = find(b); if (a != b) p[a] = b; }
};

double pointToLineKm(const QPointF &pt, const QVector<QPointF> &line)
{
    return geo::pointPolylineKm(pt, line);
}

bool linesTouch(const QVector<QPointF> &a, const QVector<QPointF> &b, double snapKm)
{
    for (const auto &v : a) if (geo::pointPolylineKm(v, b) <= snapKm) return true;
    for (const auto &v : b) if (geo::pointPolylineKm(v, a) <= snapKm) return true;
    for (int i = 1; i < a.size(); ++i)
        for (int j = 1; j < b.size(); ++j)
            if (geo::segmentsIntersect(a[i - 1], a[i], b[j - 1], b[j])) return true;
    return false;
}

QRectF bboxOf(const QVector<QPointF> &l, double padDeg)
{
    QRectF r;
    for (const auto &p : l) r = r.united(QRectF(p, QSizeF(1e-9, 1e-9)));
    return r.adjusted(-padDeg, -padDeg, padDeg, padDeg);
}
} // namespace

double NetworkEngine::distanceToNetwork(const SimContext &ctx, const QPointF &ll, const QString &network, int *lineIndex)
{
    double best = 1e18;
    for (int i = 0; i < ctx.w.facilities.size(); ++i) {
        const auto &f = ctx.w.facilities[i];
        if (!f.isOperating() || f.networkBroken) continue;
        const auto *d = ctx.def(f);
        if (!d || !d->isLine() || d->network != network) continue;
        const double km = pointToLineKm(ll, f.path);
        if (km < best) { best = km; if (lineIndex) *lineIndex = i; }
    }
    return best;
}

void NetworkEngine::rebuild(SimContext &ctx)
{
    auto &w = ctx.w;
    auto &net = ctx.net;
    const int n = w.facilities.size();
    const double snap = ctx.p("transport.snapKm");
    const double accessR = ctx.p("transport.accessRadiusKm");
    const double railR = ctx.p("transport.railNodeRadiusKm");
    const double portR = ctx.p("transport.portRadiusKm");
    const double airR = ctx.p("transport.airRadiusKm");
    const double fibreR = ctx.p("transport.fibreRadiusKm");
    const double bridgeR = ctx.p("transport.bridgeRadiusKm");
    const double powerAttach = ctx.p("transport.powerAttachKm");
    const double pipeAttach = ctx.p("transport.pipelineAttachKm");

    QVector<const FacilityDefinition *> defs(n, nullptr);
    for (int i = 0; i < n; ++i) defs[i] = ctx.def(w.facilities[i]);

    // ---- 1. river crossings for road/rail lines ----
    QVector<QPointF> bridges;
    for (int i = 0; i < n; ++i)
        if (defs[i] && w.facilities[i].isOperating() && defs[i]->tags.contains("bridge")) bridges << w.facilities[i].location;
    net.unbridgedCrossings.clear();
    for (int i = 0; i < n; ++i) {
        auto &f = w.facilities[i];
        f.networkBroken = false;
        if (!defs[i] || !defs[i]->isLine() || (defs[i]->network != "road" && defs[i]->network != "rail" && defs[i]->network != "metro")) continue;
        if (!net.crossingCache.contains(f.uid)) {
            QVector<QPointF> pts;
            for (int k = 1; k < f.path.size(); ++k)
                for (const auto &c : ctx.geo.riverCrossings(f.path[k - 1], f.path[k])) pts << c.second;
            net.crossingCache.insert(f.uid, pts);
        }
        for (const auto &c : net.crossingCache.value(f.uid)) {
            bool ok = false;
            for (const auto &b : bridges) if (geo::haversineKm(b, c) <= bridgeR) { ok = true; break; }
            if (!ok) { f.networkBroken = true; net.unbridgedCrossings.append({f.uid, c}); }
        }
    }

    // ---- 2. transport union-find (lines + node facilities) ----
    static const QStringList transportNets = {"road", "rail", "waterway", "metro"};
    UnionFind uf(n);
    QVector<int> lines;
    for (int i = 0; i < n; ++i)
        if (defs[i] && defs[i]->isLine() && w.facilities[i].isOperating() && !w.facilities[i].networkBroken && transportNets.contains(defs[i]->network)) lines << i;
    QVector<QRectF> lineBox(n);
    for (int i : lines) lineBox[i] = bboxOf(w.facilities[i].path, 0.1);
    for (int a = 0; a < lines.size(); ++a)
        for (int b = a + 1; b < lines.size(); ++b) {
            const int i = lines[a], j = lines[b];
            if (defs[i]->network != defs[j]->network) continue; // modes join only at nodes
            if (!lineBox[i].intersects(lineBox[j])) continue;
            if (linesTouch(w.facilities[i].path, w.facilities[j].path, snap)) uf.unite(i, j);
        }
    // node facilities (stations, ports, hubs) join every network they are a node of
    auto nearestLine = [&](const QPointF &pt, const QString &network, double radius) {
        int best = -1; double bd = radius;
        for (int i : lines) {
            if (defs[i]->network != network) continue;
            if (!lineBox[i].adjusted(-1, -1, 1, 1).contains(pt)) continue;
            const double km = pointToLineKm(pt, w.facilities[i].path);
            if (km <= bd) { bd = km; best = i; }
        }
        return best;
    };
    for (int i = 0; i < n; ++i) {
        auto &f = w.facilities[i];
        if (!defs[i] || defs[i]->isLine() || !f.isOperating()) continue;
        for (const auto &nw : defs[i]->nodeOf) {
            const double r = nw == "road" ? accessR : 5.0;
            const int li = nearestLine(f.location, nw, r);
            if (li >= 0) uf.unite(i, li);
        }
    }
    // component ids
    QHash<int, int> compId;
    auto comp = [&](int i) { const int r = uf.find(i); if (!compId.contains(r)) compId.insert(r, compId.size()); return compId.value(r); };

    // collect gateway/node facilities
    struct Node { int idx; QPointF at; };
    QVector<Node> railNodes, ports, airports, fibreNodes;
    for (int i = 0; i < n; ++i) {
        const auto &f = w.facilities[i];
        if (!defs[i] || !f.isOperating() || defs[i]->isLine()) continue;
        if (defs[i]->nodeOf.contains("rail") && nearestLine(f.location, "rail", 5.0) >= 0) railNodes.append({i, f.location});
        if (defs[i]->tags.contains("seaport")) ports.append({i, f.location});
        if (defs[i]->tags.contains("airport")) airports.append({i, f.location});
        if (defs[i]->tags.contains("fibre_node")) fibreNodes.append({i, f.location});
    }
    net.componentGateways.clear();
    for (const auto &p : ports) net.componentGateways[comp(p.idx)].insert(w.facilities[p.idx].uid);
    for (const auto &a : airports) net.componentGateways[comp(a.idx)].insert(w.facilities[a.idx].uid);

    // ---- 3. per-facility access flags ----
    for (int i = 0; i < n; ++i) {
        auto &f = w.facilities[i];
        f.roadAccess = f.railAccess = f.portAccess = f.airAccess = f.fibreAccess = false;
        f.component = -1;
        if (!defs[i]) continue;
        if (defs[i]->isLine()) {
            f.roadAccess = f.railAccess = f.portAccess = f.airAccess = f.fibreAccess = true;
            if (transportNets.contains(defs[i]->network) && f.isOperating() && !f.networkBroken) f.component = comp(i);
            continue;
        }
        const int roadLine = nearestLine(f.location, "road", accessR);
        f.roadAccess = roadLine >= 0;
        if (f.roadAccess) f.component = comp(roadLine);
        else if (defs[i]->nodeOf.size() && f.isOperating()) f.component = comp(i);
        for (const auto &rn : railNodes)
            if (rn.idx == i || geo::haversineKm(rn.at, f.location) <= railR) { f.railAccess = true; break; }
        for (const auto &p : ports)
            if (p.idx == i || geo::haversineKm(p.at, f.location) <= portR || (f.component >= 0 && comp(p.idx) == f.component)) { f.portAccess = true; break; }
        for (const auto &a : airports)
            if (a.idx == i || geo::haversineKm(a.at, f.location) <= airR) { f.airAccess = true; break; }
        f.fibreAccess = distanceToNetwork(ctx, f.location, "fibre") <= fibreR;
        if (!f.fibreAccess)
            for (const auto &fn : fibreNodes) if (geo::haversineKm(fn.at, f.location) <= fibreR) { f.fibreAccess = true; break; }
    }
    net.transportComponents = compId.size();

    // ---- 4. district transport indicators ----
    const int nd = w.districts.size();
    for (auto &d : w.districts) { d.roadKm = d.railKm = 0; d.roadAccess = d.railAccess = false; }
    for (int i : lines) {
        const auto &f = w.facilities[i];
        if (f.districts.isEmpty()) continue;
        const double share = f.lengthKm / f.districts.size();
        for (int di : f.districts) {
            if (di < 0 || di >= nd) continue;
            if (defs[i]->network == "road") { w.districts[di].roadKm += share; w.districts[di].roadAccess = true; }
            if (defs[i]->network == "rail") { w.districts[di].railKm += share; w.districts[di].railAccess = true; }
        }
    }
    const double fullDensity = ctx.p("transport.fullAccessRoadDensity");
    for (int di = 0; di < nd; ++di) {
        auto &d = w.districts[di];
        const double area = std::max(1.0, ctx.geo.districts()[di].areaKm2);
        d.accessIndex = std::min(1.0, d.roadKm / (fullDensity * area)) * (d.roadAccess ? 1.0 : 0.0);
        if (d.railAccess) d.accessIndex = std::min(1.0, d.accessIndex + 0.15);
    }

    // ---- 5. power islands ----
    UnionFind pu(n);
    QVector<int> powerLines, powerNodes;
    for (int i = 0; i < n; ++i) {
        if (!defs[i] || !w.facilities[i].isOperating()) continue;
        if (defs[i]->isLine() && defs[i]->network == "power") powerLines << i;
        else if (!defs[i]->isLine() && (defs[i]->generationMW > 0 || defs[i]->storageMWh > 0 || defs[i]->nodeOf.contains("power"))) powerNodes << i;
    }
    for (int a = 0; a < powerLines.size(); ++a) {
        const int i = powerLines[a];
        lineBox[i] = bboxOf(w.facilities[i].path, 0.2);
        for (int b = 0; b < a; ++b) {
            const int j = powerLines[b];
            if (lineBox[i].intersects(lineBox[j]) && linesTouch(w.facilities[i].path, w.facilities[j].path, snap)) pu.unite(i, j);
        }
    }
    for (int i : powerNodes) {
        for (int li : powerLines)
            if (lineBox[li].adjusted(-0.3, -0.3, 0.3, 0.3).contains(w.facilities[i].location) &&
                pointToLineKm(w.facilities[i].location, w.facilities[li].path) <= powerAttach) pu.unite(i, li);
        for (int j : powerNodes) // same-district local bus
            if (j < i && w.facilities[j].district == w.facilities[i].district && w.facilities[i].district >= 0) pu.unite(i, j);
    }
    net.powerIslandOfDistrict.fill(-1, nd);
    QHash<int, int> islandId;
    for (int i = 0; i < n; ++i) w.facilities[i].gridIsland = -1;
    for (int i : powerNodes) {
        const int r = pu.find(i);
        if (!islandId.contains(r)) islandId.insert(r, islandId.size());
        w.facilities[i].gridIsland = islandId.value(r);
        if (defs[i]->distributionMW > 0 && w.facilities[i].district >= 0)
            net.powerIslandOfDistrict[w.facilities[i].district] = islandId.value(r);
    }
    for (int li : powerLines) {
        const int r = pu.find(li);
        if (islandId.contains(r)) w.facilities[li].gridIsland = islandId.value(r);
    }
    net.powerIslands = islandId.size();
    // consumer facilities belong to the island serving their district
    for (int i = 0; i < n; ++i)
        if (w.facilities[i].gridIsland < 0 && w.facilities[i].district >= 0)
            w.facilities[i].gridIsland = net.powerIslandOfDistrict[w.facilities[i].district];

    // ---- 6. water networks (union over districts) ----
    UnionFind wu(nd);
    QVector<bool> hasSupply(nd, false);
    QVector<int> pipes;
    for (int i = 0; i < n; ++i) {
        if (!defs[i] || !w.facilities[i].isOperating()) continue;
        if (defs[i]->waterSupplyMLD > 0 && w.facilities[i].district >= 0) hasSupply[w.facilities[i].district] = true;
        if (defs[i]->isLine() && defs[i]->network == "water") pipes << i;
    }
    for (int pi : pipes) {
        const auto &f = w.facilities[pi];
        // a pipeline joins all districts it passes through; it must start near a supply
        QVector<int> ds = f.districts;
        for (int i = 0; i < n; ++i)
            if (defs[i] && defs[i]->waterSupplyMLD > 0 && w.facilities[i].isOperating() && w.facilities[i].district >= 0 &&
                pointToLineKm(w.facilities[i].location, f.path) <= pipeAttach) ds << w.facilities[i].district;
        for (int k = 1; k < ds.size(); ++k) if (ds[k] >= 0 && ds[0] >= 0) wu.unite(ds[0], ds[k]);
    }
    QHash<int, bool> rootHasSupply;
    for (int di = 0; di < nd; ++di) if (hasSupply[di]) rootHasSupply[wu.find(di)] = true;
    net.waterNetOfDistrict.fill(-1, nd);
    QHash<int, int> wid;
    for (int di = 0; di < nd; ++di) {
        const int r = wu.find(di);
        if (!rootHasSupply.value(r)) continue;
        if (!wid.contains(r)) wid.insert(r, wid.size());
        net.waterNetOfDistrict[di] = wid.value(r);
    }
    net.waterNets = wid.size();
    net.dirty = false;
}

} // namespace isim
