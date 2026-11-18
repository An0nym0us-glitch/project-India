#pragma once
#include "engines/SimContext.h"
#include <QStringList>

namespace isim {

enum class CheckLevel { Ok, Warning, Problem, Blocker };

struct SuitabilityCheck {
    QString area;     // e.g. "Electricity"
    CheckLevel level = CheckLevel::Ok;
    QString detail;
};

struct PrerequisiteInvestment {
    QString facilityId;   // catalogue id (may be empty for policy suggestions)
    QString title;
    QString why;
};

enum class SuitabilityStatus { Recommended, FeasibleWithGaps, NotRecommended, NotFeasible };

struct SuitabilityReport {
    SuitabilityStatus status = SuitabilityStatus::Recommended;
    QString statusText;
    QString location;                 // "District, State"
    int district = -1;
    double estimatedEfficiency = 0;   // expected output share at current national state
    double capexCr = 0, publicCostCr = 0, lengthKm = 0;
    double localPm25IncrementUg = 0;  // rough local estimate
    QVector<SuitabilityCheck> checks;
    QStringList reasons;              // headline reasons for the status
    QVector<PrerequisiteInvestment> prerequisites;
    QString regionalSuitability;
};

QString statusName(SuitabilityStatus s);

// Pre-placement assessment of a facility at a location using current simulation data.
class SuitabilityEngine {
public:
    static SuitabilityReport assess(SimContext &ctx, const FacilityDefinition &d, const QPointF &location,
                                    const QVector<QPointF> &path = {});
    // Facilities that best address a missing capability (used by the advisor too)
    static QString facilityForSkill(int skill);
    static QStringList facilitiesForTechDomain(const SimContext &ctx, const QString &domain, int max = 2);
    static QString cheapestWithTag(const SimContext &ctx, const QString &tag);
};

} // namespace isim
