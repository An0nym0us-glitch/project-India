#pragma once
#include "SimContext.h"
#include <QPointF>
#include <QVector>

namespace isim {

struct PlacementRequest {
    QString defId;
    QPointF location;          // point facilities
    QVector<QPointF> path;     // line facilities
    QString label;
};

struct PlacementResult {
    bool ok = false;
    QString error;
    int uid = -1;
    double capexCr = 0, publicCostCr = 0;
};

// Places facilities, advances construction, pays capital costs and commissions assets.
class ConstructionEngine {
public:
    static PlacementResult place(SimContext &ctx, const PlacementRequest &req);
    static bool demolish(SimContext &ctx, int uid);
    // Effective capex (₹ crore) including policy cost multipliers, for a given scale (km for lines).
    static double effectiveCapex(const SimContext &ctx, const FacilityDefinition &d, double scale);
    static double publicShareFor(const SimContext &ctx, const FacilityDefinition &d);
    void step(SimContext &ctx, double &publicCapexPaid, double &privateCapexPaid);
};

} // namespace isim
