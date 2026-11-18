#include "AIManager.h"
#include "AIContextBuilder.h"
#include "AppSettings.h"
#include "CloudProviders.h"
#include "LocalLlamaProvider.h"
#include <QJsonDocument>
#include <QUuid>

namespace isim {

AIManager::AIManager(QObject *parent) : QObject(parent)
{
    m_providers << new LocalLlamaProvider(this) << new OpenAIProvider(this) << new GeminiProvider(this);
    for (auto *p : m_providers) {
        connect(p, &AIProvider::statusChanged, this, &AIManager::providerStatusChanged);
        connect(p, &AIProvider::finished, this, [this, p](const QString &id, const QString &text, double secs) {
            if (!m_pending.contains(id)) return;
            auto pend = m_pending.value(id);
            AIAnswer a;
            a.requestId = id; a.task = pend.task; a.providerName = p->displayName(); a.seconds = secs;
            a.response = AIResponseValidator::validate(text, pend.context);
            if (!a.response.structured && pend.attempts < 1) {
                // one repair attempt asking for the JSON schema only
                pend.attempts++;
                m_pending.insert(id, pend);
                AIRequest r;
                r.id = id; r.task = pend.task; r.systemPrompt = AIContextBuilder::systemPrompt();
                r.userPrompt = AIContextBuilder::taskPrompt(pend.task, pend.ctxObj, pend.question) +
                               "\n\nYour previous reply was not valid JSON. Reply with only the JSON object.";
                p->complete(r);
                return;
            }
            m_pending.remove(id);
            emit answered(a);
        });
        connect(p, &AIProvider::failed, this, [this, p](const QString &id, const QString &err) {
            if (!m_pending.contains(id)) return;
            const auto pend = m_pending.take(id);
            AIAnswer a;
            a.requestId = id; a.task = pend.task; a.providerName = p->displayName(); a.error = err;
            emit answered(a);
        });
    }
    m_active = AppSettings::defaultProvider();
}

AIProvider *AIManager::provider(const QString &id) const
{
    for (auto *p : m_providers) if (p->id() == id) return p;
    return nullptr;
}

AIProvider *AIManager::active() const { return provider(m_active); }

void AIManager::setActive(const QString &id)
{
    m_active = id;
    AppSettings::setDefaultProvider(id);
    emit activeChanged(id);
}

bool AIManager::available() const
{
    auto *p = active();
    return p && p->isConfigured() && (p->status() == AIStatus::Ready || p->status() == AIStatus::Busy);
}

QString AIManager::ask(const QString &task, const QJsonObject &context, const QString &question)
{
    auto *p = active();
    if (!p) return {};
    AIRequest r;
    r.id = QUuid::createUuid().toString(QUuid::Id128);
    r.task = task;
    r.systemPrompt = AIContextBuilder::systemPrompt();
    r.userPrompt = AIContextBuilder::taskPrompt(task, context, question);
    m_pending.insert(r.id, {task, QString::fromUtf8(QJsonDocument(context).toJson(QJsonDocument::Compact)), 0, question, context});
    p->complete(r);
    return r.id;
}

} // namespace isim
