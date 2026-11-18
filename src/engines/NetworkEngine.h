#pragma once
#include "SimContext.h"

namespace isim {
// Transport connectivity (road/rail/waterway/air/sea/fibre), power-grid islands
// and piped-water networks. Disconnected facilities do not get network benefits.
class NetworkEngine {
public:
    void rebuild(SimContext &ctx);
    // Distance (km) from point to the nearest operating, unbroken line of a network; 1e18 if none.
    static double distanceToNetwork(const SimContext &ctx, const QPointF &ll, const QString &network, int *lineIndex = nullptr);
};
}
