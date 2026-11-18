#include "SecretStore.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincred.h>
#endif

namespace isim {

static QString g_dir;
void SecretStore::setFallbackDirectory(const QString &dir) { g_dir = dir; }

static QString target(const QString &key) { return QStringLiteral("IndiaDevelopmentSimulator/") + key; }

#ifdef Q_OS_WIN
bool SecretStore::write(const QString &key, const QString &secret, QString *error)
{
    const std::wstring t = target(key).toStdWString();
    const QByteArray blob = secret.toUtf8();
    CREDENTIALW c{};
    c.Type = CRED_TYPE_GENERIC;
    c.TargetName = const_cast<LPWSTR>(t.c_str());
    c.CredentialBlobSize = DWORD(blob.size());
    c.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char *>(blob.constData()));
    c.Persist = CRED_PERSIST_LOCAL_MACHINE;
    if (!CredWriteW(&c, 0)) { if (error) *error = QStringLiteral("CredWrite failed (%1)").arg(GetLastError()); return false; }
    return true;
}
QString SecretStore::read(const QString &key)
{
    const std::wstring t = target(key).toStdWString();
    PCREDENTIALW c = nullptr;
    if (!CredReadW(t.c_str(), CRED_TYPE_GENERIC, 0, &c)) return {};
    const QString s = QString::fromUtf8(reinterpret_cast<const char *>(c->CredentialBlob), int(c->CredentialBlobSize));
    CredFree(c);
    return s;
}
bool SecretStore::remove(const QString &key)
{
    const std::wstring t = target(key).toStdWString();
    return CredDeleteW(t.c_str(), CRED_TYPE_GENERIC, 0);
}
bool SecretStore::isSecureBackend() { return true; }
QString SecretStore::backendName() { return QStringLiteral("Windows Credential Manager"); }
#else
static QString filePath()
{
    QString dir = g_dir.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) : g_dir;
    QDir().mkpath(dir);
    return dir + "/secrets.json";
}
static QJsonObject readAll()
{
    QFile f(filePath());
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}
static bool writeAll(const QJsonObject &o, QString *error)
{
    QFile f(filePath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) { if (error) *error = f.errorString(); return false; }
    f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    f.write(QJsonDocument(o).toJson(QJsonDocument::Compact));
    return true;
}
bool SecretStore::write(const QString &key, const QString &secret, QString *error)
{
    auto o = readAll();
    o.insert(target(key), QString::fromLatin1(secret.toUtf8().toBase64()));
    return writeAll(o, error);
}
QString SecretStore::read(const QString &key)
{
    return QString::fromUtf8(QByteArray::fromBase64(readAll().value(target(key)).toString().toLatin1()));
}
bool SecretStore::remove(const QString &key)
{
    auto o = readAll();
    o.remove(target(key));
    return writeAll(o, nullptr);
}
bool SecretStore::isSecureBackend() { return false; }
QString SecretStore::backendName() { return QStringLiteral("user-only file (not encrypted) in the application config folder"); }
#endif

} // namespace isim
