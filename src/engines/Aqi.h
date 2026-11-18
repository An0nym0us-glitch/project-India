#pragma once
#include <QColor>
#include <QHash>
#include <QString>
#include <QVector>

namespace isim {

struct AqiResult {
    int aqi = -1;               // -1 when not computable
    QString dominant;           // pollutant with the highest sub-index
    QString category;
    QColor color;
    QHash<QString, int> subIndices;
};

// Indian National Air Quality Index (CPCB, 2014) calculator.
class CpcbAqi {
public:
    bool load(const QString &path, QString *error = nullptr);
    void loadDefaults();      // built-in copy of the CPCB breakpoints
    int subIndex(const QString &pollutant, double concentration) const;
    // concentrations keyed by pm25, pm10, no2, so2, co (mg/m3), o3
    AqiResult compute(const QHash<QString, double> &conc) const;
    static QString categoryFor(int aqi);
    static QColor colorFor(int aqi);
    QString source() const { return m_source; }
private:
    struct Band { double cLo, cHi; int iLo, iHi; };
    QHash<QString, QVector<Band>> m_bands;
    QString m_source;
};

} // namespace isim
