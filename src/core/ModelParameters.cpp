#include "ModelParameters.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDebug>
#include <algorithm>
#include <cmath>

namespace isim {

bool ModelParameters::load(const QString &path, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("Cannot open %1").arg(path);
        return false;
    }
    const auto doc = QJsonDocument::fromJson(f.readAll());
    const auto arr = doc.object().value("parameters").toArray();
    for (const auto &v : arr) {
        const auto o = v.toObject();
        Parameter p;
        p.key = o.value("key").toString();
        p.value = o.value("value").toDouble();
        p.low = o.contains("low") ? o.value("low").toDouble() : p.value;
        p.high = o.contains("high") ? o.value("high").toDouble() : p.value;
        p.unit = o.value("unit").toString();
        p.description = o.value("description").toString();
        p.provenance.kind = dataKindFromString(o.value("kind").toString());
        p.provenance.source = o.value("source").toString();
        p.provenance.confidence = o.value("confidence").toString();
        m_params.insert(p.key, p);
    }
    if (m_params.isEmpty() && error) *error = QStringLiteral("No parameters in %1").arg(path);
    return !m_params.isEmpty();
}

double ModelParameters::get(const QString &key) const
{
    auto it = m_params.constFind(key);
    if (it == m_params.constEnd()) {
        qWarning() << "Unknown model parameter" << key;
        return 0.0;
    }
    return it->value;
}

const Parameter *ModelParameters::find(const QString &key) const
{
    auto it = m_params.constFind(key);
    return it == m_params.constEnd() ? nullptr : &it.value();
}

QStringList ModelParameters::keys() const
{
    QStringList k = m_params.keys();
    k.sort();
    return k;
}

void ModelParameters::set(const QString &key, double value)
{
    auto it = m_params.find(key);
    if (it != m_params.end()) {
        it->value = value;
    } else {
        Parameter p; p.key = key; p.value = p.low = p.high = value;
        p.provenance.kind = DataKind::UserConfigured;
        m_params.insert(key, p);
    }
}

ModelParameters ModelParameters::sampled(std::mt19937_64 &rng) const
{
    ModelParameters out = *this;
    std::uniform_real_distribution<double> u(0.0, 1.0);
    // iterate in sorted order so a given seed is reproducible
    for (const auto &k : keys()) {
        Parameter &p = out.m_params[k];
        if (p.high <= p.low) continue;
        const double a = p.low, b = p.high, c = std::clamp(p.value, a, b);
        const double r = u(rng);
        const double fc = (c - a) / (b - a);
        p.value = r < fc ? a + std::sqrt(r * (b - a) * (c - a))
                         : b - std::sqrt((1 - r) * (b - a) * (b - c));
    }
    return out;
}

} // namespace isim
