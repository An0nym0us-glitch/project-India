#pragma once
#include "core/Provenance.h"
#include <QColor>
#include <QLocale>
#include <QString>
#include <cmath>

namespace isim::fmt {

inline QString num(double v, int prec = 0) { return QLocale(QLocale::English).toString(v, 'f', prec); }
// Indian money units: crore (1e7) and lakh crore (1e12)
inline QString cr(double v)
{
    const double a = std::abs(v);
    if (a >= 100000) return QStringLiteral("₹%1 lakh cr").arg(num(v / 100000, 2));
    if (a >= 1) return QStringLiteral("₹%1 cr").arg(num(v, 0));
    return QStringLiteral("₹%1 lakh").arg(num(v * 100, 1));
}
inline QString usd(double v)
{
    const double a = std::abs(v);
    if (a >= 1e12) return QStringLiteral("US$%1 trillion").arg(num(v / 1e12, 2));
    if (a >= 1e9) return QStringLiteral("US$%1 billion").arg(num(v / 1e9, 1));
    if (a >= 1e6) return QStringLiteral("US$%1 million").arg(num(v / 1e6, 1));
    return QStringLiteral("US$%1").arg(num(v, 0));
}
inline QString people(double v)
{
    const double a = std::abs(v);
    if (a >= 1e9) return QStringLiteral("%1 bn").arg(num(v / 1e9, 3));
    if (a >= 1e7) return QStringLiteral("%1 crore").arg(num(v / 1e7, 2));
    if (a >= 1e5) return QStringLiteral("%1 lakh").arg(num(v / 1e5, 1));
    return num(v, 0);
}
inline QString pct(double f, int prec = 1) { return num(f * 100, prec) + "%"; }
inline QString mw(double v) { return std::abs(v) >= 1000 ? num(v / 1000, 1) + " GW" : num(v, 0) + " MW"; }
inline QString general(double v, const QString &unit)
{
    const double a = std::abs(v);
    QString s;
    if (a >= 1e12) s = num(v / 1e12, 2) + " trillion";
    else if (a >= 1e9) s = num(v / 1e9, 2) + " billion";
    else if (a >= 1e6) s = num(v / 1e6, 2) + " million";
    else if (a >= 100) s = num(v, 0);
    else s = num(v, 2);
    return unit.isEmpty() ? s : s + " " + unit;
}
inline QString badge(DataKind k)
{
    switch (k) {
    case DataKind::Measured: return QStringLiteral("<span style='background:#1f6f43;color:white;padding:0 4px'>MEASURED</span>");
    case DataKind::Estimated: return QStringLiteral("<span style='background:#5a5f1f;color:white;padding:0 4px'>ESTIMATED</span>");
    case DataKind::Assumption: return QStringLiteral("<span style='background:#7a4b00;color:white;padding:0 4px'>ASSUMPTION</span>");
    case DataKind::UserConfigured: return QStringLiteral("<span style='background:#244c8a;color:white;padding:0 4px'>USER</span>");
    case DataKind::Simulated: return QStringLiteral("<span style='background:#5b2a86;color:white;padding:0 4px'>SIMULATED</span>");
    }
    return {};
}
inline QString esc(const QString &s) { return s.toHtmlEscaped(); }

inline QColor categoryColor(const QString &cat)
{
    static const QHash<QString, QColor> c = {
        {"Residential & Urban", QColor(0xd9, 0x8c, 0x5f)}, {"Commercial & Industrial Zones", QColor(0xb0, 0x7a, 0xc6)},
        {"Transportation", QColor(0x6b, 0x6b, 0x6b)}, {"Energy & Utilities", QColor(0xe8, 0xb3, 0x1c)},
        {"Agriculture & Food", QColor(0x5f, 0xa8, 0x3a)}, {"Healthcare & Education", QColor(0xd9, 0x4a, 0x5a)},
        {"Heavy Industry & Mining", QColor(0x8a, 0x5a, 0x3c)}, {"Semiconductors", QColor(0x2a, 0x9d, 0xa8)},
        {"Computing & AI", QColor(0x3b, 0x6f, 0xd6)}, {"Technology Manufacturing", QColor(0x5d, 0x4f, 0xc4)},
        {"Digital Infrastructure", QColor(0x1f, 0x8f, 0xd0)}};
    return c.value(cat, QColor(0x88, 0x88, 0x88));
}

} // namespace isim::fmt
