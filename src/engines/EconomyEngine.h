#pragma once
#include "SimContext.h"

namespace isim {

// Workforce allocation, facility efficiency, supply chains & trade, GDP,
// employment and public finances.
class EconomyEngine {
public:
    // Phase 1 (before utilities): staffing, inputs, transport, technology, supporting industry.
    void prepare(SimContext &ctx);
    // Phase 2 (after utilities): final efficiency, outputs, GDP, jobs, budget.
    void finalize(SimContext &ctx, double publicCapexPaid, double privateCapexPaid, bool applyBudget = true);

    // Technology readiness factor for a facility definition given current maturity.
    static double techFactor(const SimContext &ctx, const FacilityDefinition &d, QStringList *why = nullptr);
    // Count of operating facilities carrying a tag within radius (0 = national).
    static int countTagged(const SimContext &ctx, const QString &tag, const QPointF &at, double radiusKm, bool includeUnderConstruction = false);
    static double supportFactor(const SimContext &ctx, const FacilityDefinition &d, const QPointF &at, QStringList *why = nullptr);
    static double transportScore(const FacilityDefinition &d, const PlacedFacility &f, QStringList *missing = nullptr);

private:
    QHash<QString, double> m_availability; // good -> 0..1 supply availability (lagged one month)
    QVector<SkillVector> m_fill;           // per state fill ratio per skill
};

} // namespace isim
