#include <algorithm>
#include "Aqi.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>

namespace isim {

static const int kIndexBands[6][2] = {{0, 50}, {51, 100}, {101, 200}, {201, 300}, {301, 400}, {401, 500}};

bool CpcbAqi::load(const QString &path, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { if (error) *error = "Cannot open " + path; loadDefaults(); return false; }
    const auto o = QJsonDocument::fromJson(f.readAll()).object();
    m_source = o.value("source").toString();
    const auto pol = o.value("pollutants").toObject();
    m_bands.clear();
    for (auto it = pol.begin(); it != pol.end(); ++it) {
        const auto bp = it.value().toObject().value("bp").toArray();
        QVector<Band> bands;
        for (int i = 0; i < bp.size() && i < 6; ++i) {
            const auto r = bp[i].toArray();
            bands.append({r[0].toDouble(), r[1].toDouble(), kIndexBands[i][0], kIndexBands[i][1]});
        }
        m_bands.insert(it.key(), bands);
    }
    if (m_bands.isEmpty()) { loadDefaults(); return false; }
    return true;
}

void CpcbAqi::loadDefaults()
{
    auto mk = [](std::initializer_list<std::pair<double, double>> l) {
        QVector<Band> b; int i = 0;
        for (const auto &p : l) { b.append({p.first, p.second, kIndexBands[i][0], kIndexBands[i][1]}); ++i; }
        return b;
    };
    m_bands["pm10"] = mk({{0, 50}, {51, 100}, {101, 250}, {251, 350}, {351, 430}, {431, 510}});
    m_bands["pm25"] = mk({{0, 30}, {31, 60}, {61, 90}, {91, 120}, {121, 250}, {251, 380}});
    m_bands["no2"] = mk({{0, 40}, {41, 80}, {81, 180}, {181, 280}, {281, 400}, {401, 800}});
    m_bands["o3"] = mk({{0, 50}, {51, 100}, {101, 168}, {169, 208}, {209, 748}, {749, 1000}});
    m_bands["co"] = mk({{0, 1.0}, {1.1, 2.0}, {2.1, 10}, {10.1, 17}, {17.1, 34}, {34.1, 50}});
    m_bands["so2"] = mk({{0, 40}, {41, 80}, {81, 380}, {381, 800}, {801, 1600}, {1601, 2400}});
    m_source = "CPCB National Air Quality Index (2014)";
}

int CpcbAqi::subIndex(const QString &pollutant, double c) const
{
    const auto it = m_bands.constFind(pollutant);
    if (it == m_bands.constEnd() || c < 0 || std::isnan(c)) return -1;
    const auto &bands = *it;
    for (int i = 0; i < bands.size(); ++i) {
        const auto &b = bands[i];
        // concentrations between published integer limits (e.g. 30 < c < 31) belong to the lower band
        const double upper = (i + 1 < bands.size()) ? bands[i + 1].cLo : b.cHi;
        if (c <= b.cHi || (c < upper && i + 1 < bands.size())) {
            const double cHi = b.cHi;
            const double cc = std::min(c, cHi);
            const double v = b.iLo + (b.iHi - b.iLo) * (cc - b.cLo) / std::max(1e-9, cHi - b.cLo);
            return int(std::lround(std::max(double(b.iLo), v)));
        }
    }
    return 500; // above the Severe band: capped
}

AqiResult CpcbAqi::compute(const QHash<QString, double> &conc) const
{
    AqiResult r;
    int best = -1;
    bool hasPm = false;
    for (auto it = conc.begin(); it != conc.end(); ++it) {
        const int si = subIndex(it.key(), it.value());
        if (si < 0) continue;
        r.subIndices.insert(it.key(), si);
        if (it.key() == "pm25" || it.key() == "pm10") hasPm = true;
        if (si > best) { best = si; r.dominant = it.key(); }
    }
    // CPCB rule: at least three pollutants, one of which must be PM2.5 or PM10
    if (r.subIndices.size() < 3 || !hasPm) return r;
    r.aqi = best;
    r.category = categoryFor(best);
    r.color = colorFor(best);
    return r;
}

QString CpcbAqi::categoryFor(int a)
{
    if (a < 0) return QStringLiteral("Insufficient data");
    if (a <= 50) return QStringLiteral("Good");
    if (a <= 100) return QStringLiteral("Satisfactory");
    if (a <= 200) return QStringLiteral("Moderate");
    if (a <= 300) return QStringLiteral("Poor");
    if (a <= 400) return QStringLiteral("Very Poor");
    return QStringLiteral("Severe");
}

QColor CpcbAqi::colorFor(int a)
{
    if (a < 0) return QColor(128, 128, 128);
    if (a <= 50) return QColor(0x00, 0xb0, 0x50);
    if (a <= 100) return QColor(0x92, 0xd0, 0x50);
    if (a <= 200) return QColor(0xff, 0xff, 0x00);
    if (a <= 300) return QColor(0xff, 0x99, 0x00);
    if (a <= 400) return QColor(0xff, 0x00, 0x00);
    return QColor(0xc0, 0x00, 0x00);
}

} // namespace isim
