#include "EconomyEngine.h"
#include "PolicyEngine.h"
#include "catalog/Catalog.h"
#include "gis/GeoDatabase.h"
#include "gis/GeoMath.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace isim {

double EconomyEngine::techFactor(const SimContext &ctx, const FacilityDefinition &d, QStringList *why)
{
    double f = 1.0;
    for (auto it = d.techRequired.begin(); it != d.techRequired.end(); ++it) {
        const int have = ctx.w.techLevel.value(it.key(), 0);
        const int need = it.value();
        if (have >= need) continue;
        const double k = (need - have == 1) ? ctx.p("tech.missingLevelFactor") : ctx.p("tech.farLevelFactor");
        f *= k;
        if (why) {
            const auto *dom = ctx.cat.techDomain(it.key());
            *why << QStringLiteral("%1 maturity is level %2; level %3 is required").arg(dom ? dom->name : it.key()).arg(have).arg(need);
        }
    }
    return f;
}

int EconomyEngine::countTagged(const SimContext &ctx, const QString &tag, const QPointF &at, double radiusKm, bool includeUC)
{
    int c = 0;
    for (const auto &f : ctx.w.facilities) {
        if (!f.isOperating() && !(includeUC && f.status == FacilityStatus::UnderConstruction)) continue;
        const auto *d = ctx.def(f);
        if (!d || !d->tags.contains(tag)) continue;
        if (radiusKm > 0 && geo::haversineKm(f.location, at) > radiusKm) continue;
        ++c;
    }
    return c;
}

double EconomyEngine::supportFactor(const SimContext &ctx, const FacilityDefinition &d, const QPointF &at, QStringList *why)
{
    double f = 1.0;
    for (const auto &s : d.supporting) {
        const int have = countTagged(ctx, s.tag, at, s.radiusKm);
        if (have >= s.count) continue;
        f *= ctx.p("tech.supportMissingFactor");
        if (why) *why << QStringLiteral("Missing supporting facility '%1' (%2 of %3%4): %5").arg(s.tag).arg(have).arg(s.count)
                              .arg(s.radiusKm > 0 ? QStringLiteral(" within %1 km").arg(s.radiusKm) : QString(), s.why);
    }
    return f;
}

double EconomyEngine::transportScore(const FacilityDefinition &d, const PlacedFacility &f, QStringList *missing)
{
    if (d.isLine()) return 1.0;
    QStringList needs = d.transportNeeds;
    // every industrial or service facility needs at least road access to markets
    if (needs.isEmpty() && (d.tags.contains("manufacturing") || d.tags.contains("mining") || d.tags.contains("logistics") ||
                            d.nodeOf.contains("road") || d.nodeOf.contains("rail")))
        needs << "road";
    if (needs.isEmpty()) return 1.0;
    int ok = 0;
    for (const auto &n : needs) {
        bool has = false;
        if (n == "road") has = f.roadAccess;
        else if (n == "rail") has = f.railAccess;
        else if (n == "port" || n == "sea") has = f.portAccess;
        else if (n == "airport" || n == "air") has = f.airAccess;
        else if (n == "fibre") has = f.fibreAccess;
        if (has) ++ok;
        else if (missing) *missing << n;
    }
    return double(ok) / needs.size();
}

