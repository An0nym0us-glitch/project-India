#include "LocalLlamaProvider.h"
#include "AppSettings.h"
#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QStandardPaths>
#include <QTcpServer>
#include <QThread>

namespace isim {

static QString readGgufString(QDataStream &ds)
{
    quint64 len = 0;
    ds >> len;
    if (len > 4096) { ds.skipRawData(int(std::min<quint64>(len, 1 << 20))); return {}; }
    QByteArray b(int(len), 0);
    ds.readRawData(b.data(), int(len));
    return QString::fromUtf8(b);
}

GgufInfo GgufInfo::inspect(const QString &path)
{
    GgufInfo g;
    QFile f(path);
    g.fileSize = QFileInfo(path).size();
    if (!f.open(QIODevice::ReadOnly)) { g.error = QStringLiteral("Cannot open file"); return g; }
    QDataStream ds(&f);
    ds.setByteOrder(QDataStream::LittleEndian);
    char magic[4];
    if (ds.readRawData(magic, 4) != 4 || memcmp(magic, "GGUF", 4) != 0) { g.error = QStringLiteral("Not a GGUF file (missing 'GGUF' header)"); return g; }
    ds >> g.version;
    if (g.version < 2 || g.version > 3) { g.error = QStringLiteral("Unsupported GGUF version %1").arg(g.version); return g; }
    ds >> g.tensorCount >> g.kvCount;
    // read the first metadata entries for architecture / name / file type
    for (quint64 i = 0; i < std::min<quint64>(g.kvCount, 64) && ds.status() == QDataStream::Ok; ++i) {
        const QString key = readGgufString(ds);
        quint32 type = 0; ds >> type;
        auto skip = [&](quint32 t) -> bool {
            static const int sizes[] = {1, 1, 2, 2, 4, 4, 4, 1, 0, 0, 8, 8, 8};
            if (t == 8) { readGgufString(ds); return true; }
            if (t == 9) { // array
                quint32 at = 0; quint64 n = 0; ds >> at >> n;
                if (at == 8) { for (quint64 k = 0; k < n && ds.status() == QDataStream::Ok; ++k) readGgufString(ds); return true; }
                if (at < 13 && sizes[at] > 0) { ds.skipRawData(int(std::min<quint64>(n * sizes[at], 1u << 30))); return true; }
                return false;
            }
            if (t < 13 && sizes[t] > 0) { ds.skipRawData(sizes[t]); return true; }
            return false;
        };
        if (type == 8 && (key == "general.architecture" || key == "general.name")) {
            const QString v = readGgufString(ds);
            if (key == "general.architecture") g.architecture = v; else g.name = v;
        } else if (type == 4 && key == "general.file_type") {
            quint32 ft = 0; ds >> ft;
            static const QHash<quint32, QString> names = {{15, "Q4_K_M"}, {14, "Q4_K_S"}, {17, "Q5_K_M"}, {18, "Q6_K"}, {7, "Q8_0"}, {1, "F16"}, {0, "F32"}, {2, "Q4_0"}};
            g.fileType = names.value(ft, QString::number(ft));
        } else if (!skip(type)) break;
    }
    g.valid = true;
    return g;
}

LocalLlamaProvider::LocalLlamaProvider(QObject *parent) : HttpProviderBase(parent)
{
    m_health.setInterval(500);
    connect(&m_health, &QTimer::timeout, this, &LocalLlamaProvider::pollHealth);
    setStatus(isConfigured() ? AIStatus::Stopped : AIStatus::NotConfigured,
              isConfigured() ? QStringLiteral("Model selected; not loaded") : QStringLiteral("No local model selected"));
}

LocalLlamaProvider::~LocalLlamaProvider() { stop(); }

bool LocalLlamaProvider::isConfigured() const
{
    const QString p = AppSettings::localModelPath();
    return !p.isEmpty() && QFileInfo::exists(p);
}

QString LocalLlamaProvider::findServerExecutable()
{
    const QString configured = AppSettings::llamaServerPath();
    if (!configured.isEmpty() && QFileInfo(configured).isExecutable()) return configured;
#ifdef Q_OS_WIN
    const QString exe = "llama-server.exe";
#else
    const QString exe = "llama-server";
#endif
    const QString appDir = QCoreApplication::applicationDirPath();
    for (const QString &c : {appDir + "/llama/" + exe, appDir + "/" + exe, appDir + "/../llama/" + exe})
        if (QFileInfo(c).isExecutable()) return QFileInfo(c).absoluteFilePath();
    return QStandardPaths::findExecutable(exe);
}

void LocalLlamaProvider::start()
{
    if (m_proc && m_proc->state() != QProcess::NotRunning) return;
    if (!isConfigured()) { setStatus(AIStatus::NotConfigured, "No local model selected"); return; }
    const auto info = GgufInfo::inspect(AppSettings::localModelPath());
    if (!info.valid) { setStatus(AIStatus::Error, "Model file invalid: " + info.error); return; }
    const QString server = findServerExecutable();
    if (server.isEmpty()) {
        setStatus(AIStatus::Error, QStringLiteral("llama-server executable not found. Place it in the 'llama' folder next to the application or set its path in Settings."));
        return;
    }
    // pick a free port
    { QTcpServer probe; probe.listen(QHostAddress::LocalHost, 0); m_port = probe.serverPort(); }
    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_proc, &QProcess::readyRead, this, [this]() { m_log += QString::fromUtf8(m_proc->readAll()); if (m_log.size() > 20000) m_log = m_log.right(20000); });
    connect(m_proc, &QProcess::finished, this, [this](int code, QProcess::ExitStatus) {
        m_health.stop();
        if (m_status != AIStatus::Stopped) setStatus(AIStatus::Error, QStringLiteral("llama-server exited (code %1). See server log.").arg(code));
    });
    QStringList args{"-m", AppSettings::localModelPath(), "--host", "127.0.0.1", "--port", QString::number(m_port),
                     "-c", QString::number(AppSettings::localContextSize()), "-ngl", QString::number(AppSettings::localGpuLayers()),
                     "--jinja"};
    const int threads = AppSettings::localThreads() > 0 ? AppSettings::localThreads() : std::max(1, QThread::idealThreadCount() - 1);
    args << "-t" << QString::number(threads);
    m_log.clear();
    m_proc->start(server, args);
    m_healthTries = 0;
    setStatus(AIStatus::Loading, QStringLiteral("Loading %1 (%2 MB)…").arg(QFileInfo(AppSettings::localModelPath()).fileName()).arg(info.fileSize / 1048576));
    m_health.start();
}

