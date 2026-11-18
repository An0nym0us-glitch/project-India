#include "EnvironmentEngine.h"
#include "catalog/Catalog.h"
#include "core/DataPaths.h"
#include "gis/GeoDatabase.h"
#include "gis/GeoMath.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace isim {

EnvironmentEngine::Climate EnvironmentEngine::climate(int m)
{
    // Highly simplified climatology (assumption): prevailing transport wind (m/s, u = east, v = north),
    // daytime-average mixing height (m) and wet-removal rate (per hour).
    static const double mix[12] = {700, 800, 1100, 1500, 1700, 1300, 1100, 1100, 1100, 900, 700, 600};
    if (m >= 5 && m <= 8) return {2.5, 2.0, mix[m], 0.03};        // SW monsoon
    if (m >= 2 && m <= 4) return {2.0, -0.3, mix[m], 0.002};      // pre-monsoon westerlies
    if (m == 9 || m == 10) return {1.0, -1.0, mix[m], 0.002};     // post-monsoon north-westerlies
    return {1.0, -0.8, mix[m], 0.001};                           // winter: weak NW flow, shallow mixing
}

void EnvironmentEngine::initialise(SimContext &ctx)
{
    auto &env = ctx.env;
    env.cell = ctx.p("env.gridCellDeg") > 0 ? ctx.p("env.gridCellDeg") : 0.25;
    env.lon0 = 66.0; env.lat0 = 5.0;
    env.nx = int(std::ceil((99.0 - env.lon0) / env.cell));
    env.ny = int(std::ceil((38.5 - env.lat0) / env.cell));
    const int n = env.nx * env.ny;
    env.cellDistrict.fill(-1, n);
    m_desert.fill(false, n);
    for (auto *v : {&env.pm25, &env.pm10, &env.no2, &env.so2, &env.co, &env.o3, &env.emPm25, &env.emPm10, &env.emNOx, &env.emSO2, &env.emCO, &env.emVOC})
        v->fill(0.f, n);
    m_districtCells.fill({}, ctx.geo.districts().size());
    for (int y = 0; y < env.ny; ++y)
        for (int x = 0; x < env.nx; ++x) {
            const QPointF c = env.cellCentre(x, y);
            const int d = ctx.geo.districtAt(c);
            env.cellDistrict[env.idx(x, y)] = d;
            if (d >= 0) m_districtCells[d] << env.idx(x, y);
            m_desert[env.idx(x, y)] = ctx.geo.isDesert(c);
        }
    for (int d = 0; d < m_districtCells.size(); ++d)
        if (m_districtCells[d].isEmpty()) {
            int x, y;
            if (env.cellOf(ctx.geo.districts()[d].centroid, x, y)) m_districtCells[d] << env.idx(x, y);
        }
    m_aqi.load(dataFile("aqi/cpcb_breakpoints.json"));
}

void EnvironmentEngine::facilityEmissions(const SimContext &ctx, const PlacedFacility &f, double out[7])
{
    for (int i = 0; i < 7; ++i) out[i] = 0;
    const auto *d = ctx.def(f);
    if (!d || !f.isOperating()) return;
    double activity = f.efficiency;
    double mul = ctx.mods.mul("emissions.mult.all");
    if (d->tags.contains("power_plant")) mul *= ctx.mods.mul("emissions.mult.power");
    if (d->tags.contains("coal_power")) mul *= ctx.mods.mul("emissions.mult.coal");
    if (d->tags.contains("manufacturing") || d->tags.contains("mining")) mul *= ctx.mods.mul("emissions.mult.industry");
    if (d->category == "Agriculture & Food") mul *= ctx.mods.mul("emissions.mult.agriculture");
    if (d->network == "road" || d->tags.contains("airport") || d->tags.contains("seaport")) mul *= ctx.mods.mul("emissions.mult.transport");
    if (d->isLine()) {
        activity = f.networkBroken ? 0.3 : 1.0;
        if (d->network == "road" && !f.districts.isEmpty()) {
            // traffic scales with population density along the corridor (assumption)
            double dens = 0;
            for (int di : f.districts) dens += ctx.w.districts[di].population / std::max(1.0, ctx.geo.districts()[di].areaKm2);
            dens /= f.districts.size();
            activity *= std::clamp(dens / ctx.p("env.roadEmissionDensityRef"), 0.2, 3.0);
        }
        activity *= f.scale;
    }
    const double k = activity * mul;
    out[0] = d->emPM25 * k; out[1] = d->emPM10 * k; out[2] = d->emNOx * k; out[3] = d->emSO2 * k;
    out[4] = d->emCO * k; out[5] = d->emVOC * k; out[6] = d->emCO2 * k;
}

