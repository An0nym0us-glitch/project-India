#include "CloudProviders.h"
#include "AppSettings.h"
#include "SecretStore.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

namespace isim {

QJsonObject OpenAIProtocol::buildBody(const AIRequest &r, const QString &model, bool forceJson)
{
    QJsonObject b{{"model", model}, {"temperature", r.temperature}, {"max_tokens", r.maxTokens},
                  {"messages", QJsonArray{QJsonObject{{"role", "system"}, {"content", r.systemPrompt}},
                                          QJsonObject{{"role", "user"}, {"content", r.userPrompt}}}}};
    if (r.jsonMode && forceJson) b.insert("response_format", QJsonObject{{"type", "json_object"}});
    return b;
}

bool OpenAIProtocol::parseResponse(const QByteArray &body, QString &text, QString &error)
{
    const auto o = QJsonDocument::fromJson(body).object();
    if (o.contains("error")) {
        const auto e = o.value("error");
        error = e.isObject() ? e.toObject().value("message").toString() : e.toString();
        return false;
    }
    const auto choices = o.value("choices").toArray();
    if (choices.isEmpty()) { error = QStringLiteral("No choices in response"); return false; }
    text = choices.first().toObject().value("message").toObject().value("content").toString();
    return true;
}

QJsonObject GeminiProtocol::buildBody(const AIRequest &r)
{
    QJsonObject gen{{"temperature", r.temperature}, {"maxOutputTokens", r.maxTokens}};
    if (r.jsonMode) gen.insert("responseMimeType", "application/json");
    return QJsonObject{{"systemInstruction", QJsonObject{{"parts", QJsonArray{QJsonObject{{"text", r.systemPrompt}}}}}},
                       {"contents", QJsonArray{QJsonObject{{"role", "user"}, {"parts", QJsonArray{QJsonObject{{"text", r.userPrompt}}}}}}},
                       {"generationConfig", gen}};
}

bool GeminiProtocol::parseResponse(const QByteArray &body, QString &text, QString &error)
{
    const auto o = QJsonDocument::fromJson(body).object();
    if (o.contains("error")) { error = o.value("error").toObject().value("message").toString(); return false; }
    const auto cands = o.value("candidates").toArray();
    if (cands.isEmpty()) {
        error = o.value("promptFeedback").toObject().value("blockReason").toString("No candidates in response");
        return false;
    }
    for (const auto &p : cands.first().toObject().value("content").toObject().value("parts").toArray())
        text += p.toObject().value("text").toString();
    return true;
}

HttpProviderBase::HttpProviderBase(QObject *parent) : AIProvider(parent), m_nam(new QNetworkAccessManager(this)) {}

void HttpProviderBase::cancel()
{
    for (auto *r : m_pending.keys()) r->abort();
}

void HttpProviderBase::post(const AIRequest &r, const QUrl &url, const QJsonObject &body, const QList<QPair<QByteArray, QByteArray>> &headers,
                            const std::function<bool(const QByteArray &, QString &, QString &)> &parse, int timeoutMs)
{
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    for (const auto &h : headers) req.setRawHeader(h.first, h.second);
    req.setTransferTimeout(timeoutMs);
    auto *reply = m_nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    m_pending.insert(reply, r.id);
    auto *timer = new QElapsedTimer; timer->start();
    const AIStatus prev = m_status;
    setStatus(AIStatus::Busy, QStringLiteral("Generating…"));
    connect(reply, &QNetworkReply::finished, this, [this, reply, timer, parse, prev, id = r.id]() {
        const double secs = timer->elapsed() / 1000.0;
        delete timer;
        m_pending.remove(reply);
        const QByteArray data = reply->readAll();
        QString text, err;
        if (reply->error() != QNetworkReply::NoError && data.isEmpty()) err = reply->errorString();
        else if (!parse(data, text, err) && err.isEmpty()) err = reply->errorString();
        reply->deleteLater();
        setStatus(prev == AIStatus::Busy ? AIStatus::Ready : (prev == AIStatus::Error ? AIStatus::Ready : prev), aiStatusName(AIStatus::Ready));
        if (!err.isEmpty()) emit failed(id, err);
        else emit finished(id, text, secs);
    });
}

// ---------------- OpenAI ----------------
OpenAIProvider::OpenAIProvider(QObject *parent) : HttpProviderBase(parent) { start(); }
bool OpenAIProvider::isConfigured() const { return !SecretStore::read(secretKey()).isEmpty(); }
void OpenAIProvider::start()
{
    if (isConfigured()) setStatus(AIStatus::Ready, QStringLiteral("API key stored (%1)").arg(SecretStore::backendName()));
    else setStatus(AIStatus::NotConfigured, QStringLiteral("No API key. Add one in Settings → AI Providers."));
}
void OpenAIProvider::complete(const AIRequest &r)
{
    const QString key = SecretStore::read(secretKey());
    if (key.isEmpty()) { emit failed(r.id, "OpenAI API key is not configured."); return; }
    QString ep = AppSettings::openAIEndpoint();
    if (ep.endsWith('/')) ep.chop(1);
    post(r, QUrl(ep + "/chat/completions"), OpenAIProtocol::buildBody(r, AppSettings::openAIModel(), true),
         {{"Authorization", "Bearer " + key.toUtf8()}}, &OpenAIProtocol::parseResponse);
}

// ---------------- Gemini ----------------
GeminiProvider::GeminiProvider(QObject *parent) : HttpProviderBase(parent) { start(); }
bool GeminiProvider::isConfigured() const { return !SecretStore::read(secretKey()).isEmpty(); }
void GeminiProvider::start()
{
    if (isConfigured()) setStatus(AIStatus::Ready, QStringLiteral("API key stored (%1)").arg(SecretStore::backendName()));
    else setStatus(AIStatus::NotConfigured, QStringLiteral("No API key. Add one in Settings → AI Providers."));
}
void GeminiProvider::complete(const AIRequest &r)
{
    const QString key = SecretStore::read(secretKey());
    if (key.isEmpty()) { emit failed(r.id, "Gemini API key is not configured."); return; }
    const QUrl url(QStringLiteral("https://generativelanguage.googleapis.com/v1beta/models/%1:generateContent").arg(AppSettings::geminiModel()));
    post(r, url, GeminiProtocol::buildBody(r), {{"x-goog-api-key", key.toUtf8()}}, &GeminiProtocol::parseResponse);
}

} // namespace isim
