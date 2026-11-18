#include "GeoJson.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

namespace isim {

static QPolygonF ring(const QJsonArray &a)
{
    QPolygonF p;
    p.reserve(a.size());
    for (const auto &c : a) {
        const auto xy = c.toArray();
        p << QPointF(xy.at(0).toDouble(), xy.at(1).toDouble());
    }
    return p;
}

bool GeoFeature::contains(const QPointF &ll) const
{
    if (!bbox.contains(ll)) return false;
    for (const auto &poly : polygons) {
        if (poly.isEmpty() || !poly[0].containsPoint(ll, Qt::OddEvenFill)) continue;
        bool inHole = false;
        for (int i = 1; i < poly.size(); ++i)
            if (poly[i].containsPoint(ll, Qt::OddEvenFill)) { inHole = true; break; }
        if (!inHole) return true;
    }
    return false;
}

QVector<GeoFeature> loadGeoJson(const QString &path, QString *error)
{
    QVector<GeoFeature> out;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("Cannot open %1").arg(path);
        return out;
    }
    QJsonParseError pe;
    const auto doc = QJsonDocument::fromJson(f.readAll(), &pe);
    if (pe.error != QJsonParseError::NoError) {
        if (error) *error = pe.errorString();
        return out;
    }
    const auto feats = doc.object().value("features").toArray();
    out.reserve(feats.size());
    for (const auto &fv : feats) {
        const auto fo = fv.toObject();
        GeoFeature gf;
        gf.properties = fo.value("properties").toObject();
        const auto g = fo.value("geometry").toObject();
        const QString type = g.value("type").toString();
        const auto coords = g.value("coordinates").toArray();
        auto addPoly = [&](const QJsonArray &rings) {
            QVector<QPolygonF> poly;
            for (const auto &r : rings) poly << ring(r.toArray());
            gf.polygons << poly;
        };
        if (type == "Polygon") addPoly(coords);
        else if (type == "MultiPolygon") for (const auto &p : coords) addPoly(p.toArray());
        else if (type == "LineString") gf.lines << ring(coords);
        else if (type == "MultiLineString") for (const auto &l : coords) gf.lines << ring(l.toArray());
        else if (type == "GeometryCollection") {
            for (const auto &gv : g.value("geometries").toArray()) {
                const auto go = gv.toObject();
                const auto c2 = go.value("coordinates").toArray();
                const QString t2 = go.value("type").toString();
                if (t2 == "Polygon") addPoly(c2);
                else if (t2 == "MultiPolygon") for (const auto &p : c2) addPoly(p.toArray());
                else if (t2 == "LineString") gf.lines << ring(c2);
                else if (t2 == "MultiLineString") for (const auto &l : c2) gf.lines << ring(l.toArray());
            }
        }
        gf.path.setFillRule(Qt::OddEvenFill);
        for (const auto &poly : gf.polygons)
            for (const auto &r : poly) { gf.path.addPolygon(r); gf.path.closeSubpath(); }
        QRectF bb;
        for (const auto &poly : gf.polygons) if (!poly.isEmpty()) bb = bb.united(poly[0].boundingRect());
        for (const auto &l : gf.lines) bb = bb.united(l.boundingRect());
        gf.bbox = bb;
        out << gf;
    }
    return out;
}

} // namespace isim
