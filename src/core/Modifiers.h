#pragma once
#include <QHash>
#include <QString>

namespace isim {
// Policy-driven adjustments to model coefficients, recomputed every month.
// mul(key) defaults to 1, add(key) defaults to 0.
class Modifiers {
public:
    void clear() { m_mul.clear(); m_add.clear(); }
    double mul(const QString &k) const { return m_mul.value(k, 1.0); }
    double add(const QString &k) const { return m_add.value(k, 0.0); }
    void applyMul(const QString &k, double f) { m_mul[k] = mul(k) * f; }
    void applyAdd(const QString &k, double v) { m_add[k] = add(k) + v; }
    const QHash<QString, double> &muls() const { return m_mul; }
    const QHash<QString, double> &adds() const { return m_add; }
private:
    QHash<QString, double> m_mul, m_add;
};
}
