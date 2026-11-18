#pragma once
#include "CloudProviders.h"
#include <QProcess>
#include <QTimer>

namespace isim {

struct GgufInfo {
    bool valid = false;
    QString error;
    quint32 version = 0;
    quint64 tensorCount = 0, kvCount = 0;
    qint64 fileSize = 0;
    QString architecture, name, fileType;
    static GgufInfo inspect(const QString &path);
};

// Runs a quantised GGUF model locally through llama.cpp's `llama-server`
// (OpenAI-compatible API on 127.0.0.1). Works offline; no API key.
// The model is never downloaded automatically.
class LocalLlamaProvider : public HttpProviderBase {
    Q_OBJECT
public:
    explicit LocalLlamaProvider(QObject *parent = nullptr);
    ~LocalLlamaProvider() override;
    QString id() const override { return QStringLiteral("local"); }
    QString displayName() const override { return QStringLiteral("Local model (llama.cpp, offline)"); }
    bool isLocal() const override { return true; }
    bool isConfigured() const override;
    void start() override;
    void stop() override;
    void complete(const AIRequest &r) override;
    static QString findServerExecutable();
    QString serverLog() const { return m_log; }
    static QString recommendedModel() { return QStringLiteral("Qwen3-1.7B-Q4_K_M.gguf"); }
    static QString recommendedModelUrl() { return QStringLiteral("https://huggingface.co/Qwen/Qwen3-1.7B-GGUF"); }
private:
    void pollHealth();
    QProcess *m_proc = nullptr;
    QTimer m_health;
    int m_port = 0;
    int m_healthTries = 0;
    QString m_log;
};

} // namespace isim