void EconomyEngine::prepare(SimContext &ctx)
{
    auto &w = ctx.w;
    const auto &geo = ctx.geo;
    const int ns = geo.states().size();
    // ---- workforce demand & supply per state ----
    QVector<SkillVector> demand(ns), supply = w.skills.byState;
    supply.resize(ns);
    QVector<double> stateLabour(ns, 0.0);
    for (int di = 0; di < w.districts.size(); ++di) {
        const int s = geo.districts()[di].stateIndex;
        if (s >= 0) stateLabour[s] += w.districts[di].labourForce;
    }
    const double unskShare = ctx.p("skills.unskilledShareOfLabour");
    for (int s = 0; s < ns; ++s) supply[s][Unskilled] = stateLabour[s] * unskShare;
    for (const auto &f : w.facilities) {
        if (!f.isOperating() || f.district < 0) continue;
        const auto *d = ctx.def(f);
        if (!d) continue;
        const int s = geo.districts()[f.district].stateIndex;
        if (s >= 0) demand[s] += d->operationsJobs.scaled(d->isLine() ? f.scale : 1.0);
    }
    SkillVector surplus;
    for (int s = 0; s < ns; ++s)
        for (int k = 0; k < SkillCount; ++k) surplus[k] += std::max(0.0, supply[s][k] - demand[s][k]);
    const double mob = ctx.p("skills.mobility");
    m_fill.resize(ns);
    for (int s = 0; s < ns; ++s)
        for (int k = 0; k < SkillCount; ++k) {
            const double dmd = demand[s][k];
            m_fill[s][k] = dmd <= 0 ? 1.0 : std::min(1.0, (supply[s][k] + mob * std::max(0.0, surplus[k] - std::max(0.0, supply[s][k] - dmd))) / dmd);
        }

    // ---- per-facility pre-efficiency ----
    for (auto &f : w.facilities) {
        f.limitingFactors.clear();
        const auto *d = ctx.def(f);
        if (!d || !f.isOperating()) { f.efficiency = 0; f.staffing = 0; continue; }
        const int s = f.district >= 0 ? geo.districts()[f.district].stateIndex : -1;
        // staffing: blend of weighted average and the scarcest significant skill
        const double tot = d->operationsJobs.total();
        if (tot <= 0 || s < 0) f.staffing = 1.0;
        else {
            double avg = 0, mn = 1.0;
            for (int k = 0; k < SkillCount; ++k) {
                const double share = d->operationsJobs[k] / tot;
                avg += share * m_fill[s][k];
                if (share >= 0.1) mn = std::min(mn, m_fill[s][k]);
            }
            f.staffing = 0.5 * avg + 0.5 * mn;
            if (f.staffing < 0.95) f.limitingFactors << QStringLiteral("Workforce shortage (%1% staffed)").arg(f.staffing * 100, 0, 'f', 0);
        }
        // inputs
        f.inputSupply = 1.0;
        for (const auto &in : d->inputs) {
            if (in.qtyPerYear <= 0) continue;
            const double a = m_availability.value(in.good, -1);
            const auto *g = ctx.cat.good(in.good);
            const double av = a < 0 ? (g ? g->importability : 1.0) : a;
            if (av < f.inputSupply) f.inputSupply = av;
            if (av < 0.95 && g) f.limitingFactors << QStringLiteral("Insufficient %1 supply (%2%)").arg(g->name).arg(av * 100, 0, 'f', 0);
        }
        QStringList missing;
        f.transportScore = transportScore(*d, f, &missing);
        if (!missing.isEmpty()) f.limitingFactors << "No " + missing.join(", ") + " access";
        QStringList why;
        f.techFactor = techFactor(ctx, *d, &why);
        f.supportFactor = supportFactor(ctx, *d, f.location, &why);
        f.limitingFactors << why;
        double eff = std::min(f.staffing, f.inputSupply) * std::sqrt(std::max(0.0, f.transportScore)) * f.techFactor * f.supportFactor;
        if (d->isLine() && f.networkBroken) { eff *= 0.0; f.limitingFactors << "Unbridged river crossing breaks this line"; }
        f.efficiency = std::clamp(eff, 0.0, 1.0);
    }
}

