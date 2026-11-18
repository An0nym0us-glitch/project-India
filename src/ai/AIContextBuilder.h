#pragma once
#include <QJsonObject>
#include <QString>

namespace isim {
class Simulation;
struct FacilityDefinition;
struct SuitabilityReport;
struct Recommendation;
struct PolicyDefinition;
struct ProjectionResult;

// Builds compact, structured context (JSON) from simulation results for the language model.
// Every number the model may cite comes from here.
class AIContextBuilder {
public:
    static QJsonObject nationalSnapshot(const Simulation &sim);
    static QJsonObject facility(const Simulation &sim, const FacilityDefinition &d, const SuitabilityReport *rep);
    static QJsonObject policy(const Simulation &sim, const PolicyDefinition &p);
    static QJsonObject recommendation(const Simulation &sim, const Recommendation &r);
    static QJsonObject projections(const QVector<ProjectionResult> &runs);
    static QString systemPrompt();
    static QString taskPrompt(const QString &task, const QJsonObject &context, const QString &userQuestion = {});
};

} // namespace isim
