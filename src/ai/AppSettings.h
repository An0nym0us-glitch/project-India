#pragma once
#include <QSettings>
#include <QString>

namespace isim {
// Persistent application preferences (not scenario data).
class AppSettings {
public:
    static QSettings &s();
    static bool firstRunComplete();
    static void setFirstRunComplete(bool v);
    static QString defaultProvider();          // "local" | "openai" | "gemini" | "none"
    static void setDefaultProvider(const QString &id);
    static QString localModelPath();
    static void setLocalModelPath(const QString &p);
    static QString llamaServerPath();          // empty = auto-detect
    static void setLlamaServerPath(const QString &p);
    static int localContextSize();
    static void setLocalContextSize(int n);
    static int localGpuLayers();
    static void setLocalGpuLayers(int n);
    static int localThreads();
    static void setLocalThreads(int n);
    static QString openAIModel();
    static void setOpenAIModel(const QString &m);
    static QString openAIEndpoint();
    static void setOpenAIEndpoint(const QString &u);
    static QString geminiModel();
    static void setGeminiModel(const QString &m);
    static bool cloudConsentGiven();
    static void setCloudConsentGiven(bool v);
    static QStringList favourites();
    static void setFavourites(const QStringList &l);
    static QStringList recent();
    static void pushRecent(const QString &id);
    static QString lastScenarioDir();
    static void setLastScenarioDir(const QString &d);
};
}
