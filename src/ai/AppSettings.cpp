#include "AppSettings.h"
#include <QStringList>

namespace isim {

QSettings &AppSettings::s()
{
    static QSettings settings(QSettings::IniFormat, QSettings::UserScope, "IndiaSim", "IndiaDevelopmentSimulator");
    return settings;
}
bool AppSettings::firstRunComplete() { return s().value("setup/complete", false).toBool(); }
void AppSettings::setFirstRunComplete(bool v) { s().setValue("setup/complete", v); }
QString AppSettings::defaultProvider() { return s().value("ai/default", "none").toString(); }
void AppSettings::setDefaultProvider(const QString &id) { s().setValue("ai/default", id); }
QString AppSettings::localModelPath() { return s().value("ai/local/model").toString(); }
void AppSettings::setLocalModelPath(const QString &p) { s().setValue("ai/local/model", p); }
QString AppSettings::llamaServerPath() { return s().value("ai/local/server").toString(); }
void AppSettings::setLlamaServerPath(const QString &p) { s().setValue("ai/local/server", p); }
int AppSettings::localContextSize() { return s().value("ai/local/ctx", 8192).toInt(); }
void AppSettings::setLocalContextSize(int n) { s().setValue("ai/local/ctx", n); }
int AppSettings::localGpuLayers() { return s().value("ai/local/gpuLayers", 99).toInt(); }
void AppSettings::setLocalGpuLayers(int n) { s().setValue("ai/local/gpuLayers", n); }
int AppSettings::localThreads() { return s().value("ai/local/threads", 0).toInt(); }
void AppSettings::setLocalThreads(int n) { s().setValue("ai/local/threads", n); }
QString AppSettings::openAIModel() { return s().value("ai/openai/model", "gpt-4o-mini").toString(); }
void AppSettings::setOpenAIModel(const QString &m) { s().setValue("ai/openai/model", m); }
QString AppSettings::openAIEndpoint() { return s().value("ai/openai/endpoint", "https://api.openai.com/v1").toString(); }
void AppSettings::setOpenAIEndpoint(const QString &u) { s().setValue("ai/openai/endpoint", u); }
QString AppSettings::geminiModel() { return s().value("ai/gemini/model", "gemini-2.5-flash").toString(); }
void AppSettings::setGeminiModel(const QString &m) { s().setValue("ai/gemini/model", m); }
bool AppSettings::cloudConsentGiven() { return s().value("ai/cloudConsent", false).toBool(); }
void AppSettings::setCloudConsentGiven(bool v) { s().setValue("ai/cloudConsent", v); }
QStringList AppSettings::favourites() { return s().value("catalog/favourites").toStringList(); }
void AppSettings::setFavourites(const QStringList &l) { s().setValue("catalog/favourites", l); }
QStringList AppSettings::recent() { return s().value("catalog/recent").toStringList(); }
void AppSettings::pushRecent(const QString &id)
{
    auto l = recent();
    l.removeAll(id);
    l.prepend(id);
    while (l.size() > 12) l.removeLast();
    s().setValue("catalog/recent", l);
}
QString AppSettings::lastScenarioDir() { return s().value("scenario/dir").toString(); }
void AppSettings::setLastScenarioDir(const QString &d) { s().setValue("scenario/dir", d); }

} // namespace isim
