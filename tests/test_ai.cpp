// Tests for the AI integration layer (no network access or model needed).
#include "ai/AIManager.h"
#include "ai/AIResponseValidator.h"
#include "ai/AppSettings.h"
#include "ai/CloudProviders.h"
#include "ai/LocalLlamaProvider.h"
#include "ai/SecretStore.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest>

using namespace isim;

// Minimal HTTP server that answers every POST with a fixed OpenAI-style body.
class MockServer : public QTcpServer {
public:
    QByteArray reply, lastRequest;
    MockServer() {
        connect(this, &QTcpServer::newConnection, this, [this]() {
            auto *s = nextPendingConnection();
            connect(s, &QTcpSocket::readyRead, s, [this, s]() {
                lastRequest += s->readAll();
                const int h = lastRequest.indexOf("\r\n\r\n");
                if (h < 0) return;
                const auto hdr = lastRequest.left(h);
                int len = 0;
                for (const auto &line : hdr.split('\n'))
                    if (line.toLower().startsWith("content-length:")) len = line.mid(15).trimmed().toInt();
                if (lastRequest.size() < h + 4 + len) return;
                s->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + QByteArray::number(reply.size()) + "\r\nConnection: close\r\n\r\n" + reply);
                s->disconnectFromHost();
            });
        });
    }
};

