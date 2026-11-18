#include "AIContextBuilder.h"
#include "advisor/Advisor.h"
#include "advisor/Suitability.h"
#include "engines/PolicyEngine.h"
#include "scenario/ProjectionRunner.h"
#include "sim/Simulation.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <cmath>

namespace isim {

static double r3(double v) { return std::abs(v) >= 100 ? std::round(v) : std::round(v * 1000) / 1000; }
static QJsonArray arr(const QStringList &l) { QJsonArray a; for (const auto &s : l) a.append(s); return a; }

QString AIContextBuilder::systemPrompt()
{
    return QStringLiteral(
        "You are the analysis assistant of the India National Development & Policy Simulator.\n"
        "Rules:\n"
        "1. The simulator's C++ engine computes all numbers. Use ONLY numbers that appear in CONTEXT. Never invent, estimate or recall statistics.\n"
        "2. If something needed for a judgement is not in CONTEXT, say so in 'missing_assumptions' instead of guessing.\n"
        "3. Projections are uncertain model outputs, not predictions; say so when relevant.\n"
        "4. Respect human rights; do not present coercive policies as acceptable trade-offs.\n"
        "5. Be concise and specific to the CONTEXT.\n"
        "Reply with ONE JSON object and nothing else, with keys:\n"
        "{\"summary\": string, \"key_points\": [string], \"risks\": [string], \"missing_assumptions\": [string], \"alternatives\": [string]}");
}

QString AIContextBuilder::taskPrompt(const QString &task, const QJsonObject &ctx, const QString &q)
{
    static const QHash<QString, QString> tasks = {
        {"explain_facility", "Explain this facility's requirements, why its suitability status is what it is, and which prerequisite investments matter most and why."},
        {"explain_policy", "Explain the likely consequences of this policy using the context: benefits, unintended consequences, who is affected, and implementation challenges."},
        {"explain_recommendation", "Explain why the advisor made this recommendation, what the evidence shows, and what the trade-offs are."},
        {"interpret_report", "Interpret the national simulation report: main strengths, main bottlenecks, and what the trends indicate."},
        {"compare_scenarios", "Summarise the differences between the scenario projections, noting uncertainty bands."},
        {"alternatives", "Suggest alternative development strategies grounded in the context and explain their trade-offs."},
        {"question", "Answer the user's question using only the context."}};
    QString p = tasks.value(task, tasks.value("question"));
    if (!q.isEmpty()) p += "\nUser question: " + q;
    p += "\n\nCONTEXT (JSON, produced by the simulation engine):\n" + QString::fromUtf8(QJsonDocument(ctx).toJson(QJsonDocument::Compact));
    return p;
}

QJsonObject AIContextBuilder::nationalSnapshot(const Simulation &sim)
{
    const auto &w = sim.world();
    const auto &s = w.stats;
    QJsonObject o{{"date", w.dateString()}, {"scenario", w.name},
                  {"note", "Greenfield scenario: infrastructure is built by the planner; population is anchored to real-world data."},
                  {"population_millions", r3(s.population / 1e6)}, {"gdp_crore_inr", r3(s.gdpCr)}, {"gdp_billion_usd", r3(s.gdpUsd / 1e9)},
                  {"gdp_per_capita_usd", r3(s.gdpPerCapitaUsd)}, {"gdp_growth_pct", r3(s.gdpGrowth)}, {"unemployment_pct", r3(s.unemploymentRate * 100)},
                  {"manufacturing_share_pct", r3(s.manufacturingShare * 100)}, {"electricity_demand_mw", r3(s.powerDemandMW)},
                  {"electricity_served_mw", r3(s.powerServedMW)}, {"generation_capacity_mw", r3(s.generationCapacityMW)},
                  {"renewable_share_pct", r3(s.renewableShareGen * 100)}, {"grid_reliability_pct", r3(s.gridReliability * 100)},
                  {"water_served_pct", r3(s.waterDemandMLD > 0 ? s.waterSuppliedMLD / s.waterDemandMLD * 100 : 0)},
                  {"housing_shortage_millions", r3(s.housingShortagePersons / 1e6)}, {"hospital_beds_per_1000", r3(s.hospitalBedsPer1000)},
                  {"primary_care_coverage_pct", r3(s.primaryCareCoverage * 100)}, {"school_seat_coverage_pct", r3(s.schoolSeatShare * 100)},
                  {"food_supply_ratio", r3(s.foodSupplyRatio)}, {"life_expectancy", r3(s.lifeExpectancy)}, {"fertility_rate", r3(s.tfr)},
                  {"dependency_ratio", r3(s.dependencyRatio)}, {"urban_share_pct", r3(s.urbanShare * 100)}, {"pm25_pop_weighted", r3(s.pm25PopWeighted)},
                  {"co2_mt", r3(s.co2Mt)}, {"import_dependency_pct", r3(s.importDependency * 100)}, {"rnd_share_gdp_pct", r3(s.rndShareGdp)},
                  {"treasury_crore", r3(w.budget.treasuryCr)}, {"debt_crore", r3(w.budget.debtCr)}, {"revenue_crore_per_year", r3(w.budget.revenueCrYr)},
                  {"development_score", r3(s.developmentScore)}, {"facilities_placed", w.facilities.size()}};
    QJsonObject tech;
    for (auto it = w.techLevel.begin(); it != w.techLevel.end(); ++it) tech.insert(it.key(), it.value());
    o.insert("technology_levels", tech);
    QJsonArray pol;
    for (const auto &p : w.policies) pol.append(QJsonObject{{"id", p.id}, {"intensity_pct", r3(p.intensity * 100)}});
    o.insert("active_policies", pol);
    // trend over the last 12 months
    if (w.history.size() > 12) {
        const auto &a = w.history[w.history.size() - 13].v, &b = w.history.last().v;
        o.insert("change_last_12_months", QJsonObject{{"gdp_crore", r3(b.value("gdpCr") - a.value("gdpCr"))},
                                                      {"population", r3(b.value("population") - a.value("population"))},
                                                      {"unemployment_pct_points", r3((b.value("unemploymentRate") - a.value("unemploymentRate")) * 100)}});
    }
    return o;
}

QJsonObject AIContextBuilder::facility(const Simulation &sim, const FacilityDefinition &d, const SuitabilityReport *rep)
{
    QJsonObject o{{"facility", d.name}, {"category", d.category}, {"description", d.description}, {"purpose", d.purpose},
                  {"capex_crore", r3(d.capexCr)}, {"unit", d.unitLabel()}, {"construction_months", d.constructionMonths},
                  {"power_demand_mw", r3(d.powerDemandMW)}, {"water_demand_mld", r3(d.waterDemandMLD)},
                  {"operations_jobs", r3(d.operationsJobs.total())}, {"engineers", r3(d.operationsJobs[Engineer])}, {"scientists", r3(d.operationsJobs[Scientist])},
                  {"value_added_crore_per_year", r3(d.valueAddedCrPerYear)}, {"risks", arr(d.risks)},
                  {"parameter_basis", d.basis}, {"parameter_status", "indicative modelling assumptions"}};
    QJsonObject tr;
    for (auto it = d.techRequired.begin(); it != d.techRequired.end(); ++it) tr.insert(it.key(), it.value());
    o.insert("technology_required", tr);
    QJsonObject tl;
    for (auto it = sim.world().techLevel.begin(); it != sim.world().techLevel.end(); ++it) if (d.techRequired.contains(it.key())) tl.insert(it.key(), it.value());
    o.insert("technology_current", tl);
    if (rep) {
        QJsonArray checks;
        for (const auto &c : rep->checks) checks.append(QJsonObject{{"area", c.area}, {"level", int(c.level)}, {"detail", c.detail}});
        QJsonArray pre;
        for (const auto &p : rep->prerequisites) pre.append(QJsonObject{{"investment", p.title}, {"why", p.why}});
        o.insert("suitability", QJsonObject{{"location", rep->location}, {"status", rep->statusText}, {"expected_output_pct", r3(rep->estimatedEfficiency * 100)},
                                            {"reasons", arr(rep->reasons)}, {"checks", checks}, {"prerequisites", pre}});
    }
    return o;
}

QJsonObject AIContextBuilder::policy(const Simulation &sim, const PolicyDefinition &p)
{
    QJsonArray eff;
    for (const auto &e : p.effects) eff.append(QJsonObject{{"variable", e.key}, {"mode", e.mode}, {"value_at_full_strength", r3(e.full)}});
    const double gdp = sim.world().stats.gdpCr;
    return QJsonObject{{"policy", p.name}, {"category", p.category}, {"description", p.description}, {"intensity_definition", p.intensityLabel},
                       {"annual_cost_crore_at_full", r3(p.costCrPerYearAtFull + p.costPctGdpAtFull * gdp)},
                       {"effectiveness_range_pct", QJsonArray{r3(p.effLow * 100), r3(p.effCentral * 100), r3(p.effHigh * 100)}},
                       {"lag_months", p.lagMonths}, {"ramp_months", p.rampMonths}, {"model_effects", eff},
                       {"benefits", arr(p.benefits)}, {"unintended_consequences", arr(p.unintended)}, {"affected_groups", arr(p.affectedGroups)},
                       {"state_differences", p.stateDifferences}, {"implementation_challenges", arr(p.challenges)}, {"uncertainty", p.uncertainty},
                       {"human_rights_concern", p.humanRightsConcern}, {"national_snapshot", nationalSnapshot(sim)}};
}

QJsonObject AIContextBuilder::recommendation(const Simulation &sim, const Recommendation &r)
{
    QJsonArray ev;
    for (const auto &e : r.evidence) ev.append(QJsonObject{{"label", e.label}, {"value", e.value}, {"source", e.source}});
    return QJsonObject{{"recommendation", r.title}, {"category", r.category}, {"why", r.why}, {"investment_crore", r3(r.investmentCr)},
                       {"quantity", r.quantity}, {"locations", arr(r.locations)}, {"benefits", arr(r.benefits)}, {"drawbacks", arr(r.drawbacks)},
                       {"risks", arr(r.risks)}, {"alternatives", arr(r.alternatives)}, {"opportunity_cost", r.opportunityCost},
                       {"evidence", ev}, {"national_snapshot", nationalSnapshot(sim)}};
}

QJsonObject AIContextBuilder::projections(const QVector<ProjectionResult> &runs)
{
    QJsonArray a;
    for (const auto &p : runs) {
        QJsonObject series;
        for (const auto &s : p.series) {
            if (s.years.isEmpty()) continue;
            QJsonObject pts;
            for (int i = 0; i < s.years.size(); ++i) {
                if (i != s.years.size() - 1 && (i + 1) % 5 != 0 && i != 0) continue; // keep the context small
                pts.insert(QString::number(s.years[i]), QJsonArray{r3(s.p10[i]), r3(s.p50[i]), r3(s.p90[i])});
            }
            series.insert(s.key, pts);
        }
        a.append(QJsonObject{{"scenario", p.scenarioName}, {"members", p.members}, {"values_p10_p50_p90", series}});
    }
    return QJsonObject{{"projections", a}, {"note", "Bands are 10th-90th percentiles under parameter uncertainty only."}};
}

} // namespace isim
