#pragma once
#include "ReferenceData.h"
#include <QString>
#include <QVector>
#include <optional>

namespace isim {
class Simulation;

struct ComparisonCell {
    bool available = false;
    double value = 0;
    int year = 0;
    QString note;      // e.g. "latest available: 2021"
};

struct ComparisonRow {
    QString code, group, label, unit, source;
    std::optional<bool> higherIsBetter;
    ComparisonCell simulated;                 // simulated India (same definition), if comparable
    QString simulatedNote;                    // why not comparable / how computed
    QVector<ComparisonCell> countries;        // order = ReferenceData::countries()
    int simulatedRank = 0;                    // 1 = best among 6 (0 = not ranked)
};

// Compares simulated India with real-world reference data for a fixed baseline year.
class ComparisonEngine {
public:
    static QVector<ComparisonRow> compare(const Simulation &sim, const ReferenceData &ref, int baselineYear);
    // Simulated value of a World Bank indicator when the model computes the same definition.
    static std::optional<double> simulatedValue(const Simulation &sim, const QString &code, QString *note = nullptr);
};

struct StrategyPreset {
    QString id, name, description;
    QVector<QPair<QString, double>> policies;
};
QVector<StrategyPreset> strategyPresets();

} // namespace isim
