#include "GeoDatabase.h"
#include "GeoMath.h"
#include <QJsonObject>
#include <cmath>

namespace isim {

qint64 GeoDatabase::cellKey(double lon, double lat) const
{
    const qint64 cx = qint64(std::floor(lon / kCell)), cy = qint64(std::floor(lat / kCell));
    return (cx << 20) ^ (cy & 0xFFFFF);
}

static QPointF centroidOf(const GeoFeature &f)
{
    // area-weighted centroid of the largest polygon's outer ring
    double best = -1; QPointF c = f.bbox.center();
    for (const auto &poly : f.polygons) {
        if (poly.isEmpty()) continue;
        const QPolygonF &r = poly[0];
        double a = 0, cx = 0, cy = 0;
        for (int i = 0, n = r.size(); i < n; ++i) {
            const QPointF &p = r[i], &q = r[(i + 1) % n];
            const double cr = p.x() * q.y() - q.x() * p.y();
            a += cr; cx += (p.x() + q.x()) * cr; cy += (p.y() + q.y()) * cr;
        }
        if (std::abs(a) > best && std::abs(a) > 1e-12) {
            best = std::abs(a);
            c = QPointF(cx / (3 * a), cy / (3 * a));
        }
    }
    if (!f.contains(c) && !f.polygons.isEmpty() && !f.polygons[0].isEmpty() && !f.polygons[0][0].isEmpty()) {
        // fall back to a vertex-average point that lies inside
        const QPolygonF &r = f.polygons[0][0];
        for (int i = 0; i < r.size(); i += std::max<int>(1, r.size() / 16)) {
            QPointF cand = (r[i] + f.bbox.center()) / 2;
            if (f.contains(cand)) return cand;
        }
    }
    return c;
}

bool GeoDatabase::load(const QString &dir, QString *error)
{
    QString err;
    m_stateFeatures = loadGeoJson(dir + "/states.geojson", &err);
    if (m_stateFeatures.isEmpty()) { if (error) *error = "states.geojson: " + err; return false; }
    m_districtFeatures = loadGeoJson(dir + "/districts.geojson", &err);
    if (m_districtFeatures.isEmpty()) { if (error) *error = "districts.geojson: " + err; return false; }
    m_rivers = loadGeoJson(dir + "/rivers.geojson");
    m_lakes = loadGeoJson(dir + "/lakes.geojson");
    m_physical = loadGeoJson(dir + "/physical_regions.geojson");
    m_neighbours = loadGeoJson(dir + "/neighbours.geojson");
    m_coast = loadGeoJson(dir + "/coastline.geojson");
    m_outline = loadGeoJson(dir + "/india_outline.geojson");

    m_states.clear();
    for (int i = 0; i < m_stateFeatures.size(); ++i) {
        const auto &f = m_stateFeatures[i];
        State s;
        s.index = i;
        s.id = f.properties.value("id").toString();
        s.name = f.properties.value("name").toString();
        s.type = f.properties.value("type").toString();
        s.areaKm2 = f.properties.value("areaKm2").toDouble();
        s.census2011Population = f.properties.value("census2011Population").toDouble();
        s.centroid = centroidOf(f);
        s.feature = &m_stateFeatures[i];
        m_stateById.insert(s.id, i);
        m_stateByName.insert(s.name.toLower(), i);
        m_states << s;
        m_bounds = m_bounds.united(f.bbox);
    }
    m_districts.clear();
    for (int i = 0; i < m_districtFeatures.size(); ++i) {
        const auto &f = m_districtFeatures[i];
        District d;
        d.index = i;
        d.id = f.properties.value("id").toString();
        d.name = f.properties.value("name").toString();
        d.stateName = f.properties.value("state").toString();
        d.stateIndex = m_stateByName.value(d.stateName.toLower(), -1);
        d.stateId = d.stateIndex >= 0 ? m_states[d.stateIndex].id : QString();
        d.areaKm2 = f.properties.value("areaKm2").toDouble();
        d.centroid = centroidOf(f);
        d.feature = &m_districtFeatures[i];
        const auto c = f.properties.value("census2011").toObject();
        auto g = [&](const char *k) { return c.value(k).toDouble(); };
        auto &cd = d.census;
        cd.population = g("Population"); cd.male = g("Male"); cd.female = g("Female"); cd.literate = g("Literate");
        cd.workers = g("Workers"); cd.cultivators = g("Cultivator_Workers"); cd.agriLabourers = g("Agricultural_Workers");
        cd.householdIndustry = g("Household_Workers"); cd.otherWorkers = g("Other_Workers");
        cd.households = g("Households"); cd.ruralHouseholds = g("Rural_Households"); cd.urbanHouseholds = g("Urban_Households");
        cd.electrifiedHouseholds = g("Housholds_with_Electric_Lighting"); cd.internetHouseholds = g("Households_with_Internet");
        cd.tapWaterHouseholds = g("Main_source_of_drinking_water_Tapwater_Households");
        cd.latrineHouseholds = g("Having_latrine_facility_within_the_premises_Total_Households");
        cd.graduates = g("Graduate_Education"); cd.higherEducation = g("Higher_Education"); cd.secondaryEducation = g("Secondary_Education");
        cd.age0_29 = g("Age_Group_0_29"); cd.age30_49 = g("Age_Group_30_49"); cd.age50plus = g("Age_Group_50");
        m_districtById.insert(d.id, i);
        if (d.stateIndex >= 0) m_states[d.stateIndex].districts << i;
        m_districts << d;
        // grid index
        const QRectF &bb = f.bbox;
        for (double x = std::floor(bb.left() / kCell) * kCell; x <= bb.right(); x += kCell)
            for (double y = std::floor(bb.top() / kCell) * kCell; y <= bb.bottom(); y += kCell)
                m_districtGrid[cellKey(x + 1e-9, y + 1e-9)] << i;
    }
    return true;
}

int GeoDatabase::districtAt(const QPointF &ll) const
{
    const auto it = m_districtGrid.constFind(cellKey(ll.x(), ll.y()));
    if (it == m_districtGrid.constEnd()) return -1;
    for (int i : *it)
        if (m_districtFeatures[i].contains(ll)) return i;
    return -1;
}

int GeoDatabase::stateAt(const QPointF &ll) const
{
    const int d = districtAt(ll);
    if (d >= 0) return m_districts[d].stateIndex;
    for (const auto &s : m_states)
        if (s.feature->contains(ll)) return s.index;
    return -1;
}

double GeoDatabase::distanceToCoastKm(const QPointF &ll) const
{
    double best = 1e18;
    for (const auto &f : m_coast)
        for (const auto &l : f.lines) {
            // quick reject using bbox distance
            const QRectF bb = l.boundingRect().adjusted(-3, -3, 3, 3);
            if (!bb.contains(ll) && best < 300) continue;
            best = std::min(best, geo::pointPolylineKm(ll, QVector<QPointF>(l.begin(), l.end())));
        }
    return best;
}

double GeoDatabase::distanceToRiverKm(const QPointF &ll, int maxScaleRank) const
{
    double best = 1e18;
    for (const auto &f : m_rivers) {
        if (f.properties.value("scalerank").toInt(99) > maxScaleRank) continue;
        if (f.properties.value("featurecla").toString().contains("Lake")) continue;
        for (const auto &l : f.lines) {
            const QRectF bb = l.boundingRect().adjusted(-2, -2, 2, 2);
            if (!bb.contains(ll) && best < 200) continue;
            best = std::min(best, geo::pointPolylineKm(ll, QVector<QPointF>(l.begin(), l.end())));
        }
    }
    for (const auto &f : m_lakes)
        if (f.contains(ll)) return 0;
    return best;
}

QString GeoDatabase::physicalRegionAt(const QPointF &ll, const QString &cls) const
{
    for (const auto &f : m_physical) {
        if (!cls.isEmpty() && f.properties.value("featurecla").toString() != cls) continue;
        if (f.contains(ll)) return f.properties.value("name").toString();
    }
    return {};
}

bool GeoDatabase::isMountainous(const QPointF &ll) const
{
    if (!physicalRegionAt(ll, "Range/mtn").isEmpty()) return true;
    // Himalayan / north-eastern hill states are treated as mountainous terrain
    // (Natural Earth range polygons are coarse). Documented simplification.
    const int s = stateAt(ll);
    if (s < 0) return false;
    static const QStringList hill = {"Jammu & Kashmir", "Ladakh", "Himachal Pradesh", "Uttarakhand", "Sikkim",
                                     "Arunachal Pradesh", "Meghalaya", "Mizoram", "Nagaland", "Manipur"};
    return hill.contains(m_states[s].name);
}

bool GeoDatabase::isDesert(const QPointF &ll) const
{
    return !physicalRegionAt(ll, "Desert").isEmpty();
}

QVector<QPair<QString, QPointF>> GeoDatabase::riverCrossings(const QPointF &a, const QPointF &b) const
{
    QVector<QPair<QString, QPointF>> out;
    const QRectF seg = QRectF(a, b).normalized().adjusted(-0.01, -0.01, 0.01, 0.01);
    for (const auto &f : m_rivers) {
        if (f.properties.value("featurecla").toString().contains("Lake")) continue;
        for (const auto &l : f.lines) {
            if (!l.boundingRect().adjusted(-0.01, -0.01, 0.01, 0.01).intersects(seg)) continue;
            for (int i = 1; i < l.size(); ++i) {
                QPointF at;
                if (geo::segmentsIntersect(a, b, l[i - 1], l[i], &at))
                    out.append({f.properties.value("name").toString(), at});
            }
        }
    }
    return out;
}

QVector<SearchHit> GeoDatabase::search(const QString &text, int maxHits) const
{
    QVector<SearchHit> hits;
    const QString t = text.trimmed().toLower();
    if (t.isEmpty()) return hits;
    auto score = [&](const QString &name) {
        const QString n = name.toLower();
        if (n == t) return 0;
        if (n.startsWith(t)) return 1;
        if (n.contains(t)) return 2;
        return -1;
    };
    QVector<QPair<int, SearchHit>> scored;
    for (const auto &s : m_states) {
        int sc = score(s.name);
        if (sc < 0 && s.id.toLower() == t) sc = 0;
        if (sc >= 0) scored.append({sc, {SearchHit::StateHit, s.index, s.name + " (" + s.type + ")", s.centroid, s.feature->bbox}});
    }
    for (const auto &d : m_districts) {
        const int sc = score(d.name);
        if (sc >= 0) scored.append({sc + 1, {SearchHit::DistrictHit, d.index, d.name + " district, " + d.stateName, d.centroid, d.feature->bbox}});
    }
    for (int i = 0; i < m_physical.size(); ++i) {
        const QString n = m_physical[i].properties.value("name").toString();
        const int sc = score(n);
        if (sc >= 0) scored.append({sc + 2, {SearchHit::PhysicalHit, i, n + " (" + m_physical[i].properties.value("featurecla").toString() + ")", m_physical[i].bbox.center(), m_physical[i].bbox}});
    }
    for (int i = 0; i < m_rivers.size(); ++i) {
        const QString n = m_rivers[i].properties.value("name").toString();
        if (n.isEmpty()) continue;
        const int sc = score(n);
        if (sc >= 0) scored.append({sc + 2, {SearchHit::PhysicalHit, -1 - i, n + " (river)", m_rivers[i].bbox.center(), m_rivers[i].bbox}});
    }
    std::stable_sort(scored.begin(), scored.end(), [](auto &a, auto &b) { return a.first < b.first; });
    for (int i = 0; i < scored.size() && hits.size() < maxHits; ++i) hits << scored[i].second;
    return hits;
}

} // namespace isim
