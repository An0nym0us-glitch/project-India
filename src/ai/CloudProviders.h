#pragma once
#include <functional>
#include "AIProvider.h"
#include <QElapsedTimer>
#include <QHash>
#include <QJsonObject>
#include <QPointer>

class QNetworkAccessManager;
class QNetworkReply;

namespace isim {

// OpenAI-compatible chat-completions protocol (also used for the local llama.cpp server).
struct OpenAIProtocol {
    static QJsonObject buildBody(const AIRequest &r, const QString &model, bool forceJson);
    static bool parseResponse(const QByteArray &body, QString &text, QString &error);
};

struct GeminiProtocol {
    static QJsonObject buildBody(const AIRequest &r);
    static bool parseResponse(const QByteArray &body, QString &text, QString &error);
};

class HttpProviderBase : public AIProvider {
    Q_OBJECT
public:
    explicit HttpProviderBase(QObject *parent = nullptr);
    void cancel() override;
protected:
    void post(const AIRequest &r, const QUrl &url, const QJsonObject &body, const QList<QPair<QByteArray, QByteArray>> &headers,
              const std::function<bool(const QByteArray &, QString &, QString &)> &parse, int timeoutMs = 120000);
    QNetworkAccessManager *m_nam;
    QHash<QNetworkReply *, QString> m_pending;
};

class OpenAIProvider : public HttpProviderBase {
    Q_OBJECT
public:
    explicit OpenAIProvider(QObject *parent = nullptr);
    QString id() const override { return QStringLiteral("openai"); }
    QString displayName() const override { return QStringLiteral("OpenAI (cloud, optional)"); }
    bool isLocal() const override { return false; }
    bool isConfigured() const override;
    void start() override;
    void complete(const AIRequest &r) override;
    static const char *secretKey() { return "openai_api_key"; }
};

class GeminiProvider : public HttpProviderBase {
    Q_OBJECT
public:
    explicit GeminiProvider(QObject *parent = nullptr);
    QString id() const override { return QStringLiteral("gemini"); }
    QString displayName() const override { return QStringLiteral("Google Gemini (cloud, optional)"); }
    bool isLocal() const override { return false; }
    bool isConfigured() const override;
    void start() override;
    void complete(const AIRequest &r) override;
    static const char *secretKey() { return "gemini_api_key"; }
};

} // namespace isim
