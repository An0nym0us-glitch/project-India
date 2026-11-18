#pragma once
#include "GeoJson.h"
#include <QHash>
#include <QString>
#include <QVector>

namespace isim {

struct CensusDistrict2011 {
    double population = 0, male = 0, female = 0, literate = 0, workers = 0;
    double cultivators = 0, agriLabourers = 0, householdIndustry = 0, otherWorkers = 0;
    double households = 0, ruralHouseholds = 0, urbanHouseholds = 0;
    double electrifiedHouseholds = 0, internetHouseholds = 0, tapWaterHouseholds = 0, latrineHouseholds = 0;
    double graduates = 0, higherEducation = 0, secondaryEducation = 0;
    double age0_29 = 0, age30_49 = 0, age50plus = 0;
};

struct District {
    int index = -1;
    QString id, name, stateId, stateName;
    int stateIndex = -1;
    double areaKm2 = 0;
    QPointF centroid;
    const GeoFeature *feature = nullptr;
    CensusDistrict2011 census;
};

struct State {
    int index = -1;
    QString id, name, type;
    double areaKm2 = 0;
    QPointF centroid;
    const GeoFeature *feature = nullptr;
    QVector<int> districts;
    double census2011Population = 0;
};

struct SearchHit {
    enum Kind { StateHit, DistrictHit, PhysicalHit } kind;
    int index;
    QString label;
    QPointF location;
    QRectF bounds;
};

// Read-only geographic reference data (physical geography + administrative units).
class GeoDatabase {
public:
    GeoDatabase() = default;
    GeoDatabase(const GeoDatabase &) = delete;
    GeoDatabase &operator=(const GeoDatabase &) = delete;
    bool load(const QString &geoDir, QString *error = nullptr);

    const QVector<State> &states() const { return m_states; }
    const QVector<District> &districts() const { return m_districts; }
    const QVector<GeoFeature> &stateFeatures() const { return m_stateFeatures; }
    const QVector<GeoFeature> &districtFeatures() const { return m_districtFeatures; }
    const QVector<GeoFeature> &rivers() const { return m_rivers; }
    const QVector<GeoFeature> &lakes() const { return m_lakes; }
    const QVector<GeoFeature> &physicalRegions() const { return m_physical; }
    const QVector<GeoFeature> &neighbours() const { return m_neighbours; }
    const QVector<GeoFeature> &coastline() const { return m_coast; }
    const GeoFeature *indiaOutline() const { return m_outline.isEmpty() ? nullptr : &m_outline.first(); }
    QRectF bounds() const { return m_bounds; }

    int districtAt(const QPointF &lonlat) const;
    int stateAt(const QPointF &lonlat) const;
    int stateIndexById(const QString &id) const { return m_stateById.value(id, -1); }
    int stateIndexByName(const QString &name) const { return m_stateByName.value(name.toLower(), -1); }
    int districtIndexById(const QString &id) const { return m_districtById.value(id, -1); }
    bool isInIndia(const QPointF &lonlat) const { return districtAt(lonlat) >= 0; }

    // Physical suitability helpers (km)
    double distanceToCoastKm(const QPointF &lonlat) const;
    double distanceToRiverKm(const QPointF &lonlat, int maxScaleRank = 99) const;
    QString physicalRegionAt(const QPointF &lonlat, const QString &featureClass = QString()) const;
    bool isMountainous(const QPointF &lonlat) const;
    bool isDesert(const QPointF &lonlat) const;
    // Rivers crossed by segment a-b (river names); used for bridge requirements.
    QVector<QPair<QString, QPointF>> riverCrossings(const QPointF &a, const QPointF &b) const;

    QVector<SearchHit> search(const QString &text, int maxHits = 30) const;

private:
    QVector<GeoFeature> m_stateFeatures, m_districtFeatures, m_rivers, m_lakes, m_physical, m_neighbours, m_coast, m_outline;
    QVector<State> m_states;
    QVector<District> m_districts;
    QHash<QString, int> m_stateById, m_stateByName, m_districtById;
    QRectF m_bounds;
    // coarse grid index for districtAt
    static constexpr double kCell = 0.5;
    QHash<qint64, QVector<int>> m_districtGrid;
    qint64 cellKey(double lon, double lat) const;
};

} // namespace isim
