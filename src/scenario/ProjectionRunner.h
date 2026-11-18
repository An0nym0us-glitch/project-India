#pragma once
#include "sim/Simulation.h"
#include <QStringList>
#include <QVector>

namespace isim {

struct ProjectionSeries {
    QString key, label, unit;
    QVector<int> years;
    QVector<double> p10, p50, p90; // percentiles across the ensemble
};

struct ProjectionResult {
    QString scenarioName;
    int startYear = 0, horizonYears = 0, members = 0;
    QVector<ProjectionSeries> series;
    QStringList notes;
    const ProjectionSeries *find(const QString &key) const {
        for (const auto &s : series) if (s.key == key) return &s;
        return nullptr;
    }
};

// Runs an ensemble of simulations forward with sampled uncertain parameters and
// policy effectiveness, and reports percentile bands. Projections are not forecasts.
class ProjectionRunner {
public:
    static QVector<QPair<QString, QString>> defaultIndicators(); // key -> label
    static ProjectionResult run(const Simulation &base, int years, int members, quint64 seed,
                                const std::function<void(int done, int total)> &progress = {});
};

} // namespace isim