void EconomyEngine::finalize(SimContext &ctx, double publicPaid, double privatePaid, bool applyBudget)
{
    auto &w = ctx.w;
    const auto &geo = ctx.geo;
    const int nd = w.districts.size();
    auto &s = w.stats;

    // ---- final efficiency ----
    const double allMul = ctx.mods.mul("efficiency.mult.all");
    for (auto &f : w.facilities) {
        const auto *d = ctx.def(f);
        if (!d || !f.isOperating()) { f.efficiency = 0; continue; }
        double m = allMul * ctx.mods.mul("efficiency.mult." + d->category);
        for (const auto &t : d->tags) m *= ctx.mods.mul("efficiency.mult.tag." + t) * (t == "manufacturing" || t == "labour_intensive" ? ctx.mods.mul("efficiency.mult." + t) : 1.0);
        if (f.powerSupply < 0.95 && d->needsGrid && d->powerDemandMW > 0) f.limitingFactors << QStringLiteral("Electricity supply %1%").arg(f.powerSupply * 100, 0, 'f', 0);
        if (f.waterSupply < 0.95 && d->needsWater) f.limitingFactors << QStringLiteral("Water supply %1%").arg(f.waterSupply * 100, 0, 'f', 0);
        f.efficiency = std::clamp(f.efficiency * std::min(f.powerSupply, f.waterSupply) * m, 0.0, 1.0);
    }

    // ---- services, food, goods ----
    for (auto &d : w.districts) {
        d.hospitalBeds = d.primaryCareCoverage = d.schoolSeats = d.higherEdSeats = d.foodProductionT = d.irrigationHa = 0;
        d.modernHousing = 0; d.formalJobs = d.formalJobsFilled = d.constructionJobs = 0; d.landUsedHa = 0;
    }
    QHash<QString, double> production, interm;
    QVector<double> facVA(nd, 0.0), publicVA(nd, 0.0), constrVA(nd, 0.0);
    double agriVA = 0, industryVA = 0, manufVA = 0, servicesFacVA = 0, computeMW = 0, dcMW = 0, portCap = 0, airCap = 0;
    double opexPublic = 0;
    const double opexMul = ctx.mods.mul("opex.mult.public");
    double yieldBoost = 0;
    for (const auto &f : w.facilities) {
        const auto *d = ctx.def(f);
        if (!d || f.district < 0) continue;
        auto &dist = w.districts[f.district];
        const double sc = d->isLine() ? f.scale : 1.0;
        dist.landUsedHa += d->landHa * sc;
        if (f.status == FacilityStatus::UnderConstruction) {
            dist.constructionJobs += d->constructionJobs * sc;
            const double spend = f.capexCr / std::max(1, d->constructionMonths) * 12.0;
            constrVA[f.district] += spend * ctx.p("macro.constructionVAShare");
            continue;
        }
        if (!f.isOperating()) continue;
        const double e = f.efficiency;
        dist.modernHousing += d->housingPersons * sc * std::max(0.5, e); // housing stock exists even if services lag
        dist.hospitalBeds += d->hospitalBeds * e * ctx.mods.mul("health.quality");
        dist.primaryCareCoverage += d->primaryCareCoverage * e * ctx.mods.mul("health.quality");
        dist.schoolSeats += d->schoolSeats * e;
        dist.higherEdSeats += d->higherEdSeats * e;
        dist.irrigationHa += d->irrigationHa * e;
        yieldBoost += d->foodYieldBoost * e;
        dist.formalJobs += d->operationsJobs.total() * sc;
        dist.formalJobsFilled += d->operationsJobs.total() * sc * f.staffing;
        computeMW += d->computeMW * e;
        if (d->tags.contains("data_centre")) dcMW += d->computeMW * e;
        if (d->tags.contains("seaport")) portCap += d->freightCapacityMtpa * e;
        if (d->tags.contains("airport")) airCap += d->passengerCapacityMpa * e;
        for (const auto &o : d->outputs) production[o.good] += o.qtyPerYear * sc * e;
        // input requirements at the facility's operating level excluding the input constraint
        const double potential = f.inputSupply > 0 ? std::min(1.0, e / f.inputSupply) : std::min(f.staffing, f.powerSupply);
        for (const auto &in : d->inputs) interm[in.good] += in.qtyPerYear * sc * potential;
        // value added: transport lines and logistics nodes only add value when connected
        double va = d->valueAddedCrPerYear * sc * e;
        if (!d->nodeOf.isEmpty() && f.component < 0) va *= 0.5;
        facVA[f.district] += va;
        if (d->category == "Agriculture & Food") agriVA += va;
        else if (d->tags.contains("manufacturing") || d->tags.contains("mining") || d->tags.contains("power_plant") || d->category == "Energy & Utilities") {
            industryVA += va;
            if (d->tags.contains("manufacturing")) manufVA += va;
        } else servicesFacVA += va;
        if (d->publicOwned) {
            const double opex = d->opexCrPerYear * sc * opexMul;
            opexPublic += opex;
            publicVA[f.district] += opex * 0.6; // salaries & services delivered (assumption)
        }
    }
    // food: legacy farming + irrigation + modern farms
    const double kgCap = ctx.p("food.kgPerCapita");
    const double legacySelf = ctx.p("food.legacySelfSufficiency");
    const double irrBoost = ctx.p("food.irrigationYieldBoost");
    const double sown = ctx.p("agri.netSownShare");
    double foodNeed = 0, foodProd = production.value("food_grain");
    const double yieldMul = ctx.mods.mul("food.yield") * (1.0 + std::min(0.5, yieldBoost));
    for (int di = 0; di < nd; ++di) {
        auto &d = w.districts[di];
        const double need = d.population * kgCap / 1000.0;
        foodNeed += need;
        const double sownHa = geo.districts()[di].areaKm2 * 100.0 * sown;
        const double irrShare = sownHa > 0 ? std::min(1.0, d.irrigationHa / sownHa) : 0;
        // legacy production tied to the census-2011 rural workforce share of the district
        const double legacyFood = w.userSettings.value("baseline.foodNeedT." + QString::number(di), need) * legacySelf * (1 + irrBoost * irrShare) * yieldMul;
        d.foodProductionT = legacyFood;
        foodProd += legacyFood;
    }
    s.foodSupplyRatio = foodNeed > 0 ? foodProd / foodNeed : 0;

    // ---- goods balance & trade ----
    // Per-capita goods demand is calibrated to real-world India; scale by income relative to it.
    const double gdppc = s.population > 0 ? s.gdpCr / s.population : 0;
    const double gdppcRef = w.userSettings.value("baseline.realGdpPerCapitaCr", 0);
    const double incomeRatio = gdppcRef > 0 && gdppc > 0 ? gdppc / gdppcRef : 1.0;
    double importsCr = 0, exportsCr = 0, demandValue = 0, importValue = 0;
    w.goodsSupply.clear(); w.goodsDemand.clear(); w.goodsImports.clear(); w.goodsExports.clear();
    const double importShare = ctx.mods.mul("import.share"), importCost = ctx.mods.mul("import.cost");
    const double exportShare = ctx.p("macro.exportShareOfSurplus") * ctx.mods.mul("export.share");
    for (const auto &g : ctx.cat.goods()) {
        if (g.id == "food_grain") continue; // handled by food model
        double prod = production.value(g.id);
        const double fin = g.perCapitaDemand * s.population * std::pow(std::max(0.05, incomeRatio), g.incomeElasticity) * ctx.mods.mul("goods.demand." + g.id);
        const double dem = interm.value(g.id) + fin;
        // traditional (legacy) agriculture supplies part of farm-product demand, like food grains
        if (g.category == "Agriculture" && g.id != "fertilizer" && g.id != "processed_food") prod += fin * legacySelf * yieldMul;
        const double shortfall = std::max(0.0, dem - prod);
        const double imp = shortfall * std::clamp(g.importability * importShare, 0.0, 1.0);
        const double exp = std::max(0.0, prod - dem) * exportShare;
        m_availability[g.id] = dem > 0 ? std::min(1.0, (prod + imp) / dem) : 1.0;
        importsCr += imp * g.unitValueCr * importCost;
        exportsCr += exp * g.unitValueCr;
        demandValue += dem * g.unitValueCr;
        importValue += imp * g.unitValueCr;
        if (prod > 0) w.goodsSupply[g.id] = prod;
        if (dem > 0) w.goodsDemand[g.id] = dem;
        if (imp > 0) w.goodsImports[g.id] = imp;
        if (exp > 0) w.goodsExports[g.id] = exp;
    }
    s.importsCr = importsCr; s.exportsCr = exportsCr;
    s.importDependency = demandValue > 0 ? importValue / demandValue : 0;
    s.computeMW = computeMW; s.dataCentreMW = dcMW; s.portCapacityMtpa = portCap; s.airportCapacityMpa = airCap;

    // ---- labour market & legacy economy ----
    const double outPerWorker = ctx.p("macro.legacyOutputPerWorkerCr");
    const double pBoost = ctx.p("macro.legacyPowerBoost"), aBoost = ctx.p("macro.legacyAccessBoost");
    const double legacyMul = ctx.mods.mul("legacy.productivity");
    if (applyBudget) w.legacyProductivityIndex *= std::pow(1.0 + ctx.p("macro.legacyProductivityGrowth"), 1.0 / 12.0);
    const double legacyJobGrowth = applyBudget ? std::pow(1.0 + ctx.p("macro.legacyEmploymentGrowth"), 1.0 / 12.0) : 1.0;
    double totalFormalDemand = 0, totalFormalFilled = 0, totalConstr = 0, totalLF = 0, totalEmployed = 0;
    double legacyOut = 0, totalPop = 0;
    for (auto &d : w.districts) { totalFormalDemand += d.formalJobs; totalFormalFilled += d.formalJobsFilled; totalConstr += d.constructionJobs; totalPop += d.population; }
    QVector<double> gdpD(nd, 0.0);
    for (int di = 0; di < nd; ++di) {
        auto &d = w.districts[di];
        d.legacyEmploymentCapacity *= legacyJobGrowth;
        const double nonLegacy = std::min(d.labourForce, d.formalJobsFilled + d.constructionJobs);
        const double legacyEmployed = std::min(d.legacyEmploymentCapacity, std::max(0.0, d.labourForce - nonLegacy));
        const double employed = nonLegacy + legacyEmployed;
        d.unemployed = std::max(0.0, d.labourForce - employed);
        totalLF += d.labourForce;
        totalEmployed += employed;
        const double powerFrac = d.powerDemandMW > 0 ? d.powerServedMW / d.powerDemandMW : 0;
        d.legacyOutputCr = legacyEmployed * outPerWorker * w.legacyProductivityIndex * legacyMul * (1 + pBoost * powerFrac + aBoost * d.accessIndex);
        // agriculture part of legacy output scales with food yield improvements
        const double agriShare = ctx.p("macro.legacyAgriShare");
        const double baseFood = w.userSettings.value("baseline.foodNeedT." + QString::number(di), 0) * legacySelf;
        const double foodRatio = baseFood > 0 ? d.foodProductionT / baseFood : 1.0;
        d.legacyOutputCr *= (1 - agriShare) + agriShare * foodRatio;
        legacyOut += d.legacyOutputCr;
        gdpD[di] = d.legacyOutputCr + facVA[di] + publicVA[di] + constrVA[di];
    }
    // induced services: half local, half distributed by population
    const double inducedBase = (std::accumulate(facVA.begin(), facVA.end(), 0.0) + std::accumulate(constrVA.begin(), constrVA.end(), 0.0)) * ctx.p("macro.servicesMultiplier");
    double gdp = 0;
    for (int di = 0; di < nd; ++di) {
        auto &d = w.districts[di];
        const double localBase = (facVA[di] + constrVA[di]) * ctx.p("macro.servicesMultiplier");
        gdpD[di] += 0.5 * localBase + 0.5 * inducedBase * (totalPop > 0 ? d.population / totalPop : 0);
        d.gdpCr = gdpD[di];
        gdp += gdpD[di];
    }
    const double prevGdp = s.gdpCr;
    s.gdpCr = gdp;
    const double inr = ctx.p("macro.inrPerUsd");
    s.gdpUsd = gdp * 1e7 / inr;
    s.gdpPerCapitaUsd = s.population > 0 ? s.gdpUsd / s.population : 0;
    {
        // growth over the last 12 months (history holds one point per month)
        double ago = 0;
        for (int i = w.history.size() - 1; i >= 0; --i) if (w.history[i].month == w.month + (applyBudget ? 1 : 0) - 12) { ago = w.history[i].v.value("gdpCr"); break; }
        if (ago > 0) s.gdpGrowth = (gdp / ago - 1) * 100;
    }
    Q_UNUSED(prevGdp);
    const double constrTotalVA = std::accumulate(constrVA.begin(), constrVA.end(), 0.0);
    const double agri = legacyOut * ctx.p("macro.legacyAgriShare") + agriVA;
    const double industry = industryVA + constrTotalVA + legacyOut * (1 - ctx.p("macro.legacyAgriShare")) * 0.3;
    s.agricultureShare = gdp > 0 ? agri / gdp : 0;
    s.industryShare = gdp > 0 ? industry / gdp : 0;
    s.manufacturingShare = gdp > 0 ? manufVA / gdp : 0;
    s.servicesShare = std::max(0.0, 1 - s.agricultureShare - s.industryShare);
    s.labourForce = totalLF;
    s.employed = totalEmployed;
    s.unemploymentRate = totalLF > 0 ? (totalLF - totalEmployed) / totalLF : 0;

    // ---- public finances ----
    auto &b = w.budget;
    if (!applyBudget) { b.revenueCrYr = (ctx.p("fiscal.taxToGdp") + ctx.mods.add("tax.rate")) * gdp; return; }
    const double taxRate = ctx.p("fiscal.taxToGdp") + ctx.mods.add("tax.rate");
    const double revenue = taxRate * gdp / 12.0;
    const double pop65 = w.pop.ages(13, 16);
    const double pension = pop65 * ctx.p("fiscal.pensionCoverage") * ctx.p("fiscal.pensionPerPersonCr") / 12.0;
    const double policyCost = PolicyEngine::annualCostCr(ctx) / 12.0;
    const double debtRatio = gdp > 0 ? b.debtCr / gdp : 0;
    const double rate = ctx.p("fiscal.interestRate") + ctx.p("fiscal.riskPremiumSlope") * std::max(0.0, debtRatio - 0.8);
    const double interest = b.debtCr * rate / 12.0;
    const double baseline = ctx.p("fiscal.baselineSpendingShare") * gdp / 12.0;
    const double spend = publicPaid + opexPublic / 12.0 + pension + policyCost + interest + baseline;
    b.treasuryCr += revenue - spend;
    if (b.treasuryCr < 0) { b.debtCr += -b.treasuryCr; b.treasuryCr = 0; }
    b.revenueCrYr = revenue * 12; b.capexCrYr = publicPaid * 12; b.opexCrYr = (opexPublic / 12.0 + pension + baseline) * 12;
    b.policyCostCrYr = policyCost * 12; b.interestCrYr = interest * 12; b.privateInvestmentCrYr = privatePaid * 12;
}

} // namespace isim