void EnvironmentEngine::accumulateEmissions(SimContext &ctx)
{
    auto &env = ctx.env;
    auto &w = ctx.w;
    for (auto *v : {&env.emPm25, &env.emPm10, &env.emNOx, &env.emSO2, &env.emCO, &env.emVOC}) v->fill(0.f);
    const int moy = w.monthOfYear();
    const double hh = ctx.mods.mul("emissions.mult.household") * ctx.mods.mul("emissions.mult.all");
    const double clean = ctx.p("env.electrificationCleanFactor");
    const bool residueSeason = moy == 9 || moy == 10;
    const double residue = ctx.p("env.residueBurningPm25KgPerCapitaRural") * ctx.mods.mul("emissions.mult.residue") * ctx.mods.mul("emissions.mult.agriculture");
    double co2 = 0;
    for (int di = 0; di < w.districts.size(); ++di) {
        const auto &d = w.districts[di];
        const auto &cells = m_districtCells[di];
        if (cells.isEmpty()) continue;
        const double elec = d.powerDemandMW > 0 ? d.powerServedMW / d.powerDemandMW : 0;
        const double f = d.population * hh * (1 - clean * elec) / 1000.0; // kg -> t
        double pm25 = ctx.p("env.residentialPm25KgPerCapita") * f;
        const double nox = ctx.p("env.residentialNOxKgPerCapita") * d.population / 1000.0 * ctx.mods.mul("emissions.mult.transport");
        const double so2 = ctx.p("env.residentialSO2KgPerCapita") * f;
        const double co = ctx.p("env.residentialCOKgPerCapita") * f;
        const double voc = ctx.p("env.residentialVOCKgPerCapita") * f;
        co2 += ctx.p("env.residentialCO2TPerCapita") * d.population * (1 - 0.5 * clean * elec);
        const QString &st = ctx.geo.districts()[di].stateName;
        if (residueSeason && (st == "Punjab" || st == "Haryana")) // burning concentrated in Oct-Nov: annual amount in 2 months
            pm25 += residue * std::max(0.0, d.population - d.urbanPopulation) / 1000.0 * 6.0;
        const float n = cells.size();
        for (int c : cells) {
            env.emPm25[c] += pm25 / n; env.emPm10[c] += pm25 * 1.3 / n; env.emNOx[c] += nox / n;
            env.emSO2[c] += so2 / n; env.emCO[c] += co / n; env.emVOC[c] += voc / n;
        }
    }
    double e[7];
    for (const auto &f : w.facilities) {
        facilityEmissions(ctx, f, e);
        co2 += e[6];
        if (e[0] + e[1] + e[2] + e[3] + e[4] + e[5] <= 0) continue;
        QVector<QPointF> pts;
        if (f.path.size() >= 2) {
            for (int i = 1; i < f.path.size(); ++i) {
                const int steps = std::max(1, int(geo::haversineKm(f.path[i - 1], f.path[i]) / 20.0));
                for (int s = 0; s < steps; ++s) pts << f.path[i - 1] + (f.path[i] - f.path[i - 1]) * ((s + 0.5) / steps);
            }
        } else pts << f.location;
        for (const auto &p : pts) {
            int x, y;
            if (!env.cellOf(p, x, y)) continue;
            const int c = env.idx(x, y);
            const float k = 1.0f / pts.size();
            env.emPm25[c] += e[0] * k; env.emPm10[c] += e[1] * k; env.emNOx[c] += e[2] * k;
            env.emSO2[c] += e[3] * k; env.emCO[c] += e[4] * k; env.emVOC[c] += e[5] * k;
        }
    }
    w.stats.co2Mt = co2 / 1e6;
    w.stats.co2PerCapita = w.stats.population > 0 ? co2 / w.stats.population : 0;
}

