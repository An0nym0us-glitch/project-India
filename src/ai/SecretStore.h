#pragma once
#include <QString>

namespace isim {
// Stores API keys outside plain settings. Windows: Credential Manager.
// Other platforms: a user-only (0600) file in the app config directory.
class SecretStore {
public:
    static bool write(const QString &key, const QString &secret, QString *error = nullptr);
    static QString read(const QString &key);
    static bool remove(const QString &key);
    static bool isSecureBackend(); // true on Windows Credential Manager
    static QString backendName();
    static void setFallbackDirectory(const QString &dir); // tests
};
}
