#include <functional>
#include "Advisor.h"
#include "catalog/Catalog.h"
#include "comparison/ReferenceData.h"
#include "engines/EconomyEngine.h"
#include "engines/EnvironmentEngine.h"
#include "engines/PopulationEngine.h"
#include "engines/PolicyEngine.h"
#include "gis/GeoDatabase.h"
#include "gis/GeoMath.h"
#include <QLocale>
#include <algorithm>
#include <cmath>

namespace isim {

namespace {
QString num(double v, int prec = 0) { return QLocale(QLocale::English).toString(v, 'f', prec); }
QString cr(double v)
{
    if (std::abs(v) >= 100000) return QStringLiteral("₹%1 lakh crore").arg(v / 100000, 0, 'f', 2);
    return QStringLiteral("₹%1 crore").arg(num(v));
}
QString pct(double f, int prec = 0) { return QString::number(f * 100, 'f', prec) + "%"; }

struct Helper {
    SimContext &ctx;
    const ReferenceData *ref;
    QVector<Recommendation> out;

    const FacilityDefinition *def(const QString &id) const { return ctx.cat.facility(id); }
    QString dname(int di) const { const auto &d = ctx.geo.districts()[di]; return d.name + ", " + d.stateName; }

    // top-N districts by a score (descending), skipping zero scores
    QVector<int> topDistricts(const std::function<double(int)> &score, int n) const
    {
        QVector<QPair<double, int>> v;
        for (int i = 0; i < ctx.w.districts.size(); ++i) { const double s = score(i); if (s > 0) v.append({s, i}); }
        std::sort(v.begin(), v.end(), [](auto &a, auto &b) { return a.first > b.first; });
        QVector<int> r;
        for (int i = 0; i < v.size() && r.size() < n; ++i) r << v[i].second;
        return r;
    }
    // choose a district among candidates where the facility is physically feasible
    int feasibleIn(const QVector<int> &cands, const FacilityDefinition &d, QString *note = nullptr)
    {
        for (int di : cands) {
            const auto rep = SuitabilityEngine::assess(ctx, d, ctx.geo.districts()[di].centroid);
            if (rep.status != SuitabilityStatus::NotFeasible) { if (note) *note = rep.statusText; return di; }
        }
        return cands.isEmpty() ? -1 : cands.first();
    }
    QString opportunity(double cost) const
    {
        const double rev = ctx.w.budget.revenueCrYr;
        const auto *phc = def("phc"), *school = def("primary_school");
        QString s = QStringLiteral("%1 is %2 of annual government revenue").arg(cr(cost)).arg(rev > 0 ? pct(cost / rev, 1) : QStringLiteral("n/a"));
        if (phc && school) s += QStringLiteral("; the same money could build about %1 primary health centres or %2 primary schools.").arg(num(cost / phc->capexCr)).arg(num(cost / school->capexCr));
        return s;
    }
    Recommendation &add(const QString &id, const QString &cat, const QString &title, int prio)
    {
        Recommendation r;
        r.id = id; r.category = cat; r.title = title; r.priority = std::clamp(prio, 0, 100);
        r.decision = ctx.w.advisorDecisions.value(id);
        out << r;
        return out.last();
    }
    void sim(Recommendation &r, const QString &label, const QString &value) { r.evidence.append({label, value, QStringLiteral("Simulated (%1)").arg(ctx.w.dateString())}); }
};
} // namespace

QVector<Recommendation> Advisor::analyse(SimContext &ctx, const ReferenceData *ref)
{
    Helper h{ctx, ref, {}};
    auto &w = ctx.w;
    const auto &s = w.stats;
    const auto &geo = ctx.geo;
    const int nd = w.districts.size();

    // ================= 1. Electricity generation =================
    const double unserved = std::max(0.0, s.powerDemandMW - s.powerServedMW);
    if (s.powerDemandMW > 0 && unserved / s.powerDemandMW > 0.05) {
        // largest unserved demand among high-solar states vs elsewhere
        const auto cands = h.topDistricts([&](int i) { return w.districts[i].powerDemandMW - w.districts[i].powerServedMW; }, 8);
        const bool renewableLow = s.renewableShareGen < 0.4;
        const auto *solar = h.def("solar_farm"), *coal = h.def("coal_plant_fgd");
        const auto *pick = renewableLow && solar ? solar : coal;
        QString note;
        const int di = h.feasibleIn(cands, *pick, &note);
        const double perUnit = pick->generationMW * pick->capacityFactor * (1 - ctx.p("power.transmissionLoss"));
        const int needed = std::max(1, int(std::ceil(unserved / perUnit)));
        const int n = std::min(needed, 10); // first phase
        auto &r = h.add("power.generation", "Electricity", QStringLiteral("Phase 1: build %1 x %2 (of ≈%3 needed to close the deficit)").arg(n).arg(pick->name).arg(needed), 95);
        r.facilityId = pick->id; r.quantity = n; r.targetDistrict = di;
        r.investmentCr = n * pick->capexCr;
        r.why = QStringLiteral("Only %1 of electricity demand is served; %2 GW is unserved. Electricity is a prerequisite for every factory, hospital, data centre and household productivity gain in the model.")
                    .arg(pct(s.electricityAccess)).arg(unserved / 1000, 0, 'f', 1);
        if (di >= 0) r.locations << QStringLiteral("%1 — largest unserved demand among feasible sites (%2)").arg(h.dname(di), note);
        for (int k = 0; k < std::min<int>(3, cands.size()); ++k) if (cands[k] != di) r.locations << QStringLiteral("%1 — unserved %2 MW").arg(h.dname(cands[k])).arg(w.districts[cands[k]].powerDemandMW - w.districts[cands[k]].powerServedMW, 0, 'f', 0);
        r.supportingInfrastructure << "Substations and district distribution networks in the districts to be served" << "Transmission lines linking the plant to load centres";
        if (pick == solar) r.supportingInfrastructure << "Battery or pumped-hydro storage to firm variable output";
        else r.supportingInfrastructure << "Rail link for coal and limestone deliveries";
        r.benefits << QStringLiteral("≈%1 MW of average delivered power per plant").arg(perUnit, 0, 'f', 0) << "Enables industrial facilities that are currently idle for lack of power" << "Raises legacy-economy productivity in served districts";
        if (pick == solar) { r.drawbacks << "Variable output lowers reliability without storage" << "Large land requirement (~2 ha/MW)"; r.alternatives << "Coal plant with FGD (firm but high CO₂)" << "Nuclear PHWR (firm, low-carbon, long lead time, needs nuclear maturity)" << "Wind farms in high-wind states"; }
        else { r.drawbacks << "High CO₂ emissions (~0.95 t/MWh)" << "SO₂/NOx/PM air pollution near the plant"; r.alternatives << "Solar farms with storage" << "Nuclear power" << "Hydro in Himalayan states"; }
        r.opportunityCost = h.opportunity(r.investmentCr);
        r.shortTerm = QStringLiteral("Construction takes ~%1 months; no supply until commissioning.").arg(pick->constructionMonths);
        r.longTerm = "Reliable power raises output of every grid-connected facility and household productivity.";
        r.risks << "Construction delays" << "Grid connections not built in time";
        r.risks << pick->risks;
        h.sim(r, "Electricity demand", num(s.powerDemandMW) + " MW");
        h.sim(r, "Demand served", num(s.powerServedMW) + " MW");
        h.sim(r, "Renewable share of generation", pct(s.renewableShareGen));
    }

    // ================= 2. Grid connection of districts =================
    {
        const auto cands = h.topDistricts([&](int i) { return ctx.net.powerIslandOfDistrict.value(i, -1) < 0 ? w.districts[i].powerDemandMW : 0; }, 5);
        if (!cands.isEmpty() && s.generationCapacityMW > 0) {
            const auto *sub = h.def("distribution_network");
            auto &r = h.add("power.grid." + QString::number(cands.first()), "Electricity", QStringLiteral("Connect %1 to the grid").arg(h.dname(cands.first())), 80);
            r.facilityId = "distribution_network"; r.targetDistrict = cands.first();
            r.investmentCr = sub->capexCr + h.def("substation")->capexCr;
            r.why = QStringLiteral("%1 has %2 MW of demand but no substation or distribution network, so none of the generation capacity (%3 MW) can reach it.")
                        .arg(h.dname(cands.first())).arg(w.districts[cands.first()].powerDemandMW, 0, 'f', 0).arg(s.generationCapacityMW, 0, 'f', 0);
            for (int di : cands) r.locations << QStringLiteral("%1 — %2 MW unconnected demand").arg(h.dname(di)).arg(w.districts[di].powerDemandMW, 0, 'f', 0);
            r.supportingInfrastructure << "Substation (400/220 kV)" << "Transmission line to the nearest generating island";
            r.benefits << "Brings electricity to households and enterprises" << "Allows new industry in the district";
            r.drawbacks << "Adds demand to an island that may already be short of supply";
            r.alternatives << "Off-grid rooftop solar programme (no grid needed, but lower reliability)";
            r.opportunityCost = h.opportunity(r.investmentCr);
            r.shortTerm = "Commissioned in ~2 years."; r.longTerm = "Productivity uplift for the district's legacy economy.";
            r.risks << "Supply deficit in the connected island";
            h.sim(r, "Unconnected demand", num(w.districts[cands.first()].powerDemandMW) + " MW");
        }
    }

    // ================= 3. Storage / reliability =================
    if (s.renewableShareGen > 0.35 && s.gridReliability < 0.95) {
        const auto *b = h.def("battery_storage");
        int di = -1; double best = 0;
        for (const auto &f : w.facilities) { const auto *d = ctx.def(f); if (d && d->tags.contains("variable") && f.isOperating() && d->generationMW > best) { best = d->generationMW; di = f.district; } }
        auto &r = h.add("power.storage", "Electricity", "Add grid-scale storage to firm variable renewables", 70);
        r.facilityId = "battery_storage"; r.targetDistrict = di; r.quantity = std::max(1, int(s.renewableCapacityMW * 0.2 / 250)); r.investmentCr = r.quantity * b->capexCr;
        r.why = QStringLiteral("Variable renewables provide %1 of generation and grid reliability is %2; reliability-sensitive facilities (fabs, data centres) lose output below their thresholds.").arg(pct(s.renewableShareGen)).arg(pct(s.gridReliability, 1));
        if (di >= 0) r.locations << h.dname(di) + " — next to the largest variable-renewable plant";
        r.benefits << "Higher reliability" << "Allows more solar/wind without curtailment";
        r.drawbacks << "Batteries degrade over ~10-15 years" << "Imported cell dependence until a gigafactory exists";
        r.alternatives << "Pumped hydro (mountain states)" << "Gas combined-cycle plants for flexibility";
        r.opportunityCost = h.opportunity(r.investmentCr);
        r.shortTerm = "Fast to build (~12 months)."; r.longTerm = "Enables a low-carbon grid.";
        r.risks << "Cell price volatility";
        h.sim(r, "Grid reliability", pct(s.gridReliability, 1));
    }

    // ================= 4. Housing =================
    if (s.housingShortagePersons > s.population * 0.01) {
        const auto cands = h.topDistricts([&](int i) { const auto &d = w.districts[i]; return d.population - d.legacyHousing - d.modernHousing; }, 5);
        if (!cands.isEmpty()) {
            const auto *ah = h.def("affordable_housing");
            const double gap = w.districts[cands.first()].population - w.districts[cands.first()].legacyHousing - w.districts[cands.first()].modernHousing;
            const int n = std::max(1, int(std::ceil(gap / ah->housingPersons)));
            auto &r = h.add("housing." + QString::number(cands.first()), "Housing", QStringLiteral("Affordable housing in %1").arg(h.dname(cands.first())), 75);
            r.facilityId = "affordable_housing"; r.quantity = std::min(n, 50); r.targetDistrict = cands.first(); r.investmentCr = r.quantity * ah->capexCr;
            r.why = QStringLiteral("National housing shortage is %1 people (%2 of population), caused by population growth and deterioration of the legacy housing stock. %3 has the largest gap (%4 people).")
                        .arg(num(s.housingShortagePersons)).arg(pct(s.housingShortagePersons / s.population, 1)).arg(h.dname(cands.first())).arg(num(gap));
            for (int di : cands) r.locations << QStringLiteral("%1 — shortage %2 people").arg(h.dname(di)).arg(num(w.districts[di].population - w.districts[di].legacyHousing - w.districts[di].modernHousing));
            r.supportingInfrastructure << "Water treatment and sewage treatment capacity (each 2,000-unit project needs ≈1.2 MLD)" << "Road access and public transport" << "Primary schools and a PHC nearby";
            r.benefits << QStringLiteral("Houses ≈%1 people per project").arg(num(ah->housingPersons)) << "Construction jobs";
            r.drawbacks << "Raises water, electricity, sewage and waste demand" << "Peripheral sites can increase commuting";
            r.alternatives << "Village development clusters (rural)" << "Rental-housing & land-use reform policy (cheaper per person, slower)";
            r.policyId = "affordable_housing_policy";
            r.opportunityCost = h.opportunity(r.investmentCr);
            r.shortTerm = "Two-year construction; temporary construction employment."; r.longTerm = "Reduces overcrowding; supports urbanisation.";
            r.risks << "Under-occupancy if jobs are far away";
            h.sim(r, "Housing shortage", num(s.housingShortagePersons) + " people");
        }
    }

    // ================= 5. Water =================
    if (s.waterDemandMLD > 0 && s.waterSuppliedMLD / s.waterDemandMLD < 0.8) {
        const auto cands = h.topDistricts([&](int i) { const auto &d = w.districts[i]; return d.urbanPopulation * (1 - (d.waterDemandMLD > 0 ? d.waterSuppliedMLD / d.waterDemandMLD : 0)); }, 6);
        if (!cands.isEmpty()) {
            const int di = cands.first();
            const bool coastal = geo.distanceToCoastKm(geo.districts()[di].centroid) < 30;
            const bool river = geo.distanceToRiverKm(geo.districts()[di].centroid) < 40;
            const auto *pick = h.def(river || !coastal ? "water_treatment" : "desalination");
            const double gap = w.districts[di].waterDemandMLD - w.districts[di].waterSuppliedMLD;
            const int n = std::max(1, int(std::ceil(gap / pick->waterSupplyMLD)));
            auto &r = h.add("water." + QString::number(di), "Water", QStringLiteral("Treated water supply for %1").arg(h.dname(di)), 72);
            r.facilityId = pick->id; r.quantity = std::min(n, 10); r.targetDistrict = di; r.investmentCr = r.quantity * pick->capexCr;
            r.why = QStringLiteral("Piped treated water covers %1 of national demand (CPHEEO 135 lpcd urban / JJM 55 lpcd rural norms). %2 has the largest urban population without supply (gap %3 MLD).")
                        .arg(pct(s.waterSuppliedMLD / s.waterDemandMLD)).arg(h.dname(di)).arg(gap, 0, 'f', 0);
            for (int c : cands) r.locations << QStringLiteral("%1 — %2 MLD unmet").arg(h.dname(c)).arg(w.districts[c].waterDemandMLD - w.districts[c].waterSuppliedMLD, 0, 'f', 0);
            r.supportingInfrastructure << "Bulk water pipelines to neighbouring districts" << "Sewage treatment plants (80% of supplied water returns as sewage)" << "Electricity supply";
            r.benefits << "Health gains (life expectancy in the model rises with water & sanitation coverage)" << "Water for industry";
            r.drawbacks << (pick->id == "desalination" ? "High energy use and brine discharge" : "Depends on river flow reliability");
            r.alternatives << (pick->id == "desalination" ? "River-based treatment plant with pipeline" : "Desalination (coastal districts)") << "Demand management / leakage reduction (not modelled)";
            r.opportunityCost = h.opportunity(r.investmentCr);
            r.shortTerm = "Commissioned in 2-2.5 years."; r.longTerm = "Lower water-borne disease; enables industry.";
            r.risks << "Raw-water availability in drought years";
            h.sim(r, "Water demand served", pct(s.waterSuppliedMLD / s.waterDemandMLD));
        }
    }

    // ================= 6. Sanitation =================
    {
        double sew = 0, treated = 0; int worst = -1; double worstUntreated = 0;
        for (int i = 0; i < nd; ++i) { const auto &d = w.districts[i]; sew += d.sewageMLD; treated += d.sewageTreatedMLD; if (d.sewageMLD - d.sewageTreatedMLD > worstUntreated) { worstUntreated = d.sewageMLD - d.sewageTreatedMLD; worst = i; } }
        if (sew > 50 && treated / sew < 0.7 && worst >= 0) {
            const auto *stp = h.def("sewage_treatment");
            auto &r = h.add("sewage." + QString::number(worst), "Sanitation", QStringLiteral("Sewage treatment in %1").arg(h.dname(worst)), 60);
            r.facilityId = "sewage_treatment"; r.targetDistrict = worst; r.quantity = std::max(1, int(std::ceil(worstUntreated / 100))); r.investmentCr = r.quantity * stp->capexCr;
            r.why = QStringLiteral("Only %1 of sewage from piped supply is treated; %2 discharges %3 MLD untreated.").arg(pct(treated / sew)).arg(h.dname(worst)).arg(worstUntreated, 0, 'f', 0);
            r.locations << h.dname(worst);
            r.benefits << "Cleaner rivers and lower disease burden";
            r.drawbacks << "Operating costs; needs reliable electricity";
            r.alternatives << "Decentralised treatment (not modelled)";
            r.opportunityCost = h.opportunity(r.investmentCr);
            r.shortTerm = "Two-year build."; r.longTerm = "Improves the water & sanitation index that drives life expectancy.";
            r.risks << "Plants underused without sewer networks";
            h.sim(r, "Sewage treated", pct(treated / sew));
        }
    }

    // ================= 7. Healthcare =================
    if (s.primaryCareCoverage < 0.9) {
        const auto cands = h.topDistricts([&](int i) { return std::max(0.0, w.districts[i].population - w.districts[i].primaryCareCoverage); }, 5);
        if (!cands.isEmpty()) {
            const int di = cands.first();
            const auto *phc = h.def("phc");
            const double gap = w.districts[di].population - w.districts[di].primaryCareCoverage;
            const int n = std::max(1, int(std::ceil(gap / ctx.p("health.phcPopulationNorm"))));
            auto &r = h.add("health.phc." + QString::number(di), "Healthcare", QStringLiteral("Primary health centres for %1").arg(h.dname(di)), 78);
            r.facilityId = "phc"; r.quantity = n; r.targetDistrict = di; r.investmentCr = n * phc->capexCr;
            r.why = QStringLiteral("Primary-care coverage is %1 nationally against the IPHS norm of one PHC per %2 people. %3 has %4 people without a modelled PHC/CHC.")
                        .arg(pct(s.primaryCareCoverage)).arg(num(ctx.p("health.phcPopulationNorm"))).arg(h.dname(di)).arg(num(gap));
            for (int c : cands) r.locations << QStringLiteral("%1 — %2 people uncovered").arg(h.dname(c)).arg(num(std::max(0.0, w.districts[c].population - w.districts[c].primaryCareCoverage)));
            r.supportingInfrastructure << "Medical colleges / nursing schools to staff them (medical workforce)" << "Electricity and water at each centre" << "Rural roads for access";
            r.benefits << "Life expectancy rises with the health-service index" << "Maternal and child health";
            r.drawbacks << "Each PHC needs doctors; shortages reduce effectiveness";
            r.alternatives << "Community health centres (fewer, larger)" << "Universal health coverage policy (financing rather than buildings)";
            r.policyId = "health_spending";
            r.opportunityCost = h.opportunity(r.investmentCr);
            r.shortTerm = "Cheap and quick (12 months)."; r.longTerm = "Healthier workforce.";
            r.risks << "Staff vacancies";
            h.sim(r, "Primary care coverage", pct(s.primaryCareCoverage));
        }
    }
    if (s.hospitalBedsPer1000 < 1.5) {
        const auto cands = h.topDistricts([&](int i) { return w.districts[i].population - w.districts[i].hospitalBeds * 700; }, 4);
        if (!cands.isEmpty()) {
            const auto *hosp = h.def("district_hospital");
            auto &r = h.add("health.beds", "Healthcare", QStringLiteral("District hospitals (beds: %1 per 1,000)").arg(s.hospitalBedsPer1000, 0, 'f', 2), 65);
            r.facilityId = "district_hospital"; r.targetDistrict = cands.first(); r.quantity = 3; r.investmentCr = 3 * hosp->capexCr;
            r.why = QStringLiteral("Hospital beds are %1 per 1,000 people (benchmark used in the model: %2). India's real-world value (World Bank) is shown for reference.").arg(s.hospitalBedsPer1000, 0, 'f', 2).arg(ctx.p("health.bedsPer1000Target"), 0, 'f', 1);
            for (int c : cands) r.locations << h.dname(c) + QStringLiteral(" — population %1 lakh").arg(w.districts[c].population / 1e5, 0, 'f', 1);
            r.supportingInfrastructure << "Medical college for doctors" << "Reliable electricity and water";
            r.benefits << "Secondary care and emergency capacity";
            r.drawbacks << "High operating cost (₹60 crore/yr each)";
            r.alternatives << "Medical colleges (beds + doctors)" << "Tertiary hospitals in state capitals";
            r.opportunityCost = h.opportunity(r.investmentCr);
            r.shortTerm = "2.5-year construction."; r.longTerm = "Raises life expectancy via the health index.";
            r.risks << "Doctor shortages";
            if (ref) { const auto v = ref->valueAt("SH.MED.BEDS.ZS", "IND", int(ctx.p("scenario.referenceYear")), 8); if (v.available) r.evidence.append({"Real India hospital beds per 1,000", QString::number(v.value, 'f', 2), QStringLiteral("World Bank WDI (%1)").arg(v.year)}); }
            h.sim(r, "Simulated beds per 1,000", QString::number(s.hospitalBedsPer1000, 'f', 2));
        }
    }

    // ================= 8. Education =================
    if (s.schoolSeatShare < 0.9) {
        const auto cands = h.topDistricts([&](int i) { return w.districts[i].population * 0.22 - w.districts[i].schoolSeats; }, 5);
        if (!cands.isEmpty()) {
            const int di = cands.first();
            const auto *ps = h.def("primary_school"), *ss = h.def("secondary_school");
            const double gap = w.districts[di].population * 0.22 - w.districts[di].schoolSeats;
            const int n = std::max(1, int(gap / (ps->schoolSeats + ss->schoolSeats)));
            auto &r = h.add("edu.schools." + QString::number(di), "Education", QStringLiteral("Schools for %1").arg(h.dname(di)), 74);
            r.facilityId = "primary_school"; r.quantity = n; r.targetDistrict = di; r.investmentCr = n * (ps->capexCr + ss->capexCr);
            r.why = QStringLiteral("Modelled school seats cover %1 of the school-age population. Schooling raises the skilled workforce and lowers fertility over time. %2 has the largest gap (≈%3 children).")
                        .arg(pct(s.schoolSeatShare)).arg(h.dname(di)).arg(num(gap));
            for (int c : cands) r.locations << h.dname(c);
            r.supportingInfrastructure << QStringLiteral("%1 secondary schools alongside").arg(n) << "Teacher training (skilled workforce)" << "Rural roads";
            r.benefits << "Future skilled workforce" << "Lower fertility through education (model effect)";
            r.drawbacks << "Benefits arrive after 10-20 years" << "Recurring teacher salaries";
            r.alternatives << "Education spending policy (quality over buildings)";
            r.policyId = "education_spending";
            r.opportunityCost = h.opportunity(r.investmentCr);
            r.shortTerm = "Low visible return."; r.longTerm = "Productivity and human-development gains.";
            r.risks << "Teacher shortages";
            h.sim(r, "School seat coverage", pct(s.schoolSeatShare));
        }
    }

    // ================= 9. Workforce bottlenecks from facilities =================
    {
        QHash<QString, int> causes; QHash<QString, int> causeState;
        for (const auto &f : w.facilities) {
            if (!f.isOperating()) continue;
            for (const auto &lf : f.limitingFactors) if (lf.startsWith("Workforce shortage")) {
                const auto *d = ctx.def(f);
                for (int k = Engineer; k <= Medical; ++k) if (d && d->operationsJobs[k] > 0) { causes[SuitabilityEngine::facilityForSkill(k)]++; causeState[SuitabilityEngine::facilityForSkill(k)] = geo.districts()[f.district].stateIndex; }
            }
        }
        for (auto it = causes.begin(); it != causes.end(); ++it) {
            const auto *fd = h.def(it.key());
            if (!fd) continue;
            const int st = causeState.value(it.key());
            auto &r = h.add("skills." + it.key(), "Education", QStringLiteral("%1 to relieve workforce shortages").arg(fd->name), 82);
            r.facilityId = it.key(); r.quantity = std::min(5, it.value()); r.investmentCr = r.quantity * fd->capexCr;
            if (st >= 0 && !geo.states()[st].districts.isEmpty()) { r.targetDistrict = geo.states()[st].districts.first(); r.locations << geo.states()[st].name + " — where under-staffed facilities are located"; }
            r.why = QStringLiteral("%1 operating facilities are under-staffed for lack of specialised workers. Facilities cannot reach nameplate output without them.").arg(it.value());
            r.benefits << "Raises output of existing facilities" << "Lower brain drain risk when graduates find jobs";
            r.drawbacks << "4-6 year lag before graduates arrive";
            r.alternatives << "Skilled immigration & diaspora return policy (faster, smaller)" << "Skills mission policy";
            r.policyId = "skilled_immigration";
            r.opportunityCost = h.opportunity(r.investmentCr);
            r.shortTerm = "Construction 3 years; first graduates later."; r.longTerm = "Durable talent base.";
            r.risks << "Graduates may emigrate if jobs are scarce";
            h.sim(r, "Under-staffed facilities", QString::number(it.value()));
        }
    }

    // ================= 10. Technology dependencies =================
    {
        QHash<QString, int> blocked;
        for (const auto &f : w.facilities) {
            const auto *d = ctx.def(f);
            if (!d || f.status == FacilityStatus::Halted) continue;
            for (auto it = d->techRequired.begin(); it != d->techRequired.end(); ++it)
                if (w.techLevel.value(it.key()) < it.value()) blocked[it.key()]++;
        }
        for (auto it = blocked.begin(); it != blocked.end(); ++it) {
            const auto *dom = ctx.cat.techDomain(it.key());
            const auto ids = SuitabilityEngine::facilitiesForTechDomain(ctx, it.key(), 2);
            if (!dom || ids.isEmpty()) continue;
            const auto *fd = h.def(ids.first());
            auto &r = h.add("tech." + it.key(), "Technology", QStringLiteral("Build %1 capability: %2").arg(dom->name, fd->name), 85);
            r.facilityId = fd->id; r.investmentCr = fd->capexCr;
            QStringList blockers;
            TechnologyEngine::canAdvance(ctx, it.key(), &blockers);
            r.why = QStringLiteral("%1 facilities depend on %2 maturity above the current level %3, so they produce little or nothing. Next level needs: %4.")
                        .arg(it.value()).arg(dom->name).arg(w.techLevel.value(it.key())).arg(blockers.join("; "));
            r.supportingInfrastructure << "Engineering colleges and research universities (scientists & engineers)" << "Reliable power and fibre connectivity";
            for (const auto &x : ids) if (x != fd->id) r.alternatives << h.def(x)->name;
            r.alternatives << "Policy: national research foundation / sector R&D incentives";
            r.benefits << QStringLiteral("+%1 research points/yr in %2").arg(fd->researchPoints.value(it.key()), 0, 'f', 0).arg(dom->name) << "Unlocks the output of blocked facilities once maturity rises";
            r.drawbacks << "Research results are slow and uncertain";
            r.opportunityCost = h.opportunity(r.investmentCr);
            r.shortTerm = "No output gains for several years."; r.longTerm = "Technology sovereignty and higher-value industry.";
            r.risks << "Talent retention" << "Maturity thresholds are modelling assumptions";
            h.sim(r, dom->name + " level", QString::number(w.techLevel.value(it.key())));
            h.sim(r, "Research points", num(w.techPoints.value(it.key())));
        }
    }

    // ================= 11. Transport connectivity =================
    {
        int unconnected = 0; int sample = -1;
        for (const auto &f : w.facilities) {
            const auto *d = ctx.def(f);
            if (!d || d->isLine() || f.status == FacilityStatus::Halted) continue;
            if (f.transportScore < 1.0 && f.isOperating()) { ++unconnected; if (sample < 0) sample = f.district; }
        }
        if (unconnected > 0) {
            const auto *nh = h.def("national_highway");
            auto &r = h.add("transport.connect", "Transport", QStringLiteral("Connect %1 facilities to the transport network").arg(unconnected), 80);
            r.facilityId = "national_highway"; r.targetDistrict = sample; r.quantity = 100; r.investmentCr = 100 * nh->capexCr;
            r.why = QStringLiteral("%1 operating facilities lack required road/rail/port/air/fibre access; their output is reduced (efficiency scales with the square root of the share of transport needs met).").arg(unconnected);
            if (sample >= 0) r.locations << h.dname(sample);
            r.supportingInfrastructure << "Bridges at river crossings" << "Railway stations / freight terminals for rail access";
            r.benefits << "Restores output of stranded facilities" << "Market access for districts along the route";
            r.drawbacks << "Land acquisition" << "Traffic emissions";
            r.alternatives << "State highways (cheaper, lower capacity)" << "Rail freight corridors for bulk goods";
            r.opportunityCost = h.opportunity(r.investmentCr);
            r.shortTerm = "≈2 years per segment."; r.longTerm = "Integrated national market.";
            r.risks << "Unbridged river crossings break connectivity";
            h.sim(r, "Facilities without required access", QString::number(unconnected));
        }
        if (!ctx.net.unbridgedCrossings.isEmpty()) {
            const auto c = ctx.net.unbridgedCrossings.first().second;
            auto &r = h.add("transport.bridges", "Transport", QStringLiteral("Build %1 bridge(s) at river crossings").arg(ctx.net.unbridgedCrossings.size()), 88);
            r.facilityId = "bridge"; r.quantity = ctx.net.unbridgedCrossings.size(); r.investmentCr = r.quantity * h.def("bridge")->capexCr;
            r.targetDistrict = geo.districtAt(c);
            r.why = "Roads or railways cross mapped rivers without a bridge. Those lines are treated as broken and provide no connectivity.";
            for (const auto &x : ctx.net.unbridgedCrossings) { const int di = geo.districtAt(x.second); if (di >= 0) r.locations << QStringLiteral("%1 (%2°E, %3°N)").arg(h.dname(di)).arg(x.second.x(), 0, 'f', 2).arg(x.second.y(), 0, 'f', 2); if (r.locations.size() > 5) break; }
            r.benefits << "Restores the full value of the existing road/rail investment";
            r.drawbacks << "Flood and scour risk";
            r.alternatives << "Re-route the line to avoid the crossing";
            r.opportunityCost = h.opportunity(r.investmentCr);
            r.shortTerm = "≈30 months."; r.longTerm = "Network integrity.";
            r.risks << "Seismic zones need special design";
            h.sim(r, "Unbridged crossings", QString::number(ctx.net.unbridgedCrossings.size()));
        }
        const auto big = h.topDistricts([&](int i) { return w.districts[i].roadAccess ? 0 : w.districts[i].population; }, 5);
        if (!big.isEmpty() && w.districts[big.first()].population > 1e6) {
            const auto *nh = h.def("national_highway");
            auto &r = h.add("transport.district." + QString::number(big.first()), "Transport", QStringLiteral("Road access for %1").arg(h.dname(big.first())), 62);
            r.facilityId = "national_highway"; r.targetDistrict = big.first(); r.quantity = 80; r.investmentCr = 80 * nh->capexCr;
            r.why = QStringLiteral("%1 (population %2 lakh) has no modelled road. Legacy productivity in the model rises with transport access (up to +%3).").arg(h.dname(big.first())).arg(w.districts[big.first()].population / 1e5, 0, 'f', 1).arg(pct(ctx.p("macro.legacyAccessBoost")));
            for (int di : big) r.locations << QStringLiteral("%1 — %2 lakh people without road access").arg(h.dname(di)).arg(w.districts[di].population / 1e5, 0, 'f', 1);
            r.benefits << "Market access" << "Enables factories and services";
            r.drawbacks << "Land take";
            r.alternatives << "District roads (cheaper)" << "Railway line";
            r.opportunityCost = h.opportunity(r.investmentCr);
            r.shortTerm = "≈2 years."; r.longTerm = "Durable productivity gains.";
            r.risks << "Isolated segments give fewer benefits";
        }
    }

    // ================= 12. Food security =================
    if (s.foodSupplyRatio < 1.0) {
        const auto cands = h.topDistricts([&](int i) { return w.districts[i].population * ctx.p("food.kgPerCapita") / 1000.0 - w.districts[i].foodProductionT; }, 5);
        const auto *mi = h.def("micro_irrigation");
        auto &r = h.add("food.irrigation", "Food security", "Expand irrigation to raise food production", 84);
        r.facilityId = "micro_irrigation"; r.quantity = 20; r.investmentCr = 20 * mi->capexCr;
        if (!cands.isEmpty()) r.targetDistrict = cands.first();
        r.why = QStringLiteral("Food supply covers %1 of requirement (%2 kg per person per year). Irrigation raises legacy farm yields (model: up to +%3 on irrigated land).")
                    .arg(pct(s.foodSupplyRatio)).arg(num(ctx.p("food.kgPerCapita"))).arg(pct(ctx.p("food.irrigationYieldBoost")));
        for (int c : cands) r.locations << QStringLiteral("%1 — deficit %2 t/yr").arg(h.dname(c)).arg(num(w.districts[c].population * ctx.p("food.kgPerCapita") / 1000.0 - w.districts[c].foodProductionT));
        r.supportingInfrastructure << "Electricity for pumping" << "Cold storage and grain warehouses (reduce losses)" << "Agricultural research centres";
        r.benefits << "Food security" << "Higher rural incomes";
        r.drawbacks << "Groundwater depletion risk" << "Extra electricity demand (~1,500 kWh/ha)";
        r.alternatives << "Canal irrigation (cheaper per ha near rivers)" << "Agricultural modernisation policy" << "Food imports (fiscal and strategic cost)";
        r.policyId = "agri_support";
        r.opportunityCost = h.opportunity(r.investmentCr);
        r.shortTerm = "Gains in the first season after completion."; r.longTerm = "Resilience to drought.";
        r.risks << "Monsoon variability";
        h.sim(r, "Food supply ratio", pct(s.foodSupplyRatio));
    }

    // ================= 13. Unemployment & industry =================
    if (s.unemploymentRate > 0.06 || s.manufacturingShare < 0.12) {
        const auto cands = h.topDistricts([&](int i) { return w.districts[i].unemployed; }, 6);
        if (!cands.isEmpty()) {
            const auto *tx = h.def("textile_mill");
            QString note;
            const int di = h.feasibleIn(cands, *tx, &note);
            auto &r = h.add("jobs." + QString::number(di), "Employment & industry", QStringLiteral("Labour-intensive manufacturing in %1").arg(h.dname(di)), s.unemploymentRate > 0.08 ? 83 : 60);
            r.facilityId = "textile_mill"; r.targetDistrict = di; r.quantity = 2; r.investmentCr = 2 * tx->capexCr;
            r.why = QStringLiteral("Unemployment is %1 and manufacturing is %2 of GDP. %3 has the most unemployed people (%4). Textile & apparel parks employ ≈%5 workers each, mostly without degrees.")
                        .arg(pct(s.unemploymentRate, 1)).arg(pct(s.manufacturingShare, 1)).arg(h.dname(di)).arg(num(w.districts[di].unemployed)).arg(num(tx->operationsJobs.total()));
            for (int c : cands) r.locations << QStringLiteral("%1 — %2 unemployed").arg(h.dname(c)).arg(num(w.districts[c].unemployed));
            r.supportingInfrastructure << "Electricity (40 MW each)" << "Road access and a port connection for exports" << "Industrial park" << "Housing for workers";
            r.benefits << "Large-scale formal jobs" << "Exports";
            r.drawbacks << "Low wages; competition from other exporters" << "Water and effluent pollution";
            r.alternatives << "Smartphone assembly (needs electronics level 1)" << "Food processing" << "Manufacturing incentive policy";
            r.policyId = "manufacturing_incentive";
            r.opportunityCost = h.opportunity(r.investmentCr);
            r.shortTerm = "Construction jobs immediately; factory jobs in ~2 years."; r.longTerm = "Structural transformation from farm to factory.";
            r.risks << "Private capital may be insufficient (privately financed)";
            h.sim(r, "Unemployment rate", pct(s.unemploymentRate, 1));
            h.sim(r, "Manufacturing share of GDP", pct(s.manufacturingShare, 1));
        }
    }

    // ================= 14. Import dependencies =================
    {
        QVector<QPair<double, QString>> imp;
        for (auto it = w.goodsImports.begin(); it != w.goodsImports.end(); ++it) {
            const auto *g = ctx.cat.good(it.key());
            if (g) imp.append({it.value() * g->unitValueCr, it.key()});
        }
        std::sort(imp.begin(), imp.end(), [](auto &a, auto &b) { return a.first > b.first; });
        for (int k = 0; k < std::min<int>(2, imp.size()); ++k) {
            const auto *g = ctx.cat.good(imp[k].second);
            const auto producers = ctx.cat.producersOf(g->id);
            if (producers.isEmpty() || imp[k].first < 1000) continue;
            const FacilityDefinition *pick = producers.first();
            for (const auto *p : producers) if (EconomyEngine::techFactor(ctx, *p) > EconomyEngine::techFactor(ctx, *pick)) pick = p;
            auto &r = h.add("imports." + g->id, "Trade & industry", QStringLiteral("Reduce import dependence on %1").arg(g->name), 55);
            r.facilityId = pick->id; r.investmentCr = pick->capexCr;
            const double tf = EconomyEngine::techFactor(ctx, *pick);
            r.why = QStringLiteral("Imports of %1 are worth %2 per year (import availability %3). Domestic production via '%4' is possible%5.")
                        .arg(g->name).arg(cr(imp[k].first)).arg(pct(g->importability)).arg(pick->name)
                        .arg(tf < 1 ? QStringLiteral(" but technology maturity currently limits output to %1").arg(pct(tf)) : QString());
            r.benefits << "Lower import bill and supply risk";
            r.drawbacks << "Domestic production may be more expensive than imports initially";
            r.alternatives << "Trade agreements to diversify suppliers" << "Strategic stockpiles (not modelled)";
            r.opportunityCost = h.opportunity(r.investmentCr);
            r.shortTerm = QStringLiteral("%1-month construction.").arg(pick->constructionMonths); r.longTerm = "Supply-chain resilience.";
            r.risks << pick->risks << "Domestic output may stay uncompetitive with imports";
            h.sim(r, "Annual import value", cr(imp[k].first));
        }
        if (s.importDependency > 0.3) {
            auto &r = h.add("imports.overall", "Trade & industry", "High import dependency across supply chains", 50);
            r.policyId = "manufacturing_incentive";
            r.why = QStringLiteral("%1 of the value of goods demanded is imported.").arg(pct(s.importDependency));
            r.benefits << "Industrial policy can attract domestic production"; r.drawbacks << "Fiscal cost";
            r.alternatives << "Export promotion" << "Import tariffs (risk of higher input costs)";
            r.opportunityCost = "Policy cost depends on intensity."; r.shortTerm = "Fiscal cost first."; r.longTerm = "Broader industrial base.";
            r.risks << "Subsidies may not create competitive firms";
            h.sim(r, "Import dependency", pct(s.importDependency));
        }
    }

    // ================= 15. R&D intensity vs benchmarks =================
    if (s.rndShareGdp < 1.0 && s.gdpCr > 0) {
        auto &r = h.add("rnd.weak", "Research & development", "Raise national R&D investment", 58);
        r.facilityId = "research_university"; r.investmentCr = h.def("research_university")->capexCr;
        r.policyId = "national_research_fund";
        r.why = QStringLiteral("Simulated R&D spending is %1% of GDP.").arg(s.rndShareGdp, 0, 'f', 2);
        if (ref) {
            const int y = int(ctx.p("scenario.referenceYear"));
            for (const auto &c : {QStringLiteral("CHN"), QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("IND")}) {
                const auto v = ref->valueAt("GB.XPD.RSDV.GD.ZS", c, y, 6);
                if (v.available) r.evidence.append({ref->countryName(c) + " R&D % of GDP", QString::number(v.value, 'f', 2), QStringLiteral("World Bank WDI (%1)").arg(v.year)});
            }
            r.why += " Comparator values from World Bank data are listed as evidence.";
        }
        r.benefits << "Research points across all technology domains" << "Scientists trained";
        r.drawbacks << "Slow, uncertain returns";
        r.alternatives << "R&D tax credits" << "Sector-specific research centres";
        r.opportunityCost = h.opportunity(r.investmentCr);
        r.shortTerm = "Costs now."; r.longTerm = "Technology maturity and innovation.";
        r.risks << "Brain drain if domestic jobs for researchers are scarce";
        h.sim(r, "R&D share of GDP", QString::number(s.rndShareGdp, 'f', 2) + "%");
    }

    // ================= 16. Regional inequality =================
    if (s.inequalityTheil > 0.15) {
        const int ns = geo.states().size();
        QVector<double> g(ns, 0), p(ns, 0);
        for (int i = 0; i < nd; ++i) { const int st = geo.districts()[i].stateIndex; if (st >= 0) { g[st] += w.districts[i].gdpCr; p[st] += w.districts[i].population; } }
        int worst = -1; double low = 1e18;
        for (int st = 0; st < ns; ++st) if (p[st] > 5e6 && g[st] / p[st] < low) { low = g[st] / p[st]; worst = st; }
        if (worst >= 0) {
            auto &r = h.add("inequality." + geo.states()[worst].id, "Regional balance", QStringLiteral("Targeted development in %1").arg(geo.states()[worst].name), 52);
            r.facilityId = "industrial_park"; r.investmentCr = h.def("industrial_park")->capexCr;
            r.targetDistrict = geo.states()[worst].districts.value(0, -1);
            r.why = QStringLiteral("Regional inequality (Theil index of district GDP per capita) is %1. %2 has the lowest GDP per capita among large states (₹%3 per person).").arg(s.inequalityTheil, 0, 'f', 2).arg(geo.states()[worst].name).arg(num(low * 1e7));
            r.locations << geo.states()[worst].name;
            r.supportingInfrastructure << "Power, roads and water for the industrial park" << "Vocational institutes";
            r.benefits << "Jobs where labour is abundant" << "Reduces migration pressure on cities";
            r.drawbacks << "Investment may be less productive than in existing clusters";
            r.alternatives << "Support migration to job centres (urban migration policy)";
            r.opportunityCost = h.opportunity(r.investmentCr);
            r.shortTerm = "3-year build."; r.longTerm = "More balanced growth.";
            r.risks << "Anchor investors may not come";
            h.sim(r, "Theil index", QString::number(s.inequalityTheil, 'f', 2));
        }
    }

    // ================= 17. Air quality =================
    {
        const auto cands = h.topDistricts([&](int i) { return w.districts[i].aqi > 200 ? w.districts[i].aqi * w.districts[i].population : 0; }, 5);
        if (!cands.isEmpty()) {
            const int di = cands.first();
            // attribute sources within the district
            double fac = 0; QString biggest; double bigE = 0;
            for (const auto &f : w.facilities) {
                if (f.district != di) continue;
                double e[7]; EnvironmentEngine::facilityEmissions(ctx, f, e);
                const double pmEq = e[0] + 0.3 * e[3] + 0.15 * e[2];
                fac += pmEq;
                if (pmEq > bigE) { bigE = pmEq; const auto *d = ctx.def(f); biggest = d ? d->name : f.defId; }
            }
            auto &r = h.add("air." + QString::number(di), "Environment", QStringLiteral("Air quality in %1 (AQI %2, %3)").arg(h.dname(di)).arg(w.districts[di].aqi).arg(CpcbAqi::categoryFor(w.districts[di].aqi)), 66);
            const bool industrial = fac > 2000;
            r.policyId = industrial ? "industrial_pollution" : "clean_cooking";
            if (geo.districts()[di].stateName == "Punjab" || geo.districts()[di].stateName == "Haryana") r.alternatives << "Crop-residue management policy (Oct-Nov burning)";
            r.targetDistrict = di;
            r.why = QStringLiteral("Monthly-mean PM2.5 in %1 is %2 µg/m³ (CPCB AQI %3). %4")
                        .arg(h.dname(di)).arg(w.districts[di].pm25, 0, 'f', 0).arg(w.districts[di].aqi)
                        .arg(industrial ? QStringLiteral("The largest local source is %1.").arg(biggest) : QStringLiteral("Household (biomass) and legacy transport emissions dominate; electrification and clean cooking reduce them."));
            for (int c : cands) r.locations << QStringLiteral("%1 — AQI %2").arg(h.dname(c)).arg(w.districts[c].aqi);
            r.benefits << QStringLiteral("Each 10 µg/m³ PM2.5 reduction adds ≈%1 years of life expectancy in the model (AQLI-based)").arg(ctx.p("pop.lePerPm25") * 10, 0, 'f', 1);
            r.drawbacks << (industrial ? "Higher compliance costs for industry" : "Recurring subsidy cost");
            r.alternatives << "Electrify the district (substation + distribution) to displace biomass" << "FGD-equipped plants instead of uncontrolled coal";
            r.opportunityCost = "Policy cost depends on intensity; see the policy workspace.";
            r.shortTerm = "Seasonal improvements within a year."; r.longTerm = "Health gains.";
            r.risks << "Weak enforcement" << "Simplified air-quality model: magnitudes are uncertain";
            h.sim(r, "PM2.5 (district)", QString::number(w.districts[di].pm25, 'f', 0) + " µg/m³");
        }
    }

    // ================= 18. Logistics =================
    {
        int noPort = 0;
        for (const auto &f : w.facilities) { const auto *d = ctx.def(f); if (d && f.isOperating() && d->tags.contains("manufacturing") && !f.portAccess) ++noPort; }
        if (noPort >= 3 && EconomyEngine::countTagged(ctx, "logistics_hub", {}, 0) == 0) {
            const auto *lh = h.def("logistics_hub");
            auto &r = h.add("logistics.hub", "Logistics", "Build a multimodal logistics park", 57);
            r.facilityId = "logistics_hub"; r.investmentCr = lh->capexCr;
            r.why = QStringLiteral("%1 manufacturing facilities have no port access and there is no logistics hub; exports and inputs move inefficiently.").arg(noPort);
            r.supportingInfrastructure << "Rail line to a seaport or dry port" << "Highway access";
            r.benefits << "Lower logistics cost, port access for inland industry";
            r.drawbacks << "Needs rail and road connections to be useful";
            r.alternatives << "Dry port / ICD (cheaper)";
            r.opportunityCost = h.opportunity(r.investmentCr);
            r.shortTerm = "30-month build."; r.longTerm = "Export competitiveness.";
            r.risks << "Under-utilisation without industrial clusters";
        }
    }

    std::stable_sort(h.out.begin(), h.out.end(), [](const Recommendation &a, const Recommendation &b) { return a.priority > b.priority; });
    return h.out;
}

} // namespace isim
