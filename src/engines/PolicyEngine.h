#pragma once
#include "SimContext.h"
#include <QStringList>

namespace isim {

struct PolicyEffect {
    QString key, mode, note; // mode: "mul" | "add"
    double full = 0;          // value at intensity 1, effectiveness 1
};

struct PolicyDefinition {
    QString id, name, category, description, intensityLabel;
    QVector<PolicyEffect> effects;
    double costCrPerYearAtFull = 0, costPctGdpAtFull = 0;
    double effLow = 0.5, effCentral = 0.75, effHigh = 1.0;
    QStringList benefits, unintended, affectedGroups, challenges;
    QString shortTerm, longTerm, stateDifferences, uncertainty, humanRightsConcern, revenueNote;
    int lagMonths = 12, rampMonths = 36;
    bool isCoercive() const { return !humanRightsConcern.isEmpty() && humanRightsConcern.contains("coercive", Qt::CaseInsensitive); }
};

class PolicyLibrary {
public:
    bool load(const QString &path, QString *error = nullptr);
    const QVector<PolicyDefinition> &all() const { return m_defs; }
    const PolicyDefinition *find(const QString &id) const;
    QStringList categories() const;
private:
    QVector<PolicyDefinition> m_defs;
};

// Computes the month's coefficient modifiers and fiscal cost from active policies.
class PolicyEngine {
public:
    void computeModifiers(SimContext &ctx);
    // Annual fiscal cost (₹ crore) of active policies at current GDP.
    static double annualCostCr(const SimContext &ctx);
    // Realised strength (0..1) of a policy instance at the current month.
    static double realisedStrength(const SimContext &ctx, const PolicyInstance &inst, const PolicyDefinition &def);
};

} // namespace isim
