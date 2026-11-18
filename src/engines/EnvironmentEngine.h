#pragma once
#include "Aqi.h"
#include "SimContext.h"

namespace isim {

// Simplified regional air-quality model: gridded emissions, advection by seasonal
// prevailing winds, mixing height, deposition/washout, secondary PM formation,
// and CPCB AQI per district. NOT a chemical transport model (see documentation).
class EnvironmentEngine {
public:
    void initialise(SimContext &ctx);
    void step(SimContext &ctx);
    const CpcbAqi &aqi() const { return m_aqi; }
    CpcbAqi &aqi() { return m_aqi; }
    struct Climate { double u, v, mixingHeight, washout; };
    static Climate climate(int monthOfYear);
    // Annual emissions (t/yr) attributable to one facility under current modifiers.
    static void facilityEmissions(const SimContext &ctx, const PlacedFacility &f, double out[7]);
private:
    void accumulateEmissions(SimContext &ctx);
    QVector<QVector<int>> m_districtCells;
    QVector<bool> m_desert;
    CpcbAqi m_aqi;
};

} // namespace isim
