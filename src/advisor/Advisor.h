#pragma once
#include "Suitability.h"
#include <QStringList>

namespace isim {

struct EvidenceItem {
    QString label, value, source; // source: "Simulated", "World Bank 2023", ...
};

struct Recommendation {
    QString id;               // stable key (analysis + target)
    QString category;         // Electricity, Housing, ...
    QString title;
    int priority = 50;        // 0..100 (higher = more urgent)
    QString facilityId;       // primary investment (may be empty for policy recommendations)
    QString policyId;         // suggested policy (optional)
    int quantity = 1;
    QStringList locations;    // "District, State — reason"
    int targetDistrict = -1;
    QString why;
    QStringList supportingInfrastructure;
    double investmentCr = 0;
    QStringList benefits, drawbacks, risks, alternatives;
    QString opportunityCost, shortTerm, longTerm;
    QVector<EvidenceItem> evidence;
    QString decision;         // accepted | rejected | saved | ""
};

// Examines the simulated country and proposes investments, derived only from
// simulation state (and, for benchmarks, cited real-world reference data).
class Advisor {
public:
    static QVector<Recommendation> analyse(SimContext &ctx, const class ReferenceData *ref = nullptr);
};

} // namespace isim