void LocalLlamaProvider::pollHealth()
{
    if (++m_healthTries > 600) { m_health.stop(); setStatus(AIStatus::Error, "Timed out waiting for llama-server to load the model."); return; }
    auto *reply = m_nam->get(QNetworkRequest(QUrl(QStringLiteral("http://127.0.0.1:%1/health").arg(m_port))));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();
        if (code == 200 && m_status == AIStatus::Loading) {
            m_health.stop();
            setStatus(AIStatus::Ready, QStringLiteral("Model loaded (%1)").arg(QFileInfo(AppSettings::localModelPath()).fileName()));
        }
    });
}

void LocalLlamaProvider::stop()
{
    m_health.stop();
    if (m_proc) {
        setStatus(AIStatus::Stopped, "Stopped");
        m_proc->terminate();
        if (!m_proc->waitForFinished(3000)) m_proc->kill();
        m_proc->deleteLater();
        m_proc = nullptr;
    }
}

void LocalLlamaProvider::complete(const AIRequest &r)
{
    if (m_status != AIStatus::Ready) {
        emit failed(r.id, m_status == AIStatus::Loading ? QStringLiteral("The local model is still loading.") : QStringLiteral("The local model is not loaded. Start it from the AI panel or Settings."));
        return;
    }
    AIRequest q = r;
    q.userPrompt = "/no_think\n" + r.userPrompt; // Qwen3: disable the thinking phase for concise answers
    post(q, QUrl(QStringLiteral("http://127.0.0.1:%1/v1/chat/completions").arg(m_port)), OpenAIProtocol::buildBody(q, "local", true), {},
         &OpenAIProtocol::parseResponse, 300000);
}

} // namespace isim
