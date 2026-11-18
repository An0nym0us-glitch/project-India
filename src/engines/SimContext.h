#pragma once
#include "core/ModelParameters.h"
#include "core/Modifiers.h"
#include "sim/WorldState.h"
#include <QHash>
#include <QPointF>
#include <QSet>
#include <QVector>

namespace isim {
class GeoDatabase;
class Catalog;
class PolicyLibrary;
struct FacilityDefinition;

// Derived network topology (rebuilt when facilities change).
struct NetworkModel {
    bool dirty = true;
    int transportComponents = 0;
    QVector<int> powerIslandOfDistrict;   // -1: district has no grid connection point
    QVector<int> waterNetOfDistrict;      // -1: no piped water network
    int powerIslands = 0, waterNets = 0;
    QHash<int, QVector<QPointF>> crossingCache;      // facility uid -> river crossing points
    QVector<QPair<int, QPointF>> unbridgedCrossings;  // (facility uid, point)
    QHash<int, QSet<int>> componentGateways;          // transport component -> facility uids of ports/airports
};

// Air-quality grid (concentrations in µg/m3; CO in mg/m3).
struct EnvironmentGrid {
    int nx = 0, ny = 0;
    double lon0 = 66.0, lat0 = 6.0, cell = 0.25;
    QVector<int> cellDistrict;            // district index per cell (-1 outside India)
    QVector<float> pm25, pm10, no2, so2, co, o3;
    QVector<float> emPm25, emPm10, emNOx, emSO2, emCO, emVOC; // t/yr per cell (last month)
    bool initialised() const { return nx > 0; }
    int idx(int x, int y) const { return y * nx + x; }
    bool cellOf(const QPointF &ll, int &x, int &y) const {
        x = int((ll.x() - lon0) / cell); y = int((ll.y() - lat0) / cell);
        return x >= 0 && y >= 0 && x < nx && y < ny;
    }
    QPointF cellCentre(int x, int y) const { return {lon0 + (x + 0.5) * cell, lat0 + (y + 0.5) * cell}; }
};

struct SimContext {
    WorldState &w;
    const GeoDatabase &geo;
    const Catalog &cat;
    const PolicyLibrary &policies;
    ModelParameters &p;
    Modifiers &mods;
    NetworkModel &net;
    EnvironmentGrid &env;
    bool fastMode = false; // projections: cheaper environment solve, fewer notifications
    const FacilityDefinition *def(const PlacedFacility &f) const;
};

} // namespace isim
