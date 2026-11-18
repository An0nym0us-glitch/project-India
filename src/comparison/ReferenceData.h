#pragma once
#include <QHash>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

namespace isim {

struct ReferenceIndicator {
    QString code, group, label, wbName, unit, sourceOrganization, availabilityNote;
    std::optional<bool> higherIsBetter;
    QHash<QString, QMap<int, double>> series; // country ISO3 -> year -> value
};

struct ValueAtYear {
    bool available = false;
    double value = 0;
    int year = 0;          // actual year of the observation
    bool exactYear = false;
};

// Real-world reference data (World Bank WDI), loaded from the bundled snapshot.
class ReferenceData {
public:
    bool load(const QString &path, QString *error = nullptr);
    bool isLoaded() const { return !m_indicators.isEmpty(); }
    const QVector<ReferenceIndicator> &indicators() const { return m_indicators; }
    const ReferenceIndicator *indicator(const QString &code) const;
    QStringList countries() const { return m_countryOrder; }
    QString countryName(const QString &iso3) const { return m_countryNames.value(iso3, iso3); }
    QString source() const { return m_source; }
    QString retrieved() const { return m_retrieved; }
    QString license() const { return m_license; }
    // Value for the baseline year; falls back to the most recent earlier year within maxLag.
    ValueAtYear valueAt(const QString &code, const QString &iso3, int year, int maxLag = 5) const;
    // India age structure: share (%) of male/female population by 5-year group (17 groups)
    bool indiaAgeShares(int year, QVector<double> &male, QVector<double> &female, double &maleSharePct) const;
private:
    QVector<ReferenceIndicator> m_indicators;
    QHash<QString, int> m_index;
    QHash<QString, QString> m_countryNames;
    QStringList m_countryOrder;
    QString m_source, m_retrieved, m_license;
    QHash<QString, QMap<int, double>> m_ageGroups;
    QMap<int, double> m_maleShare;
};

} // namespace isim
