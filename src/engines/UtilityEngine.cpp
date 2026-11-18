#include "UtilityEngine.h"
#include "catalog/Catalog.h"
#include "gis/GeoDatabase.h"
#include <algorithm>

namespace isim {

double UtilityEngine::seasonalFactor(const FacilityDefinition &d, int m)
{
    // Simplified monsoon seasonality (assumption): factors average ≈1 over the year.
    const bool monsoon = m >= 5 && m <= 8;          // Jun-Sep
    const bool winter = m == 11 || m <= 1;          // Dec-Feb
    if (d.tags.contains("solar")) return monsoon ? 0.8 : (winter ? 0.95 : 1.12);
    if (d.tags.contains("wind")) return monsoon ? 1.6 : 0.8;
    if (d.tags.contains("hydro")) return monsoon ? 1.6 : 0.7;
    return 1.0;
}

void UtilityEngine::step(SimContext &ctx)
{
    auto &w = ctx.w;
    const int nd = w.districts.size();
    const double kwhCap = ctx.p("power.residentialKwhPerCapita");
    const double kwhUrban = ctx.p("power.commercialKwhPerCapitaUrban");
    const double kwhIrr = ctx.p("power.agriKwhPerHaIrrigated");
    const double demandMul = ctx.mods.mul("power.demand");
    const int moy = w.monthOfYear();

    // ---- district demand ----
    for (int di = 0; di < nd; ++di) {
        auto &d = w.districts[di];
        d.powerDemandMW = (d.population * kwhCap + d.urbanPopulation * kwhUrban + d.irrigationHa * kwhIrr) / 8760.0 / 1000.0 * demandMul;
        d.distributionMW = 0;
        const double ruralPop = std::max(0.0, d.population - d.urbanPopulation);
        d.waterDemandMLD = (d.urbanPopulation * ctx.p("water.lpcdUrban") + ruralPop * ctx.p("water.lpcdRural")) / 1e6;
        d.wasteTPD = (d.urbanPopulation * ctx.p("waste.urbanKgPerCapitaDay") + ruralPop * ctx.p("waste.ruralKgPerCapitaDay")) / 1000.0;
        d.waterSuppliedMLD = d.sewageTreatedMLD = d.wasteProcessedTPD = 0;
    }
    QVector<double> stpCap(nd, 0.0), wasteCap(nd, 0.0);
    for (const auto &f : w.facilities) {
        if (!f.isOperating() || f.district < 0) continue;
        const auto *d = ctx.def(f);
        if (!d) continue;
        if (d->powerDemandMW > 0) w.districts[f.district].powerDemandMW += d->powerDemandMW * (d->isLine() ? f.scale : 1.0) * demandMul;
        if (d->waterDemandMLD > 0) w.districts[f.district].waterDemandMLD += d->waterDemandMLD * (d->isLine() ? f.scale : 1.0);
        if (d->distributionMW > 0) w.districts[f.district].distributionMW += d->distributionMW * std::max(0.3, f.staffing);
        if (d->sewageTreatMLD > 0) stpCap[f.district] += d->sewageTreatMLD * f.efficiency;
        if (d->wasteTPD > 0) wasteCap[f.district] += d->wasteTPD * f.efficiency;
    }

    // ---- power islands ----
    const int ni = ctx.net.powerIslands;
    m_islands.fill(PowerIslandBalance{}, ni);
    double genTotalMW = 0, renewableMW = 0;
    for (const auto &f : w.facilities) {
        if (!f.isOperating() || f.gridIsland < 0 || f.gridIsland >= ni) continue;
        const auto *d = ctx.def(f);
        if (!d) continue;
        auto &is = m_islands[f.gridIsland];
        if (d->generationMW > 0) {
            // plant availability uses its pre-grid efficiency (staffing, fuel, technology)
            const double out = d->generationMW * d->capacityFactor * seasonalFactor(*d, moy) * f.efficiency;
            is.supplyMW += out;
            is.capacityMW += d->generationMW;
            if (d->tags.contains("variable")) is.variableMW += out;
            genTotalMW += out;
            if (d->tags.contains("renewable") || d->tags.contains("low_carbon")) renewableMW += out;
        }
        if (d->storageMWh > 0) is.firmStorageMW += d->storageMWh * f.efficiency / ctx.p("power.storageFirmHours");
    }
    for (int di = 0; di < nd; ++di) {
        const int isl = ctx.net.powerIslandOfDistrict.value(di, -1);
        if (isl >= 0 && isl < ni) m_islands[isl].demandMW += w.districts[di].powerDemandMW;
    }
    const double loss = ctx.p("power.transmissionLoss");
    const double penalty = ctx.p("power.variabilityPenalty");
    for (auto &is : m_islands) {
        is.servedFraction = is.demandMW > 0 ? std::min(1.0, is.supplyMW * (1 - loss) / is.demandMW) : (is.supplyMW > 0 ? 1.0 : 0.0);
        const double varShare = is.supplyMW > 0 ? is.variableMW / is.supplyMW : 0;
        const double storageCover = is.variableMW > 0 ? std::min(1.0, is.firmStorageMW / is.variableMW) : 1.0;
        is.reliability = std::clamp(is.servedFraction * (1 - penalty * std::max(0.0, varShare - 0.3) * (1 - storageCover)), 0.0, 1.0);
    }
    double served = 0, demand = 0;
    for (int di = 0; di < nd; ++di) {
        auto &d = w.districts[di];
        const int isl = ctx.net.powerIslandOfDistrict.value(di, -1);
        d.powerServedMW = 0;
        if (isl >= 0 && isl < ni)
            d.powerServedMW = std::min(d.powerDemandMW * m_islands[isl].servedFraction, d.distributionMW);
        served += d.powerServedMW;
        demand += d.powerDemandMW;
    }
    // facility power supply factor
    for (auto &f : w.facilities) {
        const auto *d = ctx.def(f);
        if (!d) continue;
        if (!d->needsGrid || d->powerDemandMW <= 0) { f.powerSupply = 1.0; continue; }
        const auto &dist = w.districts[f.district];
        const int isl = ctx.net.powerIslandOfDistrict.value(f.district, -1);
        if (isl < 0 || isl >= ni || dist.powerDemandMW <= 0) { f.powerSupply = 0.0; continue; }
        const double rel = m_islands[isl].reliability;
        double ps = std::min(1.0, dist.powerServedMW / dist.powerDemandMW) * (0.5 + 0.5 * rel);
        if (d->minGridReliability > 0 && rel < d->minGridReliability) ps *= std::max(0.1, rel / d->minGridReliability) * 0.6;
        f.powerSupply = std::clamp(ps, 0.0, 1.0);
    }
    auto &s = w.stats;
    s.powerDemandMW = demand;
    s.powerServedMW = served;
    s.generationTWhYr = std::min(genTotalMW, demand / std::max(0.01, 1 - loss)) * 8.76 / 1000.0;
    s.renewableShareGen = genTotalMW > 0 ? renewableMW / genTotalMW : 0;
    double rsum = 0, rw = 0;
    for (const auto &is : m_islands) { rsum += is.reliability * is.demandMW; rw += is.demandMW; }
    s.gridReliability = rw > 0 ? rsum / rw : 0;
    double cap = 0, rcap = 0;
    for (const auto &f : w.facilities) {
        const auto *d = ctx.def(f);
        if (!d || !f.isOperating() || d->generationMW <= 0) continue;
        cap += d->generationMW;
        if (d->tags.contains("renewable")) rcap += d->generationMW;
    }
    s.generationCapacityMW = cap;
    s.renewableCapacityMW = rcap;

    // ---- water ----
    QVector<double> netSupply(ctx.net.waterNets, 0.0), netDemand(ctx.net.waterNets, 0.0);
    for (const auto &f : w.facilities) {
        const auto *d = ctx.def(f);
        if (!d || !f.isOperating() || d->waterSupplyMLD <= 0 || f.district < 0) continue;
        const int wn = ctx.net.waterNetOfDistrict.value(f.district, -1);
        if (wn >= 0) netSupply[wn] += d->waterSupplyMLD * f.efficiency;
    }
    for (int di = 0; di < nd; ++di) {
        const int wn = ctx.net.waterNetOfDistrict.value(di, -1);
        if (wn >= 0) netDemand[wn] += w.districts[di].waterDemandMLD;
    }
    double wd = 0, ws = 0, sew = 0, sewT = 0, waste = 0, wasteP = 0;
    const double selfSupply = ctx.p("water.industrialSelfSupply");
    QVector<double> waterFrac(nd, 0.0);
    for (int di = 0; di < nd; ++di) {
        auto &d = w.districts[di];
        const int wn = ctx.net.waterNetOfDistrict.value(di, -1);
        const double frac = wn >= 0 && netDemand[wn] > 0 ? std::min(1.0, netSupply[wn] / netDemand[wn]) : 0.0;
        waterFrac[di] = frac;
        d.waterSuppliedMLD = d.waterDemandMLD * frac;
        d.sewageMLD = d.waterSuppliedMLD * ctx.p("water.sewageFraction");
        d.sewageTreatedMLD = std::min(d.sewageMLD, stpCap[di]);
        d.wasteProcessedTPD = std::min(d.wasteTPD, wasteCap[di]);
        wd += d.waterDemandMLD; ws += d.waterSuppliedMLD; sew += d.sewageMLD; sewT += d.sewageTreatedMLD;
        waste += d.wasteTPD; wasteP += d.wasteProcessedTPD;
    }
    for (auto &f : w.facilities) {
        const auto *d = ctx.def(f);
        if (!d || !d->needsWater || f.district < 0) { f.waterSupply = 1.0; continue; }
        const double fr = waterFrac[f.district];
        f.waterSupply = fr + (1 - fr) * selfSupply;
    }
    s.waterDemandMLD = wd; s.waterSuppliedMLD = ws;
    s.sewageTreatedShare = sew > 0 ? sewT / sew : 0;
    s.wasteProcessedShare = waste > 0 ? wasteP / waste : 0;
}

} // namespace isim
