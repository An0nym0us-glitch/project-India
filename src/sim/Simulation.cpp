#include "Simulation.h"
#include "core/DataPaths.h"
#include <QDebug>
#include <algorithm>
#include <cmath>

namespace isim {

bool SimulationResources::load(const QString &dir, QString *error)
{
    QString e;
    if (!geo.load(dir + "/geo", &e)) { if (error) *error = "Geography: " + e; return false; }
    if (!catalog.load(dir, &e)) { if (error) *error = "Catalogue: " + e; return false; }
    if (!policies.load(dir + "/policies/policies.json", &e)) { if (error) *error = "Policies: " + e; return false; }
    if (!params.load(dir + "/model_parameters.json", &e)) { if (error) *error = "Parameters: " + e; return false; }
    if (!reference.load(dir + "/reference/worldbank_reference.json", &e))
        qWarning() << "Reference data unavailable:" << e; // comparison will show 'unavailable'
    return true;
}

Simulation::Simulation(const SimulationResources &res) : m_res(&res), m_p(res.params) {}

SimContext Simulation::context()
{
    return SimContext{m_w, m_res->geo, m_res->catalog, m_res->policies, m_p, m_mods, m_net, m_env, m_fast};
}

void Simulation::newScenario(const ScenarioSettings &st)
{
    m_w = WorldState{};
    m_net = NetworkModel{};
    m_env = EnvironmentGrid{};
    m_w.name = st.name;
    m_w.startYear = st.startYear;
    m_w.seed = st.seed;
    m_w.budget.treasuryCr = st.initialFundCr;
    m_w.userSettings["scenario.initialFundCr"] = st.initialFundCr;
    const auto &geo = m_res->geo;
    const auto &ref = m_res->reference;
    const int refYear = int(m_p("scenario.referenceYear"));
    m_w.userSettings["scenario.referenceYear"] = refYear;

    // --- national totals from World Bank reference data (fallback: parameters) ---
    double census = 0;
    for (const auto &d : geo.districts()) census += d.census.population;
    const auto popRef = ref.valueAt("SP.POP.TOTL", "IND", refYear);
    const double popTarget = popRef.available ? popRef.value : census * 1.17;
    const double scale = census > 0 ? popTarget / census : 1.0;
    const auto tfr = ref.valueAt("SP.DYN.TFRT.IN", "IND", refYear);
    const auto le = ref.valueAt("SP.DYN.LE00.IN", "IND", refYear);
    m_w.baseTfr = tfr.available ? tfr.value : m_p("pop.tfrBase");
    m_w.baseLifeExpectancy = le.available ? le.value : m_p("pop.lifeExpectancyBase");

    // --- age structure ---
    QVector<double> ms, fs; double maleShare = 51.5;
    if (ref.indiaAgeShares(refYear, ms, fs, maleShare)) {
        const double males = popTarget * maleShare / 100.0, females = popTarget - males;
        for (int g = 0; g < 17; ++g) { m_w.pop.male[g] = males * ms[g] / 100.0; m_w.pop.female[g] = females * fs[g] / 100.0; }
    } else {
        // fallback: smooth pyramid (assumption)
        double tot = 0; std::array<double, 17> w{};
        for (int g = 0; g < 17; ++g) { w[g] = std::exp(-0.035 * g * 5) * (g == 16 ? 2.0 : 1.0); tot += w[g]; }
        for (int g = 0; g < 17; ++g) { m_w.pop.male[g] = popTarget * 0.515 * w[g] / tot; m_w.pop.female[g] = popTarget * 0.485 * w[g] / tot; }
    }
    // rescale exactly to target
    const double pt = m_w.pop.total();
    for (int g = 0; g < 17; ++g) { m_w.pop.male[g] *= popTarget / pt; m_w.pop.female[g] *= popTarget / pt; }

    // --- labour market ---
    const double workingAge = m_w.pop.ages(3, 12);
    const auto lf = ref.valueAt("SL.TLF.TOTL.IN", "IND", refYear);
    if (lf.available && workingAge > 0) {
        m_p.set("pop.lfpRatio", lf.value / workingAge);
        m_w.userSettings["param.pop.lfpRatio"] = lf.value / workingAge; // calibrated value travels with the scenario
    }
    const auto ue = ref.valueAt("SL.UEM.TOTL.ZS", "IND", refYear);
    const double u0 = ue.available ? ue.value / 100.0 : m_p("pop.unemploymentBase");
    const double lfp = m_p("pop.lfpRatio");

    // --- districts ---
    const int nd = geo.districts().size();
    m_w.districts.resize(nd);
    for (int i = 0; i < nd; ++i) {
        const auto &c = geo.districts()[i].census;
        auto &d = m_w.districts[i];
        d.population = c.population * scale;
        const double urbShare = c.households > 0 ? c.urbanHouseholds / c.households : 0.3;
        d.legacyUrban = d.urbanPopulation = d.population * urbShare;
        d.legacyHousing = d.population;
        d.personsPerHousehold = c.households > 0 ? c.population / c.households : m_p("pop.personsPerHousehold");
        d.labourForce = d.population / popTarget * workingAge * lfp;
        d.legacyEmploymentCapacity = d.labourForce * (1 - u0);
        m_w.userSettings["baseline.foodNeedT." + QString::number(i)] = d.population * m_p("food.kgPerCapita") / 1000.0;
    }

    // --- skills by state (census 2011 education, scaled; World Bank researchers/physicians) ---
    const int ns = geo.states().size();
    m_w.skills.byState.fill(SkillVector{}, ns);
    QVector<double> stPop(ns, 0.0), stGrad(ns, 0.0);
    for (int i = 0; i < nd; ++i) {
        const auto &gd = geo.districts()[i];
        if (gd.stateIndex < 0) continue;
        auto &sk = m_w.skills.byState[gd.stateIndex];
        sk[Engineer] += gd.census.graduates * scale * m_p("skills.engineerShareOfGraduates");
        sk[Skilled] += (gd.census.secondaryEducation + gd.census.higherEducation) * scale * m_p("skills.skilledShareOfSecondary");
        stPop[gd.stateIndex] += m_w.districts[i].population;
        stGrad[gd.stateIndex] += gd.census.graduates;
    }
    const auto rpm = ref.valueAt("SP.POP.SCIE.RD.P6", "IND", refYear, 8);
    const auto phys = ref.valueAt("SH.MED.PHYS.ZS", "IND", refYear, 8);
    const double researchers = (rpm.available ? rpm.value : m_p("skills.researchersPerMillion")) * popTarget / 1e6;
    const double doctors = (phys.available ? phys.value : m_p("skills.physiciansPer1000")) * popTarget / 1000.0;
    double gradTot = 0; for (double g : stGrad) gradTot += g;
    for (int s = 0; s < ns; ++s) {
        m_w.skills.byState[s][Scientist] = gradTot > 0 ? researchers * stGrad[s] / gradTot : 0;
        m_w.skills.byState[s][Medical] = popTarget > 0 ? doctors * stPop[s] / popTarget : 0;
    }

    // --- technology ---
    for (const auto &dom : m_res->catalog.techDomains()) {
        m_w.techLevel[dom.id] = int(dom.initialLevel);
        m_w.techPoints[dom.id] = dom.initialLevel >= 1 && dom.levels.size() > 1 ? dom.levels[int(dom.initialLevel)].pointsRequired : 0;
    }
    m_w.notify(Notification::Info, QStringLiteral("Scenario '%1' created. Population %2 million (World Bank %3, distributed by Census 2011 district shares). The development layer is empty.")
                                       .arg(st.name).arg(popTarget / 1e6, 0, 'f', 1).arg(popRef.available ? QString::number(popRef.year) : QStringLiteral("estimate")));
    const auto gpc = ref.valueAt("NY.GDP.PCAP.CD", "IND", refYear);
    if (gpc.available) m_w.userSettings["baseline.realGdpPerCapitaCr"] = gpc.value * m_p("macro.inrPerUsd") / 1e7;
    refresh();
    // spin up the air-quality model so the baseline concentration is converged
    {
        auto ctx = context();
        for (int k = 0; k < 8; ++k) m_envEngine.step(ctx);
    }
    refresh();
    recordBaselines();
    refresh();
    recordHistory();
}

void Simulation::recordBaselines()
{
    auto &us = m_w.userSettings;
    const auto &s = m_w.stats;
    us["baseline.gdpPerCapitaCr"] = s.population > 0 ? s.gdpCr / s.population : 0;
    us["baseline.healthIndex"] = 0.5 * std::min(1.0, s.hospitalBedsPer1000 / m_p("health.bedsPer1000Target")) + 0.5 * s.primaryCareCoverage;
    us["baseline.eduIndex"] = s.schoolSeatShare;
    us["baseline.waterIndex"] = 0;
    us["baseline.pm25"] = s.pm25PopWeighted;
    us["baseline.urbanShare"] = s.urbanShare;
}

void Simulation::setWorld(const WorldState &w)
{
    m_w = w;
    m_net = NetworkModel{};
    m_env = EnvironmentGrid{};
    // restore modified parameters stored with the scenario
    for (auto it = m_w.userSettings.begin(); it != m_w.userSettings.end(); ++it)
        if (it.key().startsWith("param.")) m_p.set(it.key().mid(6), it.value());
    refresh();
}

void Simulation::refresh()
{
    auto ctx = context();
    m_policy.computeModifiers(ctx);
    if (m_net.dirty) m_netEngine.rebuild(ctx);
    m_population.step(ctx, false);
    m_economy.prepare(ctx);
    m_util.step(ctx);
    m_economy.finalize(ctx, 0, 0, false);
    m_population.step(ctx, false);
    m_envEngine.step(ctx);
    aggregate();
}

void Simulation::stepMonth()
{
    auto ctx = context();
    m_policy.computeModifiers(ctx);
    double pub = 0, priv = 0;
    m_construction.step(ctx, pub, priv);
    if (m_net.dirty) m_netEngine.rebuild(ctx);
    m_economy.prepare(ctx);
    m_util.step(ctx);
    m_economy.finalize(ctx, pub, priv);
    m_skills.step(ctx);
    m_tech.step(ctx);
    m_population.step(ctx);
    if (!m_fast || m_w.month % 3 == 0) m_envEngine.step(ctx);
    ++m_w.month;
    aggregate();
    recordHistory();
    if (!m_fast) checkAlerts();
}

PlacementResult Simulation::place(const PlacementRequest &req)
{
    auto ctx = context();
    m_policy.computeModifiers(ctx);
    auto r = ConstructionEngine::place(ctx, req);
    return r;
}

bool Simulation::demolish(int uid)
{
    auto ctx = context();
    const bool ok = ConstructionEngine::demolish(ctx, uid);
    if (ok) refresh();
    return ok;
}

void Simulation::enactPolicy(const QString &id, double intensity)
{
    const auto *d = m_res->policies.find(id);
    if (!d) return;
    for (auto &p : m_w.policies)
        if (p.id == id) {
            p.intensity = intensity;
            m_w.notify(Notification::Policy, QStringLiteral("Policy adjusted: %1 (intensity %2%).").arg(d->name).arg(intensity * 100, 0, 'f', 0));
            return;
        }
    m_w.policies.append({id, intensity, m_w.month, -1});
    m_w.notify(Notification::Policy, QStringLiteral("Policy enacted: %1 (intensity %2%). Effects begin after ~%3 months.").arg(d->name).arg(intensity * 100, 0, 'f', 0).arg(d->lagMonths));
    if (d->isCoercive())
        m_w.notify(Notification::Warning, QStringLiteral("Human-rights warning: '%1' is coercive. The model applies social-trust, sex-ratio and exclusion harms.").arg(d->name));
}

void Simulation::repealPolicy(const QString &id)
{
    for (int i = 0; i < m_w.policies.size(); ++i)
        if (m_w.policies[i].id == id) {
            const auto *d = m_res->policies.find(id);
            m_w.policies.remove(i);
            m_w.notify(Notification::Policy, QStringLiteral("Policy repealed: %1.").arg(d ? d->name : id));
            return;
        }
}

bool Simulation::isPolicyActive(const QString &id) const
{
    for (const auto &p : m_w.policies) if (p.id == id) return true;
    return false;
}

double Simulation::developmentScore() const
{
    // Composite 0-100 index with equal weights (modelling choice, not an official index).
    const auto &s = m_w.stats;
    const double pop = std::max(1.0, s.population);
    const double comps[] = {
        s.powerDemandMW > 0 ? s.powerServedMW / s.powerDemandMW : 0,
        s.waterDemandMLD > 0 ? s.waterSuppliedMLD / s.waterDemandMLD : 0,
        std::clamp(1 - s.housingShortagePersons / pop, 0.0, 1.0),
        0.5 * std::min(1.0, s.hospitalBedsPer1000 / 3.0) + 0.5 * s.primaryCareCoverage,
        s.schoolSeatShare,
        std::clamp(std::log(std::max(1.0, s.gdpPerCapitaUsd) / 300.0) / std::log(m_p("score.gdpPerCapitaRef") / 300.0), 0.0, 1.0),
        std::clamp(1 - s.unemploymentRate / 0.2, 0.0, 1.0),
        std::clamp(1 - (s.pm25PopWeighted - 5) / 145.0, 0.0, 1.0),
        std::clamp(s.foodSupplyRatio, 0.0, 1.0),
    };
    double sum = 0; for (double c : comps) sum += c;
    return 100.0 * sum / (sizeof(comps) / sizeof(comps[0]));
}

void Simulation::aggregate()
{
    auto &s = m_w.stats;
    double road = 0, rail = 0, districtsRoad = 0, land = 0;
    for (const auto &d : m_w.districts) { road += d.roadKm; rail += d.railKm; if (d.roadAccess) ++districtsRoad; land += d.landUsedHa; }
    s.roadKm = road; s.railKm = rail; s.districtsWithRoad = districtsRoad; s.landUsedHa = land;
    s.electricityAccess = s.powerDemandMW > 0 ? s.powerServedMW / s.powerDemandMW : 0;
    s.kwhPerCapita = s.population > 0 ? s.generationTWhYr * 1e9 / s.population : 0;
    // literacy proxy: census literacy adjusted by school coverage growth (indicative)
    // regional inequality: Theil index of district GDP per capita
    double tot = 0, pop = 0;
    for (const auto &d : m_w.districts) { tot += d.gdpCr; pop += d.population; }
    double theil = 0;
    const double mean = pop > 0 ? tot / pop : 0;
    if (mean > 0)
        for (const auto &d : m_w.districts) {
            if (d.population <= 0 || d.gdpCr <= 0) continue;
            const double x = (d.gdpCr / d.population) / mean;
            theil += (d.population / pop) * x * std::log(x);
        }
    s.inequalityTheil = theil;
    s.developmentScore = developmentScore();
}

void Simulation::recordHistory()
{
    const auto &s = m_w.stats;
    HistoryPoint h;
    h.month = m_w.month;
    h.v = {{"population", s.population}, {"gdpCr", s.gdpCr}, {"gdpUsd", s.gdpUsd}, {"gdpPerCapitaUsd", s.gdpPerCapitaUsd},
           {"unemploymentRate", s.unemploymentRate}, {"electricityAccess", s.electricityAccess}, {"generationCapacityMW", s.generationCapacityMW},
           {"powerServedMW", s.powerServedMW}, {"powerDemandMW", s.powerDemandMW}, {"renewableShareGen", s.renewableShareGen},
           {"waterServed", s.waterDemandMLD > 0 ? s.waterSuppliedMLD / s.waterDemandMLD : 0}, {"housingShortage", s.housingShortagePersons},
           {"lifeExpectancy", s.lifeExpectancy}, {"tfr", s.tfr}, {"urbanShare", s.urbanShare}, {"pm25", s.pm25PopWeighted}, {"aqi", s.aqiPopWeighted},
           {"co2Mt", s.co2Mt}, {"treasuryCr", m_w.budget.treasuryCr}, {"debtCr", m_w.budget.debtCr}, {"debtToGdp", s.gdpCr > 0 ? m_w.budget.debtCr / s.gdpCr : 0},
           {"developmentScore", s.developmentScore}, {"manufacturingShare", s.manufacturingShare}, {"importDependency", s.importDependency},
           {"foodSupplyRatio", s.foodSupplyRatio}, {"hospitalBedsPer1000", s.hospitalBedsPer1000}, {"schoolSeatShare", s.schoolSeatShare},
           {"dependencyRatio", s.dependencyRatio}, {"exportsCr", s.exportsCr}, {"importsCr", s.importsCr}, {"rndShareGdp", s.rndShareGdp},
           {"roadKm", s.roadKm}, {"railKm", s.railKm}, {"inequality", s.inequalityTheil}, {"researchersPerMillion", s.researchersPerMillion}};
    if (!m_w.history.isEmpty() && m_w.history.last().month == h.month) m_w.history.last() = h;
    else m_w.history << h;
}

void Simulation::checkAlerts()
{
    const auto &s = m_w.stats;
    if (m_w.month % 12 != 0) return; // yearly review
    if (s.powerDemandMW > 0 && s.electricityAccess < 0.5)
        m_w.notify(Notification::Warning, QStringLiteral("Electricity: only %1% of demand is served (%2 GW unserved).").arg(s.electricityAccess * 100, 0, 'f', 0).arg((s.powerDemandMW - s.powerServedMW) / 1000, 0, 'f', 1));
    if (s.unemploymentRate > 0.08)
        m_w.notify(Notification::Economy, QStringLiteral("Unemployment is %1%.").arg(s.unemploymentRate * 100, 0, 'f', 1));
    if (s.foodSupplyRatio < 0.95)
        m_w.notify(Notification::Warning, QStringLiteral("Food security risk: supply covers %1% of requirement.").arg(s.foodSupplyRatio * 100, 0, 'f', 0));
    if (s.gdpCr > 0 && m_w.budget.debtCr / s.gdpCr > 1.0)
        m_w.notify(Notification::Economy, QStringLiteral("Public debt has reached %1% of GDP; interest costs are rising.").arg(m_w.budget.debtCr / s.gdpCr * 100, 0, 'f', 0));
    int severe = 0;
    for (const auto &d : m_w.districts) if (d.aqi > 300) ++severe;
    if (severe > 0) m_w.notify(Notification::Environment, QStringLiteral("Air quality: %1 districts have monthly AQI above 300 (Very Poor/Severe).").arg(severe));
    if (!m_net.unbridgedCrossings.isEmpty())
        m_w.notify(Notification::Warning, QStringLiteral("%1 road/rail river crossings have no bridge; those lines are not connected.").arg(m_net.unbridgedCrossings.size()));
}

} // namespace isim
