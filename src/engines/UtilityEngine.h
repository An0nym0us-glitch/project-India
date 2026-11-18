#pragma once
#include "SimContext.h"

namespace isim {

struct PowerIslandBalance {
    double supplyMW = 0, demandMW = 0, servedFraction = 0, reliability = 0;
    double variableMW = 0, firmStorageMW = 0, capacityMW = 0;
};

// Electricity, water, sewage and solid-waste balances.
class UtilityEngine {
public:
    void step(SimContext &ctx);
    const QVector<PowerIslandBalance> &islands() const { return m_islands; }
    // Seasonal output factor relative to annual capacity factor.
    static double seasonalFactor(const FacilityDefinition &d, int monthOfYear);
private:
    QVector<PowerIslandBalance> m_islands;
};

} // namespace isim
