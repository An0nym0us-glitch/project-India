#pragma once
#include <QJsonObject>
#include <QObject>
#include <QString>

namespace isim {

struct AIRequest {
    QString id;
    QString task;          // e.g. "explain_policy"
    QString systemPrompt;
    QString userPrompt;    // includes the structured simulation context
    int maxTokens = 900;
    double temperature = 0.2;
    bool jsonMode = true;
};

enum class AIStatus { NotConfigured, Stopped, Loading, Ready, Busy, Error };
QString aiStatusName(AIStatus s);

// Replaceable language-model backend. The simulator never relies on the model for
// numbers: providers only turn structured simulation results into explanations.
class AIProvider : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual QString id() const = 0;
    virtual QString displayName() const = 0;
    virtual bool isLocal() const = 0;
    virtual bool isConfigured() const = 0;
    virtual AIStatus status() const { return m_status; }
    virtual QString statusText() const { return m_statusText; }
    // Prepare the backend (load a local model / nothing for cloud providers).
    virtual void start() {}
    virtual void stop() {}
    virtual void complete(const AIRequest &req) = 0;
    virtual void cancel() {}
    // Lightweight connectivity / model test.
    virtual void test();

signals:
    void statusChanged(isim::AIStatus status, const QString &text);
    void finished(const QString &requestId, const QString &text, double seconds);
    void failed(const QString &requestId, const QString &error);
    void testFinished(bool ok, const QString &message);

protected:
    void setStatus(AIStatus s, const QString &text);
    AIStatus m_status = AIStatus::NotConfigured;
    QString m_statusText;
};

} // namespace isim
