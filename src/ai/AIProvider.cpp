#include "AIProvider.h"
#include <QUuid>

namespace isim {

QString aiStatusName(AIStatus s)
{
    switch (s) {
    case AIStatus::NotConfigured: return QStringLiteral("Not configured");
    case AIStatus::Stopped: return QStringLiteral("Stopped");
    case AIStatus::Loading: return QStringLiteral("Loading");
    case AIStatus::Ready: return QStringLiteral("Ready");
    case AIStatus::Busy: return QStringLiteral("Generating");
    case AIStatus::Error: return QStringLiteral("Error");
    }
    return {};
}

void AIProvider::setStatus(AIStatus s, const QString &text)
{
    m_status = s;
    m_statusText = text;
    emit statusChanged(s, text);
}

void AIProvider::test()
{
    AIRequest r;
    r.id = "test-" + QUuid::createUuid().toString(QUuid::Id128).left(8);
    r.task = "test";
    r.systemPrompt = "You are a connectivity test. Reply with a JSON object {\"summary\": \"ok\"}.";
    r.userPrompt = "Reply now.";
    r.maxTokens = 30;
    auto *ctx = new QObject(this);
    connect(this, &AIProvider::finished, ctx, [this, ctx, id = r.id](const QString &rid, const QString &text, double secs) {
        if (rid != id) return;
        emit testFinished(true, QStringLiteral("Response received in %1 s: %2").arg(secs, 0, 'f', 1).arg(text.left(120)));
        ctx->deleteLater();
    });
    connect(this, &AIProvider::failed, ctx, [this, ctx, id = r.id](const QString &rid, const QString &err) {
        if (rid != id) return;
        emit testFinished(false, err);
        ctx->deleteLater();
    });
    complete(r);
}

} // namespace isim
