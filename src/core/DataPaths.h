#pragma once
#include <QString>

namespace isim {
// Locates the bundled data directory (next to the executable when deployed,
// or the source tree during development).
QString dataDir();
QString dataFile(const QString &relative);
void setDataDirOverride(const QString &dir);
}
