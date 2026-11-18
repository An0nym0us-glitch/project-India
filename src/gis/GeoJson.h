#pragma once
#include <QJsonObject>
#include <QPainterPath>
#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <QVector>

namespace isim {

// A GeoJSON feature in lon/lat coordinates (x = longitude, y = latitude).
struct GeoFeature {
    QJsonObject properties;
    QVector<QVector<QPolygonF>> polygons; // polygons -> rings (first ring = outer)
    QVector<QPolygonF> lines;             // LineString / MultiLineString parts
    QPainterPath path;                    // fill path (odd-even) for polygons
    QRectF bbox;
    bool isPolygon() const { return !polygons.isEmpty(); }
    bool contains(const QPointF &lonlat) const;
};

QVector<GeoFeature> loadGeoJson(const QString &path, QString *error = nullptr);

} // namespace isim
