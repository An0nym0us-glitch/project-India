#pragma once
#include <QString>
#include <QJsonObject>

namespace isim {

// Every number shown to the user is tagged with where it came from.
enum class DataKind {
    Measured,        // real-world observation from a cited dataset
    Estimated,       // derived from measured data with a documented method
    Assumption,      // modelling assumption (indicative, documented)
    UserConfigured,  // set by the user
    Simulated        // output of the simulation engine
};

inline QString dataKindName(DataKind k)
{
    switch (k) {
    case DataKind::Measured: return QStringLiteral("Measured");
    case DataKind::Estimated: return QStringLiteral("Estimated");
    case DataKind::Assumption: return QStringLiteral("Modelling assumption");
    case DataKind::UserConfigured: return QStringLiteral("User-configured");
    case DataKind::Simulated: return QStringLiteral("Simulated projection");
    }
    return {};
}

inline DataKind dataKindFromString(const QString &s)
{
    const QString l = s.toLower();
    if (l.startsWith("measured")) return DataKind::Measured;
    if (l.startsWith("estimated")) return DataKind::Estimated;
    if (l.startsWith("user")) return DataKind::UserConfigured;
    if (l.startsWith("sim")) return DataKind::Simulated;
    return DataKind::Assumption;
}

struct Provenance {
    DataKind kind = DataKind::Assumption;
    QString source;      // dataset / document name, or basis of the assumption
    QString confidence;  // "low" | "medium" | "high"
};

} // namespace isim
