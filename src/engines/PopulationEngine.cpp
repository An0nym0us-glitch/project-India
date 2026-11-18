#include "PopulationEngine.h"
#include "EconomyEngine.h"
#include "PolicyEngine.h"
#include "catalog/Catalog.h"
#include "gis/GeoDatabase.h"
#include <algorithm>
#include <cmath>

namespace isim {

std::array<double, 17> PopulationEngine::mortalityRates(double a, double b, double child)
{
    std::array<double, 17> m{};
    for (int g = 0; g < 17; ++g) {
        const double age = g * 5 + 2.5;
        m[g] = std::min(0.9, a * std::exp(b * age) + (g == 0 ? child : 0.0) + (g == 1 ? child * 0.08 : 0.0));
    }
    return m;
}

double PopulationEngine::lifeExpectancy(double a, double b, double child)
{
    // abridged life table on 5-year groups, open interval 80+
    const auto m = mortalityRates(a, b, child);
    double l = 1.0, e0 = 0;
    for (int g = 0; g < 16; ++g) {
        const double q = 1 - std::exp(-5 * m[g]);
        const double L = 5 * l * (1 - q / 2);
        e0 += L;
        l *= (1 - q);
    }
    e0 += l / std::max(1e-6, m[16]);
    return e0;
}

double PopulationEngine::solveMortalityLevel(double target, double b, double child)
{
    double lo = 1e-7, hi = 0.05;
    for (int i = 0; i < 80; ++i) {
        const double mid = std::sqrt(lo * hi);
        if (lifeExpectancy(mid, b, child) > target) lo = mid; else hi = mid;
    }
    return std::sqrt(lo * hi);
}

void PopulationEngine::step(SimContext &ctx, bool advance)
{
    auto &w = ctx.w;
    auto &s = w.stats;
    auto &pop = w.pop;
    const auto &us = w.userSettings;

    // ---- service indices (0..1) ----
    double beds = 0, pcc = 0, seats = 0, totalPop = 0, pm = 0, urban = 0, water = 0, sewT = 0, sewG = 0;
    for (const auto &d : w.districts) {
        beds += d.hospitalBeds; pcc += d.primaryCareCoverage; seats += d.schoolSeats;
        totalPop += d.population; pm += d.pm25 * d.population; urban += d.urbanPopulation;
        water += d.waterSuppliedMLD > 0 && d.waterDemandMLD > 0 ? d.population * d.waterSuppliedMLD / d.waterDemandMLD : 0;
    }
    Q_UNUSED(sewT); Q_UNUSED(sewG);
    if (totalPop <= 0) return;
    const double bedsPer1000 = beds / totalPop * 1000;
    const double healthIndex = 0.5 * std::min(1.0, bedsPer1000 / ctx.p("health.bedsPer1000Target")) + 0.5 * std::min(1.0, pcc / totalPop);
    const double schoolAge = pop.ages(1, 3) * (12.0 / 15.0); // ages ~6-17 approximated from 5-19
    const double eduIndex = schoolAge > 0 ? std::min(1.0, seats / schoolAge) : 0;
    const double waterIndex = 0.5 * (water / totalPop) + 0.5 * s.sewageTreatedShare;
    const double pm25 = pm / totalPop;
    const double urbanShare = urban / totalPop;
    const double gdppc0 = us.value("baseline.gdpPerCapitaCr", 0);
    const double gdppc = s.gdpCr / totalPop;
    s.hospitalBedsPer1000 = bedsPer1000;
    s.primaryCareCoverage = std::min(1.0, pcc / totalPop);
    s.schoolSeatShare = eduIndex;
    s.pm25PopWeighted = pm25;
    s.urbanShare = urbanShare;
    if (!advance) {
        if (s.lifeExpectancy <= 0) s.lifeExpectancy = w.baseLifeExpectancy;
        if (s.tfr <= 0) s.tfr = w.baseTfr;
        const double wa = pop.ages(3, 12), tot = pop.total();
        s.population = tot; s.workingAgeShare = tot > 0 ? wa / tot : 0; s.dependencyRatio = wa > 0 ? (tot - wa) / wa * 100 : 0;
        double sh = 0, cap = 0; for (const auto &d : w.districts) { sh += std::max(0.0, d.population - d.legacyHousing - d.modernHousing); cap += d.modernHousing; }
        s.housingShortagePersons = sh; s.housingCapacity = cap;
        return;
    }

    // ---- life expectancy & mortality ----
    double le = w.baseLifeExpectancy
        + ctx.p("pop.lePerHealthIndex") * (healthIndex - us.value("baseline.healthIndex", 0))
        + ctx.p("pop.lePerWaterIndex") * (waterIndex - us.value("baseline.waterIndex", 0))
        - ctx.p("pop.lePerPm25") * (pm25 - us.value("baseline.pm25", pm25));
    if (gdppc0 > 0 && gdppc > 0) le += ctx.p("pop.lePerIncomeLog") * std::log(gdppc / gdppc0);
    le = std::clamp(le, 40.0, 88.0);
    // smooth adjustment (outcomes respond with lags)
    s.lifeExpectancy = s.lifeExpectancy <= 0 ? le : s.lifeExpectancy + (le - s.lifeExpectancy) / 36.0;
    if (std::abs(s.lifeExpectancy - m_cachedLE) > 0.01) {
        m_a = solveMortalityLevel(s.lifeExpectancy, kGompertzB, childMortality(s.lifeExpectancy));
        m_cachedLE = s.lifeExpectancy;
    }
    auto mort = mortalityRates(m_a, kGompertzB, childMortality(s.lifeExpectancy));
    const double mortMul = ctx.mods.mul("mortality");

    // ---- fertility ----
    double tfrTarget = w.baseTfr * ctx.mods.mul("tfr")
        * (1 - ctx.p("pop.tfrUrbanEffect") * (urbanShare - us.value("baseline.urbanShare", urbanShare)))
        * (1 - ctx.p("pop.tfrEducationEffect") * (eduIndex - us.value("baseline.eduIndex", 0)));
    tfrTarget = std::clamp(tfrTarget, 0.8, 6.0);
    if (m_tfr < 0) m_tfr = s.tfr > 0 ? s.tfr : w.baseTfr;
    m_tfr += (tfrTarget - m_tfr) / 24.0;
    s.tfr = m_tfr;
    // age-specific fertility shares for groups 15-19 .. 45-49 (assumption, typical Indian pattern)
    static const double asfrShare[7] = {0.08, 0.30, 0.31, 0.18, 0.08, 0.04, 0.01};
    double births = 0;
    for (int k = 0; k < 7; ++k) births += pop.female[3 + k] * (m_tfr * asfrShare[k] / 5.0) / 12.0;
    const double srb = ctx.p("pop.sexRatioAtBirth") + ctx.mods.add("sexratio.distortion");
    const double before = pop.total();

    // ---- deaths and ageing (monthly) ----
    std::array<double, 17> nm{}, nf{};
    for (int g = 0; g < 17; ++g) {
        const double q = 1 - std::exp(-mort[g] * mortMul / 12.0);
        const double sm = pop.male[g] * (1 - q), sf = pop.female[g] * (1 - q);
        if (g < 16) {
            const double move = 1.0 / 60.0;
            nm[g] += sm * (1 - move); nf[g] += sf * (1 - move);
            nm[g + 1] += sm * move;   nf[g + 1] += sf * move;
        } else { nm[g] += sm; nf[g] += sf; }
    }
    nm[0] += births * srb / (1 + srb);
    nf[0] += births / (1 + srb);
    pop.male = nm; pop.female = nf;
    const double after = pop.total();
    const double growth = before > 0 ? after / before : 1.0;

    // ---- district distribution: natural growth + migration ----
    const double lfp = ctx.p("pop.lfpRatio") + ctx.mods.add("lfp");
    const double workingAge = pop.ages(3, 12);
    double meanAttr = 0;
    for (auto &d : w.districts) {
        const double housing = d.legacyHousing + d.modernHousing;
        const double jobRatio = d.labourForce > 0 ? std::min(1.0, (d.formalJobs + d.legacyEmploymentCapacity) / d.labourForce) : 0;
        const double houseAvail = d.population > 0 ? std::clamp(housing / d.population, 0.0, 1.2) : 1.0;
        const double power = d.powerDemandMW > 0 ? d.powerServedMW / d.powerDemandMW : 0;
        const double wat = d.waterDemandMLD > 0 ? d.waterSuppliedMLD / d.waterDemandMLD : 0;
        d.attractiveness = 0.45 * jobRatio + 0.2 * houseAvail + 0.15 * power + 0.1 * wat + 0.1 * d.accessIndex - 0.1 * std::min(1.0, d.pm25 / 150.0);
        meanAttr += d.attractiveness * d.population;
    }
    meanAttr /= totalPop;
    const double migRate = ctx.p("pop.migrationRate") * ctx.mods.mul("migration.mult") / 12.0;
    double outflow = 0, inWeight = 0;
    for (auto &d : w.districts) {
        d.population *= growth;
        d.legacyUrban *= growth;
        d.legacyHousing *= std::pow(1 - ctx.p("housing.legacyDepreciation"), 1.0 / 12.0);
        const double diff = d.attractiveness - meanAttr;
        if (diff < 0) { const double out = d.population * migRate * std::min(1.0, -diff * 4); d.population -= out; outflow += out; }
        else inWeight += diff * d.population;
    }
    for (auto &d : w.districts) {
        const double diff = d.attractiveness - meanAttr;
        if (diff > 0 && inWeight > 0) d.population += outflow * diff * d.population / inWeight;
        // urban population: legacy urban + modern (non-village) housing capacity
        d.urbanPopulation = std::min(d.population, d.legacyUrban + d.modernHousing);
        d.labourForce = d.population / std::max(1.0, after) * workingAge * lfp;
    }
    s.population = after;
    s.workingAgeShare = workingAge / after;
    s.dependencyRatio = workingAge > 0 ? (after - workingAge) / workingAge * 100 : 0;
    double shortage = 0, cap = 0;
    for (const auto &d : w.districts) { shortage += std::max(0.0, d.population - d.legacyHousing - d.modernHousing); cap += d.modernHousing; }
    s.housingShortagePersons = shortage;
    s.housingCapacity = cap;
    s.tertiaryEnrolment = 0;
    double hes = 0; for (const auto &d : w.districts) hes += d.higherEdSeats;
    const double tertiaryAge = pop.ages(4, 4);
    s.tertiaryEnrolment = tertiaryAge > 0 ? hes / tertiaryAge * 100 : 0;
}

void SkillsEngine::step(SimContext &ctx)
{
    auto &w = ctx.w;
    const auto &geo = ctx.geo;
    const int ns = geo.states().size();
    w.skills.byState.resize(ns);
    const double attr = ctx.p("skills.attrition") / 12.0;
    const double gradMul = ctx.mods.mul("graduates.mult") * ctx.mods.mul("education.quality");
    // graduates
    for (const auto &f : w.facilities) {
        if (!f.isOperating() || f.district < 0) continue;
        const auto *d = ctx.def(f);
        if (!d || d->graduatesPerYear.total() <= 0) continue;
        const int s = geo.districts()[f.district].stateIndex;
        if (s < 0) continue;
        w.skills.byState[s] += d->graduatesPerYear.scaled(f.efficiency * gradMul / 12.0);
    }
    // policy inflows distributed by population
    QVector<double> statePop(ns, 0.0);
    double total = 0;
    for (int di = 0; di < w.districts.size(); ++di) {
        const int s = geo.districts()[di].stateIndex;
        if (s >= 0) { statePop[s] += w.districts[di].population; total += w.districts[di].population; }
    }
    SkillVector inflow;
    inflow[Skilled] = ctx.mods.add("skilled.inflow.skilled");
    inflow[Engineer] = ctx.mods.add("skilled.inflow.engineer");
    inflow[Scientist] = ctx.mods.add("skilled.inflow.scientist");
    inflow[Medical] = ctx.mods.add("skilled.inflow.medical");
    // brain drain of surplus engineers & scientists
    SkillVector demand;
    for (const auto &f : w.facilities) {
        if (!f.isOperating()) continue;
        const auto *d = ctx.def(f);
        if (d) demand += d->operationsJobs.scaled(d->isLine() ? f.scale : 1.0);
    }
    const SkillVector nat = w.skills.national();
    const double drain = ctx.p("skills.brainDrainRate") * ctx.mods.mul("braindrain") / 12.0;
    for (int s = 0; s < ns && total > 0; ++s) {
        auto &pool = w.skills.byState[s];
        const double share = statePop[s] / total;
        for (int k = Skilled; k < SkillCount; ++k) {
            pool[k] += inflow[k] / 12.0 * share;
            pool[k] *= (1 - attr);
            if ((k == Engineer || k == Scientist) && nat[k] > 0) {
                const double surplusShare = std::max(0.0, nat[k] - demand[k]) / nat[k];
                pool[k] *= (1 - drain * surplusShare);
            }
        }
    }
    const SkillVector n2 = w.skills.national();
    w.stats.researchersPerMillion = w.stats.population > 0 ? n2[Scientist] / w.stats.population * 1e6 : 0;
}

QHash<QString, double> TechnologyEngine::researchRates(const SimContext &ctx)
{
    QHash<QString, double> r;
    const double allMul = ctx.mods.mul("research.mult.all");
    for (const auto &f : ctx.w.facilities) {
        if (!f.isOperating()) continue;
        const auto *d = ctx.def(f);
        if (!d) continue;
        for (auto it = d->researchPoints.begin(); it != d->researchPoints.end(); ++it)
            r[it.key()] += it.value() * f.efficiency * allMul * ctx.mods.mul("research.mult." + it.key());
    }
    const auto &domains = ctx.cat.techDomains();
    const double base = ctx.w.skills.national()[Scientist] * ctx.p("tech.baseResearchPerResearcher") / std::max<int>(1, domains.size());
    for (const auto &dom : domains)
        r[dom.id] += base * allMul + ctx.mods.add("tech.points." + dom.id) + ctx.mods.add("tech.points.all");
    return r;
}

bool TechnologyEngine::canAdvance(const SimContext &ctx, const QString &domain, QStringList *blockers)
{
    const auto *dom = ctx.cat.techDomain(domain);
    if (!dom) return false;
    const int lvl = ctx.w.techLevel.value(domain, 0);
    if (lvl + 1 >= dom->levels.size()) { if (blockers) *blockers << "Maximum level reached"; return false; }
    const auto &next = dom->levels[lvl + 1];
    bool ok = true;
    if (ctx.w.techPoints.value(domain) < next.pointsRequired) {
        ok = false;
        if (blockers) *blockers << QStringLiteral("Research points %1 / %2").arg(ctx.w.techPoints.value(domain), 0, 'f', 0).arg(next.pointsRequired, 0, 'f', 0);
    }
    for (auto it = next.prerequisites.begin(); it != next.prerequisites.end(); ++it)
        if (ctx.w.techLevel.value(it.key(), 0) < it.value()) {
            ok = false;
            const auto *pd = ctx.cat.techDomain(it.key());
            if (blockers) *blockers << QStringLiteral("Requires %1 level %2").arg(pd ? pd->name : it.key()).arg(it.value());
        }
    for (const auto &tag : next.requiredFacilityTags)
        if (EconomyEngine::countTagged(ctx, tag, {}, 0) == 0) {
            ok = false;
            if (blockers) *blockers << QStringLiteral("Requires an operating facility tagged '%1'").arg(tag);
        }
    return ok;
}

void TechnologyEngine::step(SimContext &ctx)
{
    const auto rates = researchRates(ctx);
    for (auto it = rates.begin(); it != rates.end(); ++it) ctx.w.techPoints[it.key()] += it.value() / 12.0;
    for (const auto &dom : ctx.cat.techDomains()) {
        while (canAdvance(ctx, dom.id)) {
            const int lvl = ctx.w.techLevel.value(dom.id, 0) + 1;
            ctx.w.techLevel[dom.id] = lvl;
            if (!ctx.fastMode)
                ctx.w.notify(Notification::Info, QStringLiteral("Technology milestone: %1 reached level %2 — %3.").arg(dom.name).arg(lvl).arg(dom.levels[lvl].name));
        }
    }
    // R&D share of GDP: operating costs of research facilities + spending on R&D policies
    double rnd = 0;
    for (const auto &f : ctx.w.facilities) {
        const auto *d = ctx.def(f);
        if (d && f.isOperating() && d->tags.contains("research")) rnd += d->opexCrPerYear * (d->isLine() ? f.scale : 1.0);
    }
    for (const auto &inst : ctx.w.policies) {
        const auto *pd = ctx.policies.find(inst.id);
        if (pd && pd->category == "Research & development" && ctx.w.month >= inst.startMonth)
            rnd += (pd->costCrPerYearAtFull + pd->costPctGdpAtFull * ctx.w.stats.gdpCr) * std::clamp(inst.intensity, 0.0, 1.0);
    }
    ctx.w.stats.rndShareGdp = ctx.w.stats.gdpCr > 0 ? rnd / ctx.w.stats.gdpCr * 100 : 0;
}

} // namespace isim
