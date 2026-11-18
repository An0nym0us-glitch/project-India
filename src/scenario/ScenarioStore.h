#pragma once
#include "sim/WorldState.h"
#include <QString>

namespace isim {

// Versioned scenario files (.indiasim = SQLite database).
//   meta(key, value)            : schema_version, app_version, name, saved_at, ...
//   facilities(uid, def_id, ...) : placed facilities (queryable)
//   policies(id, intensity, start_month)
//   history(month, key, value)  : monthly national indicators
//   world(id, json)              : complete world state (authoritative)
class ScenarioStore {
public:
    static constexpr int kSchemaVersion = 1;
    static bool save(const WorldState &w, const QString &path, QString *error = nullptr);
    static bool load(const QString &path, WorldState &out, QString *error = nullptr);
    static bool readMeta(const QString &path, QHash<QString, QString> &meta, QString *error = nullptr);
};

} // namespace isim
