#pragma once
#include "AIProvider.h"
#include "AIResponseValidator.h"
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPointer>

namespace isim {

struct AIAnswer {
    QString requestId, task, providerName;
    ValidatedResponse response;
    double seconds = 0;
    QString error;
};

// Owns the providers, routes requests to the selected one, validates answers.
class AIManager : public QObject {
    Q_OBJECT
public:
    explicit AIManager(QObject *parent = nullptr);
    QList<AIProvider *> providers() const { return m_providers; }
    AIProvider *provider(const QString &id) const;
    AIProvider *active() const;
    void setActive(const QString &id);
    QString activeId() const { return m_active; }
    // Ask the active provider; returns request id (empty if no provider is available).
    QString ask(const QString &task, const QJsonObject &context, const QString &question = {});
    bool available() const;

signals:
    void answered(const isim::AIAnswer &answer);
    void activeChanged(const QString &id);
    void providerStatusChanged();

private:
    QList<AIProvider *> m_providers;
    QString m_active;
    struct Pending { QString task, context; int attempts = 0; QString question; QJsonObject ctxObj; };
    QHash<QString, Pending> m_pending;
};

} // namespace isim
