#include "AIResponseValidator.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <cmath>

namespace isim {

QString AIResponseValidator::stripThinking(const QString &text)
{
    QString t = text;
    static const QRegularExpression think(QStringLiteral("<think>.*?</think>"), QRegularExpression::DotMatchesEverythingOption);
    t.remove(think);
    t.remove(QStringLiteral("<think>"));
    return t.trimmed();
}

QString AIResponseValidator::extractJsonObject(const QString &text)
{
    QString t = stripThinking(text);
    // remove markdown fences
    static const QRegularExpression fence(QStringLiteral("```(?:json)?"));
    t.remove(fence);
    const int a = t.indexOf('{');
    const int b = t.lastIndexOf('}');
    if (a < 0 || b <= a) return {};
    return t.mid(a, b - a + 1);
}

QVector<double> AIResponseValidator::numbersIn(const QString &text)
{
    QVector<double> out;
    static const QRegularExpression re(QStringLiteral("(?<![A-Za-z_])-?\\d[\\d,]*(?:\\.\\d+)?"));
    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        QString s = it.next().captured(0);
        s.remove(',');
        bool ok = false;
        const double v = s.toDouble(&ok);
        if (ok) out << v;
    }
    return out;
}

bool AIResponseValidator::numberGrounded(double v, const QVector<double> &ctx)
{
    const double a = std::abs(v);
    if (a <= 10 && std::floor(a) == a) return true;           // small counts / list numbering
    if (a >= 1900 && a <= 2200 && std::floor(a) == a) return true; // years
    for (double c : ctx) {
        const double ca = std::abs(c);
        if (ca == 0 && a == 0) return true;
        // equal within 1 %, or a rounded/rescaled form (x100 for %, /1e3,/1e5,/1e6,/1e7,/1e9 for units)
        for (double k : {1.0, 100.0, 0.01, 1e-3, 1e-5, 1e-6, 1e-7, 1e-9, 1e3}) {
            const double cc = ca * k;
            if (cc == 0) continue;
            if (std::abs(cc - a) / cc < 0.01 || std::abs(cc - a) < 0.051) return true;
        }
    }
    return false;
}

ValidatedResponse AIResponseValidator::validate(const QString &text, const QString &contextJson)
{
    ValidatedResponse r;
    r.rawText = stripThinking(text);
    const QString js = extractJsonObject(text);
    if (js.isEmpty()) { r.problems << QStringLiteral("Response is not a JSON object."); r.summary = r.rawText; }
    else {
        QJsonParseError pe;
        const auto doc = QJsonDocument::fromJson(js.toUtf8(), &pe);
        if (pe.error != QJsonParseError::NoError || !doc.isObject()) r.problems << QStringLiteral("Invalid JSON: %1").arg(pe.errorString());
        else {
            const auto o = doc.object();
            auto list = [&](const char *k, QStringList &dst) {
                const auto v = o.value(k);
                if (v.isUndefined()) return;
                if (!v.isArray()) { r.problems << QStringLiteral("Field '%1' must be a list").arg(k); return; }
                for (const auto &x : v.toArray()) if (x.isString() && !x.toString().trimmed().isEmpty()) dst << x.toString().trimmed();
            };
            if (!o.value("summary").isString() || o.value("summary").toString().trimmed().isEmpty()) r.problems << QStringLiteral("Missing 'summary'.");
            r.summary = o.value("summary").toString().trimmed();
            list("key_points", r.keyPoints); list("risks", r.risks); list("missing_assumptions", r.missingAssumptions); list("alternatives", r.alternatives);
            r.structured = r.problems.isEmpty();
        }
    }
    // numeric grounding: every number in the explanation must come from the context
    const auto ctxNums = numbersIn(contextJson);
    QStringList all{r.summary};
    all << r.keyPoints << r.risks << r.missingAssumptions << r.alternatives;
    static const QRegularExpression re(QStringLiteral("(?<![A-Za-z_])-?\\d[\\d,]*(?:\\.\\d+)?"));
    for (const auto &s : all) {
        auto it = re.globalMatch(s);
        while (it.hasNext()) {
            const QString tok = it.next().captured(0);
            QString clean = tok; clean.remove(',');
            const double v = clean.toDouble();
            if (!numberGrounded(v, ctxNums) && !r.unverifiedNumbers.contains(tok)) r.unverifiedNumbers << tok;
        }
    }
    return r;
}

} // namespace isim
