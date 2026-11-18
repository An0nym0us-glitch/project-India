#pragma once
#include "Provenance.h"
#include <QHash>
#include <QString>
#include <QStringList>
#include <random>

namespace isim {

struct Parameter {
    QString key;
    double value = 0;   // central value
    double low = 0;     // plausible lower bound (uncertainty range)
    double high = 0;    // plausible upper bound
    QString unit;
    QString description;
    Provenance provenance;
};

// Global model coefficients loaded from data/model_parameters.json.
// Projections sample parameters within [low, high] to show uncertainty.
class ModelParameters {
public:
    bool load(const QString &path, QString *error = nullptr);
    double operator()(const QString &key) const { return get(key); }
    double get(const QString &key) const;
    bool contains(const QString &key) const { return m_params.contains(key); }
    const Parameter *find(const QString &key) const;
    QStringList keys() const;
    void set(const QString &key, double value);
    // Returns a copy with every uncertain parameter sampled (triangular distribution).
    ModelParameters sampled(std::mt19937_64 &rng) const;
    void insert(const Parameter &p) { m_params.insert(p.key, p); }

private:
    QHash<QString, Parameter> m_params;
};

} // namespace isim
