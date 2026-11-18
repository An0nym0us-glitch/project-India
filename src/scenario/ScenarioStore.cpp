#include "ScenarioStore.h"
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>

namespace isim {

namespace {
struct DbHandle {
    QString name;
    QSqlDatabase db;
    explicit DbHandle(const QString &path)
    {
        name = "isim_" + QUuid::createUuid().toString(QUuid::Id128);
        db = QSqlDatabase::addDatabase("QSQLITE", name);
        db.setDatabaseName(path);
    }
    ~DbHandle()
    {
        db.close();
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(name);
    }
};
bool exec(QSqlQuery &q, const QString &sql, QString *error)
{
    if (q.exec(sql)) return true;
    if (error) *error = q.lastError().text() + " [" + sql.left(60) + "]";
    return false;
}
} // namespace

bool ScenarioStore::save(const WorldState &w, const QString &path, QString *error)
{
    const QString tmp = path + ".tmp";
    QFile::remove(tmp);
    {
        DbHandle h(tmp);
        if (!h.db.open()) { if (error) *error = h.db.lastError().text(); return false; }
        QSqlQuery q(h.db);
        const QStringList ddl = {
            "CREATE TABLE meta(key TEXT PRIMARY KEY, value TEXT)",
            "CREATE TABLE facilities(uid INTEGER PRIMARY KEY, def_id TEXT, lon REAL, lat REAL, district INTEGER, status INTEGER, progress REAL, capex_cr REAL, length_km REAL, json TEXT)",
            "CREATE TABLE policies(id TEXT PRIMARY KEY, intensity REAL, start_month INTEGER)",
            "CREATE TABLE history(month INTEGER, key TEXT, value REAL)",
            "CREATE TABLE world(id INTEGER PRIMARY KEY, json BLOB)"};
        for (const auto &s : ddl) if (!exec(q, s, error)) return false;
        h.db.transaction();
        auto meta = [&](const QString &k, const QString &v) {
            q.prepare("INSERT INTO meta(key, value) VALUES(?, ?)");
            q.addBindValue(k); q.addBindValue(v);
            q.exec();
        };
        meta("schema_version", QString::number(kSchemaVersion));
        meta("app", "India National Development & Policy Simulator");
#ifdef ISIM_VERSION
        meta("app_version", ISIM_VERSION);
#endif
        meta("name", w.name);
        meta("description", w.description);
        meta("date", w.dateString());
        meta("saved_at", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        meta("facility_count", QString::number(w.facilities.size()));
        for (const auto &f : w.facilities) {
            q.prepare("INSERT INTO facilities VALUES(?,?,?,?,?,?,?,?,?,?)");
            q.addBindValue(f.uid); q.addBindValue(f.defId); q.addBindValue(f.location.x()); q.addBindValue(f.location.y());
            q.addBindValue(f.district); q.addBindValue(int(f.status)); q.addBindValue(f.progress); q.addBindValue(f.capexCr);
            q.addBindValue(f.lengthKm); q.addBindValue(QString::fromUtf8(QJsonDocument(f.toJson()).toJson(QJsonDocument::Compact)));
            if (!q.exec()) { if (error) *error = q.lastError().text(); return false; }
        }
        for (const auto &p : w.policies) {
            q.prepare("INSERT INTO policies VALUES(?,?,?)");
            q.addBindValue(p.id); q.addBindValue(p.intensity); q.addBindValue(p.startMonth);
            q.exec();
        }
        q.prepare("INSERT INTO history VALUES(?,?,?)");
        QVariantList hm, hk, hv;
        for (const auto &h2 : w.history)
            for (auto it = h2.v.begin(); it != h2.v.end(); ++it) { hm << h2.month; hk << it.key(); hv << it.value(); }
        q.addBindValue(hm); q.addBindValue(hk); q.addBindValue(hv);
        if (!q.execBatch()) { if (error) *error = q.lastError().text(); return false; }
        q.prepare("INSERT INTO world(id, json) VALUES(1, ?)");
        q.addBindValue(QJsonDocument(w.toJson()).toJson(QJsonDocument::Compact));
        if (!q.exec()) { if (error) *error = q.lastError().text(); return false; }
        if (!h.db.commit()) { if (error) *error = h.db.lastError().text(); return false; }
    }
    QFile::remove(path);
    if (!QFile::rename(tmp, path)) { if (error) *error = "Could not write " + path; return false; }
    return true;
}

bool ScenarioStore::readMeta(const QString &path, QHash<QString, QString> &meta, QString *error)
{
    if (!QFile::exists(path)) { if (error) *error = "File not found: " + path; return false; }
    DbHandle h(path);
    if (!h.db.open()) { if (error) *error = h.db.lastError().text(); return false; }
    QSqlQuery q(h.db);
    if (!q.exec("SELECT key, value FROM meta")) { if (error) *error = "Not a scenario file (" + q.lastError().text() + ")"; return false; }
    while (q.next()) meta.insert(q.value(0).toString(), q.value(1).toString());
    return true;
}

bool ScenarioStore::load(const QString &path, WorldState &out, QString *error)
{
    QHash<QString, QString> meta;
    if (!readMeta(path, meta, error)) return false;
    const int ver = meta.value("schema_version").toInt();
    if (ver < 1 || ver > kSchemaVersion) {
        if (error) *error = QStringLiteral("Unsupported scenario schema version %1 (this build supports up to %2).").arg(ver).arg(kSchemaVersion);
        return false;
    }
    DbHandle h(path);
    if (!h.db.open()) { if (error) *error = h.db.lastError().text(); return false; }
    QSqlQuery q(h.db);
    if (!q.exec("SELECT json FROM world WHERE id = 1") || !q.next()) { if (error) *error = "Scenario file has no world state."; return false; }
    QJsonParseError pe;
    const auto doc = QJsonDocument::fromJson(q.value(0).toByteArray(), &pe);
    if (pe.error != QJsonParseError::NoError) { if (error) *error = "Corrupt world state: " + pe.errorString(); return false; }
    QString e;
    out = WorldState::fromJson(doc.object(), &e);
    if (!e.isEmpty()) { if (error) *error = e; return false; }
    return true;
}

} // namespace isim