class TestAI : public QObject {
    Q_OBJECT
    QTemporaryDir m_dir;
private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        SecretStore::setFallbackDirectory(m_dir.path());
        AppSettings::s().clear();
    }
    void stripAndExtract()
    {
        const QString t = "<think>reasoning 123</think>\n```json\n{\"summary\": \"ok\"}\n```";
        QCOMPARE(AIResponseValidator::stripThinking("<think>x</think>hi"), QString("hi"));
        QCOMPARE(AIResponseValidator::extractJsonObject(t), QString("{\"summary\": \"ok\"}"));
    }
    void schemaValidation()
    {
        auto good = AIResponseValidator::validate(R"({"summary":"Power is short.","key_points":["a"],"risks":[],"missing_assumptions":[],"alternatives":["b"]})", "{}");
        QVERIFY(good.structured && good.acceptable());
        auto bad = AIResponseValidator::validate("The answer is that power is short.", "{}");
        QVERIFY(!bad.structured);
        auto badType = AIResponseValidator::validate(R"({"summary":"x","key_points":"not a list"})", "{}");
        QVERIFY(!badType.structured);
    }
    void numbersMustComeFromContext()
    {
        const QString ctx = R"({"electricity_served_mw": 12345, "unemployment_pct": 7.42, "gdp_crore_inr": 8473402})";
        auto ok = AIResponseValidator::validate(R"({"summary":"Only 12,345 MW is served and unemployment is 7.4%. GDP is ₹84.7 lakh crore."})", ctx);
        QVERIFY2(ok.unverifiedNumbers.isEmpty(), qPrintable(ok.unverifiedNumbers.join(",")));
        auto invented = AIResponseValidator::validate(R"({"summary":"India has 450 GW of capacity and 62% literacy."})", ctx);
        QVERIFY(invented.unverifiedNumbers.contains("450"));
        QVERIFY(invented.unverifiedNumbers.contains("62"));
    }
    void ggufInspection()
    {
        // valid minimal header: magic, version 3, 0 tensors, 1 kv (general.architecture = "qwen3")
        QByteArray b("GGUF");
        auto u32 = [&](quint32 v) { b.append(reinterpret_cast<const char *>(&v), 4); };
        auto u64 = [&](quint64 v) { b.append(reinterpret_cast<const char *>(&v), 8); };
        auto str = [&](const QByteArray &s) { u64(s.size()); b.append(s); };
        u32(3); u64(0); u64(1); str("general.architecture"); u32(8); str("qwen3");
        const QString path = m_dir.filePath("tiny.gguf");
        QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly)); f.write(b); f.close();
        const auto g = GgufInfo::inspect(path);
        QVERIFY2(g.valid, qPrintable(g.error));
        QCOMPARE(g.architecture, QString("qwen3"));
        const QString junk = m_dir.filePath("junk.gguf");
        QFile j(junk); QVERIFY(j.open(QIODevice::WriteOnly)); j.write("NOTAMODEL"); j.close();
        QVERIFY(!GgufInfo::inspect(junk).valid);
    }
    void secretStore()
    {
        QVERIFY(SecretStore::write("test_key", "sk-abc123"));
        QCOMPARE(SecretStore::read("test_key"), QString("sk-abc123"));
        QVERIFY(SecretStore::remove("test_key"));
        QVERIFY(SecretStore::read("test_key").isEmpty());
    }
    void protocols()
    {
        AIRequest r; r.systemPrompt = "S"; r.userPrompt = "U"; r.maxTokens = 50;
        const auto ob = OpenAIProtocol::buildBody(r, "m", true);
        QCOMPARE(ob.value("messages").toArray().size(), 2);
        QCOMPARE(ob.value("response_format").toObject().value("type").toString(), QString("json_object"));
        QString text, err;
        QVERIFY(OpenAIProtocol::parseResponse(R"({"choices":[{"message":{"content":"hello"}}]})", text, err));
        QCOMPARE(text, QString("hello"));
        QVERIFY(!OpenAIProtocol::parseResponse(R"({"error":{"message":"bad key"}})", text, err));
        QCOMPARE(err, QString("bad key"));
        const auto gb = GeminiProtocol::buildBody(r);
        QCOMPARE(gb.value("generationConfig").toObject().value("responseMimeType").toString(), QString("application/json"));
        text.clear();
        QVERIFY(GeminiProtocol::parseResponse(R"({"candidates":[{"content":{"parts":[{"text":"hi"}]}}]})", text, err));
        QCOMPARE(text, QString("hi"));
    }
    void endToEndWithMockOpenAI()
    {
        MockServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        server.reply = QJsonDocument(QJsonObject{{"choices", QJsonArray{QJsonObject{{"message", QJsonObject{{"content",
            R"({"summary":"Only 1,000 MW of 5,000 MW demand is served.","key_points":["Build generation"],"risks":[],"missing_assumptions":["Fuel prices"],"alternatives":[]})"}}}}}}}).toJson();
        AppSettings::setOpenAIEndpoint(QStringLiteral("http://127.0.0.1:%1/v1").arg(server.serverPort()));
        QVERIFY(SecretStore::write(OpenAIProvider::secretKey(), "sk-test"));
        AIManager mgr;
        mgr.setActive("openai");
        QSignalSpy spy(&mgr, &AIManager::answered);
        const QString id = mgr.ask("interpret_report", QJsonObject{{"electricity_served_mw", 1000}, {"electricity_demand_mw", 5000}});
        QVERIFY(!id.isEmpty());
        QVERIFY(spy.wait(10000));
        const auto a = spy.first().first().value<AIAnswer>();
        QVERIFY2(a.error.isEmpty(), qPrintable(a.error));
        QVERIFY(a.response.structured);
        QVERIFY(a.response.unverifiedNumbers.isEmpty());
        QVERIFY(server.lastRequest.contains("Bearer sk-test"));
        QVERIFY(server.lastRequest.contains("electricity_served_mw")); // context was sent
        SecretStore::remove(OpenAIProvider::secretKey());
    }
    void localProviderWithoutModelFailsGracefully()
    {
        AppSettings::setLocalModelPath(QString());
        LocalLlamaProvider p;
        QVERIFY(!p.isConfigured());
        QCOMPARE(p.status(), AIStatus::NotConfigured);
        QSignalSpy spy(&p, &AIProvider::failed);
        AIRequest r; r.id = "x";
        p.complete(r);
        QCOMPARE(spy.count(), 1); // simulation keeps working; the request just fails
    }
};

Q_DECLARE_METATYPE(isim::AIAnswer)
QTEST_GUILESS_MAIN(TestAI)
#include "test_ai.moc"