void EnvironmentEngine::step(SimContext &ctx)
{
    auto &env = ctx.env;
    if (!env.initialised()) initialise(ctx);
    accumulateEmissions(ctx);
    const int moy = ctx.w.monthOfYear();
    const Climate cl = climate(moy);
    const int nx = env.nx, ny = env.ny, n = nx * ny;
    const double dt = 3600.0;
    const double dy = env.cell * 110570.0;
    // per-species loss rates (per hour, assumption) and transported fields
    struct Species { QVector<float> *conc; const QVector<float> *em; double loss; double wash; };
    for (auto *v : {&env.emPm25, &env.emPm10, &env.emNOx, &env.emSO2, &env.emCO, &env.emVOC}) v->detach();
    QVector<float> nox(n), voc(n), pmPrimary(n), pm10c(n);
    for (int i = 0; i < n; ++i) { nox[i] = env.no2[i] / 0.7f; pmPrimary[i] = 0; }
    // working copies (start from previous month for faster convergence)
    QVector<float> so2 = env.so2, co = env.co, pm25 = env.pm25;
    // remove backgrounds stored in the output fields
    const double bgPm25 = ctx.p("env.backgroundPm25"), bgPm10 = ctx.p("env.backgroundPm10");
    for (int i = 0; i < n; ++i) {
        pm25[i] = std::max(0.f, pm25[i] - float(bgPm25));
        co[i] = std::max(0.f, (co[i] - 0.3f) * 1000.f);
        pm10c[i] = 0;
        voc[i] = 0;
    }
    Species sp[] = {{&pm25, &env.emPm25, 0.01, 1.0}, {&pm10c, &env.emPm10, 0.03, 1.0}, {&nox, &env.emNOx, 0.05, 0.3},
                    {&so2, &env.emSO2, 0.02, 0.7}, {&co, &env.emCO, 0.002, 0.0}, {&voc, &env.emVOC, 0.05, 0.1}};
    // warm-started from last month's fields, so a few days of model time per month suffice
    const int steps = ctx.fastMode ? 48 : 120;
    // source term: t/yr -> µg/m3 per step in a box of cell area x mixing height
    std::vector<double> srcScale(ny), cxRow(ny);
    for (int y = 0; y < ny; ++y) {
        const double lat = env.lat0 + (y + 0.5) * env.cell;
        const double dx = env.cell * 111320.0 * std::cos(geo::deg2rad(lat));
        srcScale[y] = 1e12 / 3.156e7 * dt / (dx * dy * cl.mixingHeight); // t/yr -> µg/s -> µg/m3 per step
        cxRow[y] = std::min(0.9, std::abs(cl.u) * dt / dx);
    }
    const double cy = std::min(0.9, std::abs(cl.v) * dt / dy);
    const double sulfY = ctx.p("env.secondarySulfateYield"), nitY = ctx.p("env.secondaryNitrateYield");
    std::vector<float> tmp(n);
    const int xs = cl.u >= 0 ? -1 : 1, ys = cl.v >= 0 ? -1 : 1;
    for (int step = 0; step < steps; ++step) {
        for (auto &sp1 : sp) {
            float *c = sp1.conc->data();
            const float *em = sp1.em->constData();
            const double k = (sp1.loss + cl.washout * sp1.wash) * dt / 3600.0;
            float *t = tmp.data();
            for (int y = 0; y < ny; ++y) {
                const double cx = cxRow[y], sc = srcScale[y];
                const int yu = y + ys;
                const bool yIn = yu >= 0 && yu < ny;
                const float *row = c + y * nx, *rowU = yIn ? c + yu * nx : nullptr;
                float *out = t + y * nx;
                const float *e = em + y * nx;
                for (int x = 0; x < nx; ++x) {
                    // first-order upwind advection (inflow at domain edges = 0 excess over background)
                    const int xu = x + xs;
                    const float cxu = (xu >= 0 && xu < nx) ? row[xu] : 0.f;
                    const float cyu = yIn ? rowU[x] : 0.f;
                    double v = row[x] - cx * (row[x] - cxu) - cy * (row[x] - cyu);
                    v += e[x] * sc;
                    v *= (1 - k);
                    out[x] = float(v > 0 ? v : 0);
                }
            }
            std::copy(tmp.begin(), tmp.end(), c);
        }
        // secondary inorganic aerosol from SO2 and NOx chemical loss
        float *p25 = pm25.data();
        const float *s2 = so2.constData(), *nx2 = nox.constData();
        for (int i = 0; i < n; ++i) p25[i] += float(s2[i] * 0.02 * sulfY + nx2[i] * 0.05 * nitY);
    }
    // assemble outputs with backgrounds
    const bool dustSeason = moy >= 3 && moy <= 5;
    const double desertDust = ctx.p("env.desertDustPm10");
    const double o3bg = (moy >= 2 && moy <= 4) ? 50 : (moy >= 5 && moy <= 8 ? 30 : 40);
    for (int i = 0; i < n; ++i) {
        env.pm25[i] = pm25[i] + float(bgPm25);
        const double dust = bgPm10 * (dustSeason ? 1.5 : 1.0) + (m_desert[i] ? desertDust * (dustSeason ? 1.5 : 1.0) : 0.0);
        env.pm10[i] = float(env.pm25[i] + 0.5 * pm10c[i] + dust); // coarse fraction ≈ half of primary PM10 (assumption)
        env.no2[i] = float(nox[i] * 0.7);
        env.so2[i] = so2[i];
        env.co[i] = co[i] / 1000.f + 0.3f;
        env.o3[i] = float(std::min(300.0, o3bg + 0.5 * std::sqrt(std::max(0.f, voc[i]) * std::max(0.f, nox[i]))));
    }
    // district averages and AQI
    double aqiW = 0, popW = 0;
    for (int di = 0; di < ctx.w.districts.size(); ++di) {
        auto &d = ctx.w.districts[di];
        const auto &cells = m_districtCells[di];
        if (cells.isEmpty()) continue;
        double a[6] = {0, 0, 0, 0, 0, 0};
        for (int c : cells) { a[0] += env.pm25[c]; a[1] += env.pm10[c]; a[2] += env.no2[c]; a[3] += env.so2[c]; a[4] += env.co[c]; a[5] += env.o3[c]; }
        const double k = 1.0 / cells.size();
        d.pm25 = a[0] * k; d.pm10 = a[1] * k; d.no2 = a[2] * k; d.so2 = a[3] * k; d.co = a[4] * k; d.o3 = a[5] * k;
        const auto r = m_aqi.compute({{"pm25", d.pm25}, {"pm10", d.pm10}, {"no2", d.no2}, {"so2", d.so2}, {"co", d.co}, {"o3", d.o3}});
        d.aqi = r.aqi;
        aqiW += std::max(0, r.aqi) * d.population; popW += d.population;
    }
    ctx.w.stats.aqiPopWeighted = popW > 0 ? aqiW / popW : 0;
}

} // namespace isim
