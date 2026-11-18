#include "DataPaths.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

namespace isim {
static QString g_override;

void setDataDirOverride(const QString &dir) { g_override = dir; }

QString dataDir()
{
    if (!g_override.isEmpty()) return g_override;
    const QString envDir = qEnvironmentVariable("ISIM_DATA_DIR");
    if (!envDir.isEmpty() && QFileInfo::exists(envDir + "/model_parameters.json")) return envDir;
    QStringList candidates;
    if (QCoreApplication::instance())
        candidates << QCoreApplication::applicationDirPath() + "/data"
                   << QCoreApplication::applicationDirPath() + "/../data"
                   << QCoreApplication::applicationDirPath() + "/../share/india-sim/data";
#ifdef ISIM_SOURCE_DATA_DIR
    candidates << QStringLiteral(ISIM_SOURCE_DATA_DIR);
#endif
    for (const auto &c : candidates)
        if (QFileInfo::exists(c + "/model_parameters.json")) return QDir(c).absolutePath();
    return candidates.isEmpty() ? QStringLiteral("data") : candidates.first();
}

QString dataFile(const QString &relative) { return dataDir() + "/" + relative; }
}
