#include <functional>
#include <numeric>
#include "ProjectionRunner.h"
#include <QtConcurrent/QtConcurrent>
#include <algorithm>
#include <atomic>
#include <random>

namespace isim {

QVector<QPair<QString, QString>> ProjectionRunner::defaultIndicators()
{
    return {{"gdpUsd", "GDP (nominal, US$)"}, {"gdpPerCapitaUsd", "GDP per capita (US$)"}, {"population", "Population"},
            {"unemploymentRate", "Unemployment rate"}, {"electricityAccess", "Electricity demand served"},
            {"generationCapacityMW", "Generation capacity (MW)"}, {"renewableShareGen", "Renewable share of generation"},
            {"lifeExpectancy", "Life expectancy (years)"}, {"tfr", "Fertility rate"}, {"urbanShare", "Urban share"},
            {"pm25", "PM2.5 (pop.-weighted, µg/m³)"}, {"co2Mt", "CO2 emissions (Mt)"}, {"debtToGdp", "Public debt / GDP"},
            {"developmentScore", "Development score (0-100)"}, {"housingShortage", "Housing shortage (persons)"},
            {"foodSupplyRatio", "Food supply ratio"}, {"dependencyRatio", "Dependency ratio"}, {"importDependency", "Import dependency"}};
}

ProjectionResult ProjectionRunner::run(const Simulation &base, int years, int members, quint64 seed,
                                       const std::function<void(int, int)> &progress)
{
    ProjectionResult res;
    res.scenarioName = base.world().name;
    res.startYear = base.world().year();
    res.horizonYears = years;
    res.members = members;
    const auto indicators = defaultIndicators();
    QVector<int> idx(members);
    std::iota(idx.begin(), idx.end(), 0);
    std::atomic<int> done{0};
    // each member: yearly values for each indicator
    auto runMember = [&](int m) {
        Simulation sim(base);
        sim.setFastMode(true);
        std::mt19937_64 rng(seed + 7919ULL * m);
        if (m > 0) { // member 0 = central parameters
            sim.setParams(base.params().sampled(rng));
            std::uniform_real_distribution<double> u(0, 1);
            for (auto &p : sim.world().policies) {
                const auto *d = base.resources().policies.find(p.id);
                if (d) p.effectiveness = d->effLow + (d->effHigh - d->effLow) * u(rng);
            }
        }
        QVector<QVector<double>> out(indicators.size());
        for (int y = 1; y <= years; ++y) {
            sim.runMonths(12);
            const auto &h = sim.world().history.last();
            for (int k = 0; k < indicators.size(); ++k) out[k] << h.v.value(indicators[k].first);
        }
        const int d = ++done;
        if (progress) progress(d, members);
        return out;
    };
    const auto results = QtConcurrent::blockingMapped<QVector<QVector<QVector<double>>>>(idx, std::function<QVector<QVector<double>>(int)>(runMember));
    for (int k = 0; k < indicators.size(); ++k) {
        ProjectionSeries s;
        s.key = indicators[k].first; s.label = indicators[k].second;
        for (int y = 0; y < years; ++y) {
            QVector<double> vals;
            for (const auto &r : results) if (k < r.size() && y < r[k].size()) vals << r[k][y];
            std::sort(vals.begin(), vals.end());
            auto pct = [&](double p) { if (vals.isEmpty()) return 0.0; const double pos = p * (vals.size() - 1); const int i = int(pos); const double f = pos - i; return i + 1 < vals.size() ? vals[i] * (1 - f) + vals[i + 1] * f : vals[i]; };
            s.years << res.startYear + y + 1;
            s.p10 << pct(0.1); s.p50 << pct(0.5); s.p90 << pct(0.9);
        }
        res.series << s;
    }
    res.notes << QStringLiteral("%1 ensemble members; parameters sampled within documented ranges; policy effectiveness sampled within each policy's range.").arg(members)
              << QStringLiteral("Bands show the 10th-90th percentile of model outcomes under parameter uncertainty only. They do not capture structural model error, shocks or future decisions.")
              << QStringLiteral("No new construction or policy changes are assumed beyond what is already planned in the scenario.");
    return res;
}

} // namespace isim
