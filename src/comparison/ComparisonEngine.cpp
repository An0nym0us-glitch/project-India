#include "ComparisonEngine.h"
#include "sim/Simulation.h"
#include <algorithm>

namespace isim {

std::optional<double> ComparisonEngine::simulatedValue(const Simulation &sim, const QString &code, QString *note)
{
    const auto &s = sim.world().stats;
    const auto &b = sim.world().budget;
    auto n = [&](const QString &t) { if (note) *note = t; };
    if (code == "NY.GDP.MKTP.CD") { n("Simulated GDP converted at the fixed model exchange rate."); return s.gdpUsd; }
    if (code == "NY.GDP.PCAP.CD") { n("Simulated GDP / simulated population."); return s.gdpPerCapitaUsd; }
    if (code == "NY.GDP.MKTP.KD.ZG") { n("Simulated growth over the last 12 months (current prices; no inflation is modelled)."); return s.gdpGrowth; }
    if (code == "NV.IND.TOTL.ZS") { n("Industry incl. construction, energy, mining, manufacturing."); return s.industryShare * 100; }
    if (code == "NV.IND.MANF.ZS") return s.manufacturingShare * 100;
    if (code == "NE.EXP.GNFS.CD") { n("Goods exports only (services trade is not modelled)."); return s.exportsCr * 1e7 / sim.params()("macro.inrPerUsd"); }
    if (code == "NE.IMP.GNFS.CD") { n("Goods imports only (services trade is not modelled)."); return s.importsCr * 1e7 / sim.params()("macro.inrPerUsd"); }
    if (code == "GC.DOD.TOTL.GD.ZS") { n("Debt accumulated by the simulated government (greenfield starts with no debt)."); return s.gdpCr > 0 ? b.debtCr / s.gdpCr * 100 : 0; }
    if (code == "SP.POP.TOTL") return s.population;
    if (code == "SP.POP.1564.TO.ZS") return s.workingAgeShare * 100;
    if (code == "SP.URB.TOTL.IN.ZS") { n("Urban = legacy urban population + residents of modern urban housing."); return s.urbanShare * 100; }
    if (code == "SL.TLF.TOTL.IN") return s.labourForce;
    if (code == "SP.POP.DPND") return s.dependencyRatio;
    if (code == "SP.DYN.LE00.IN") return s.lifeExpectancy;
    if (code == "SP.DYN.TFRT.IN") return s.tfr;
    if (code == "SL.UEM.TOTL.ZS") return s.unemploymentRate * 100;
    if (code == "SP.POP.GROW") {
        const auto &h = sim.world().history;
        if (h.size() > 12) { const double a = h[h.size() - 13].v.value("population"); if (a > 0) return (s.population / a - 1) * 100; }
        n("Needs 12 months of simulation."); return std::nullopt;
    }
    if (code == "EG.ELC.ACCS.ZS") { n("Share of electricity demand served by the modelled grid (proxy for access)."); return s.electricityAccess * 100; }
    if (code == "EG.USE.ELEC.KH.PC") { n("Electricity delivered per person."); return s.kwhPerCapita; }
    if (code == "EG.ELC.RNEW.ZS") { n("Renewable share of modelled generation (hydro, solar, wind, geothermal)."); return s.renewableShareGen * 100; }
    if (code == "GB.XPD.RSDV.GD.ZS") { n("Research-facility operating costs plus R&D policy spending."); return s.rndShareGdp; }
    if (code == "SP.POP.SCIE.RD.P6") return s.researchersPerMillion;
    if (code == "SH.MED.BEDS.ZS") { n("Beds in modelled facilities only (greenfield)."); return s.hospitalBedsPer1000; }
    if (code == "SE.TER.ENRR") { n("Modelled higher-education seats / population aged 20-24."); return s.tertiaryEnrolment; }
    if (code == "EN.ATM.PM25.MC.M3") { n("Population-weighted monthly mean from the simplified air model (WB value is an annual mean)."); return s.pm25PopWeighted; }
    if (code == "EN.GHG.CO2.MT.CE.AR5") { n("Modelled sources only."); return s.co2Mt; }
    if (code == "EN.GHG.CO2.PC.CE.AR5") { n("Modelled sources only."); return s.co2PerCapita; }
    if (note) *note = QStringLiteral("Not simulated by the model");
    return std::nullopt;
}

QVector<ComparisonRow> ComparisonEngine::compare(const Simulation &sim, const ReferenceData &ref, int year)
{
    QVector<ComparisonRow> rows;
    for (const auto &ind : ref.indicators()) {
        ComparisonRow r;
        r.code = ind.code; r.group = ind.group; r.label = ind.label; r.unit = ind.unit; r.higherIsBetter = ind.higherIsBetter;
        r.source = (ind.sourceOrganization.isEmpty() ? ref.source() : ind.sourceOrganization.left(120)) + QStringLiteral(" — World Bank WDI, retrieved ") + ref.retrieved();
        for (const auto &c : ref.countries()) {
            const auto v = ref.valueAt(ind.code, c, year, 5);
            ComparisonCell cell;
            cell.available = v.available; cell.value = v.value; cell.year = v.year;
            if (v.available && !v.exactYear) cell.note = QStringLiteral("latest available: %1").arg(v.year);
            if (!v.available) cell.note = ind.availabilityNote.isEmpty() ? QStringLiteral("Unavailable for %1 (±5 years)").arg(year) : ind.availabilityNote;
            r.countries << cell;
        }
        QString note;
        const auto sv = simulatedValue(sim, ind.code, &note);
        r.simulatedNote = note;
        if (sv) { r.simulated.available = true; r.simulated.value = *sv; r.simulated.year = sim.world().year(); }
        // rank simulated India among the five real countries + itself (only for directional indicators)
        if (sv && ind.higherIsBetter.has_value()) {
            QVector<double> vals{*sv};
            for (const auto &c : r.countries) if (c.available) vals << c.value;
            if (vals.size() >= 3) {
                int rank = 1;
                for (int i = 1; i < vals.size(); ++i)
                    if ((*ind.higherIsBetter && vals[i] > vals[0]) || (!*ind.higherIsBetter && vals[i] < vals[0])) ++rank;
                r.simulatedRank = rank;
            }
        }
        rows << r;
    }
    return rows;
}

QVector<StrategyPreset> strategyPresets()
{
    return {
        {"A", "A: Prioritise manufacturing", "Industrial incentives, export promotion, labour reform and skills.",
         {{"manufacturing_incentive", 1.0}, {"export_promotion", 0.8}, {"labour_reform", 0.6}, {"skills_mission", 0.6}}},
        {"B", "B: Prioritise education & research", "Education spending, national research foundation, AI programme and R&D credits.",
         {{"education_spending", 1.0}, {"national_research_fund", 1.0}, {"ai_programme", 0.7}, {"rnd_tax_credit", 0.7}, {"medical_education", 0.5}}},
        {"C", "C: Prioritise renewable energy", "Renewable push, coal phase-down, efficiency standards and clean transport.",
         {{"renewable_push", 1.0}, {"coal_phase_down", 0.6}, {"energy_efficiency", 0.8}, {"vehicle_standards", 0.6}, {"clean_cooking", 0.5}}},
        {"D", "D: Balanced development", "Moderate intensity across industry, human development, energy and governance.",
         {{"manufacturing_incentive", 0.5}, {"education_spending", 0.5}, {"health_spending", 0.5}, {"renewable_push", 0.5}, {"governance_reform", 0.5}, {"skills_mission", 0.4}}},
    };
}

} // namespace isim
