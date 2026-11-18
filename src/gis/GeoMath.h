#pragma once
#include <QPointF>
#include <QVector>
#include <cmath>

namespace isim::geo {
constexpr double kEarthRadiusKm = 6371.0088;
constexpr double kPi = 3.14159265358979323846;
inline double deg2rad(double d) { return d * kPi / 180.0; }

// Great-circle distance between two lon/lat points (x = lon, y = lat).
inline double haversineKm(const QPointF &a, const QPointF &b)
{
    const double dLat = deg2rad(b.y() - a.y()), dLon = deg2rad(b.x() - a.x());
    const double s = std::sin(dLat / 2) * std::sin(dLat / 2) +
                     std::cos(deg2rad(a.y())) * std::cos(deg2rad(b.y())) * std::sin(dLon / 2) * std::sin(dLon / 2);
    return 2 * kEarthRadiusKm * std::asin(std::min(1.0, std::sqrt(s)));
}

// Local equirectangular projection to km around a reference latitude.
inline QPointF toKm(const QPointF &p, double refLat)
{
    return {p.x() * 111.32 * std::cos(deg2rad(refLat)), p.y() * 110.57};
}

// Distance (km) from point p to segment ab, all lon/lat.
inline double pointSegmentKm(const QPointF &p, const QPointF &a, const QPointF &b)
{
    const double ref = p.y();
    const QPointF P = toKm(p, ref), A = toKm(a, ref), B = toKm(b, ref);
    const QPointF AB = B - A, AP = P - A;
    const double len2 = AB.x() * AB.x() + AB.y() * AB.y();
    double t = len2 > 0 ? (AP.x() * AB.x() + AP.y() * AB.y()) / len2 : 0;
    t = std::max(0.0, std::min(1.0, t));
    const QPointF C = A + AB * t;
    const QPointF d = P - C;
    return std::sqrt(d.x() * d.x() + d.y() * d.y());
}

inline double polylineLengthKm(const QVector<QPointF> &pts)
{
    double s = 0;
    for (int i = 1; i < pts.size(); ++i) s += haversineKm(pts[i - 1], pts[i]);
    return s;
}

inline double pointPolylineKm(const QPointF &p, const QVector<QPointF> &line)
{
    double best = 1e18;
    if (line.size() == 1) return haversineKm(p, line[0]);
    for (int i = 1; i < line.size(); ++i) best = std::min(best, pointSegmentKm(p, line[i - 1], line[i]));
    return best;
}

// Segment intersection test in lon/lat plane (adequate for short segments).
inline bool segmentsIntersect(const QPointF &p1, const QPointF &p2, const QPointF &q1, const QPointF &q2, QPointF *at = nullptr)
{
    const QPointF r = p2 - p1, s = q2 - q1;
    const double rxs = r.x() * s.y() - r.y() * s.x();
    if (std::abs(rxs) < 1e-12) return false;
    const QPointF qp = q1 - p1;
    const double t = (qp.x() * s.y() - qp.y() * s.x()) / rxs;
    const double u = (qp.x() * r.y() - qp.y() * r.x()) / rxs;
    if (t < 0 || t > 1 || u < 0 || u > 1) return false;
    if (at) *at = p1 + r * t;
    return true;
}
} // namespace isim::geo
