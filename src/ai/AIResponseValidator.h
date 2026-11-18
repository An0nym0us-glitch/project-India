#pragma once
#include <QJsonObject>
#include <QSet>
#include <QString>
#include <QStringList>

namespace isim {

struct ValidatedResponse {
    bool structured = false;     // valid JSON matching the schema
    QString summary;
    QStringList keyPoints, risks, missingAssumptions, alternatives;
    QStringList unverifiedNumbers; // numbers in the text that do not appear in the provided context
    QStringList problems;          // validation problems
    QString rawText;
    bool acceptable() const { return structured && problems.isEmpty(); }
};

// Validates model output: strips reasoning tags, extracts the JSON object, checks the
// schema, and flags any number that is not present in the simulation context.
class AIResponseValidator {
public:
    static QString stripThinking(const QString &text);
    static QString extractJsonObject(const QString &text);
    static QVector<double> numbersIn(const QString &text);
    static ValidatedResponse validate(const QString &text, const QString &contextJson);
    static bool numberGrounded(double v, const QVector<double> &context);
};

} // namespace isim
