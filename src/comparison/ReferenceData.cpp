#include "ReferenceData.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace isim {

static QMap<int, double> yearMap(const QJsonObject &o)
{
    QMap<int, double> m;
    for (auto it = o.begin(); it != o.end(); ++it) m.insert(it.key().toInt(), it.value().toDouble());
    return m;
}

bool ReferenceData::load(const QString &path, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { if (error) *error = "Cannot open " + path; return false; }
    const auto o = QJsonDocument::fromJson(f.readAll()).object();
    m_source = o.value("source").toString();
    m_retrieved = o.value("retrieved").toString();
    m_license = o.value("license").toString();
    const auto c = o.value("countries").toObject();
    m_countryOrder = {"IND", "CHN", "USA", "JPN", "RUS"};
    for (auto it = c.begin(); it != c.end(); ++it) m_countryNames.insert(it.key(), it.value().toString());
    m_indicators.clear();
    for (const auto &v : o.value("indicators").toArray()) {
        const auto io = v.toObject();
        ReferenceIndicator ri;
        ri.code = io.value("code").toString(); ri.group = io.value("group").toString(); ri.label = io.value("label").toString();
        ri.wbName = io.value("wbName").toString(); ri.unit = io.value("unit").toString();
        ri.sourceOrganization = io.value("sourceOrganization").toString(); ri.availabilityNote = io.value("availabilityNote").toString();
        if (io.value("higherIsBetter").isBool()) ri.higherIsBetter = io.value("higherIsBetter").toBool();
        const auto s = io.value("series").toObject();
        for (auto it = s.begin(); it != s.end(); ++it) ri.series.insert(it.key(), yearMap(it.value().toObject()));
        m_index.insert(ri.code, m_indicators.size());
        m_indicators << ri;
    }
    const auto age = o.value("indiaAgeStructure").toObject();
    m_maleShare = yearMap(age.value("maleSharePct").toObject());
    const auto g = age.value("groups").toObject();
    for (auto it = g.begin(); it != g.end(); ++it) m_ageGroups.insert(it.key(), yearMap(it.value().toObject()));
    if (m_indicators.isEmpty() && error) *error = "No indicators in " + path;
    return !m_indicators.isEmpty();
}

const ReferenceIndicator *ReferenceData::indicator(const QString &code) const
{
    const int i = m_index.value(code, -1);
    return i < 0 ? nullptr : &m_indicators[i];
}

ValueAtYear ReferenceData::valueAt(const QString &code, const QString &iso3, int year, int maxLag) const
{
    ValueAtYear r;
    const auto *ind = indicator(code);
    if (!ind) return r;
    const auto s = ind->series.value(iso3);
    for (int y = year; y >= year - maxLag; --y) {
        auto it = s.constFind(y);
        if (it != s.constEnd()) { r.available = true; r.value = it.value(); r.year = y; r.exactYear = (y == year); return r; }
    }
    return r;
}

bool ReferenceData::indiaAgeShares(int year, QVector<double> &male, QVector<double> &female, double &maleSharePct) const
{
    static const char *groups[] = {"0004", "0509", "1014", "1519", "2024", "2529", "3034", "3539", "4044", "4549",
                                   "5054", "5559", "6064", "6569", "7074", "7579", "80UP"};
    male.clear(); female.clear();
    for (const char *g : groups) {
        const auto m = m_ageGroups.value(QStringLiteral("MA_") + g), f = m_ageGroups.value(QStringLiteral("FE_") + g);
        int y = year;
        while (y > year - 6 && (!m.contains(y) || !f.contains(y))) --y;
        if (!m.contains(y) || !f.contains(y)) return false;
        male << m.value(y); female << f.value(y);
    }
    int y = year;
    while (y > year - 6 && !m_maleShare.contains(y)) --y;
    maleSharePct = m_maleShare.value(y, 51.5);
    return true;
}

} // namespace isim
