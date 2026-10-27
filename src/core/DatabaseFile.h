#pragma once

#include <QString>
#include <QList>
#include "CryptoHelper.h"
#include "PasswordItem.h"
#include "SecretString.h"

namespace EasePass::Core {

// SourceUnavailable / SourceLocked are only produced by DatabaseService (IDatabaseSource availability).
enum class LoadResult {
    Success,
    WrongPassword,
    DatabaseNotFound,
    WrongFormat,
    NeedsSecondFactor,
    Error,
    SourceUnavailable,
    SourceLocked
};

class DatabaseFile {
public:
    DatabaseFile() = default;
    ~DatabaseFile();

    QString filePath;
    SecretString masterPassword;
    SecretString secondFactor;
    int databaseFileType = 0; // epdb
    double version = 1.4;
    bool useSecondFactor = false;
    int secondFactorType = 0;
    bool isReadOnly = false;

    QList<PasswordItem> items;

    static LoadResult loadFromFile(const QString &path, const SecretString &masterPassword, DatabaseFile &outDb,
                                   const SecretString &secondFactor = SecretString());

    /**
     * Decrypts an epdb v1.4 blob (version tag + encrypted outer JSON) as returned by an
     * IDatabaseSource. Does not touch filePath/isReadOnly. Thread-safe (no shared state), so it
     * can run on a worker thread. kdf: see KdfParams (tests only may pass non-production values).
     */
    static LoadResult loadFromBytes(const QByteArray &fileBytes, const SecretString &masterPassword,
                                    DatabaseFile &outDb, const SecretString &secondFactor = SecretString(),
                                    const KdfParams &kdf = KdfParams::production());

    /**
     * Encrypts the database into the epdb v1.4 on-disk representation (the exact bytes that
     * saveToFile() writes). Returns an empty array on failure. Ignores isReadOnly.
     * C#: Core/Database/Format/epdb/MainDatabaseLoader.cs (Save)
     */
    QByteArray serialize(const KdfParams &kdf = KdfParams::production()) const;

    bool saveToFile(const QString &path = QString());

    void wipeSecrets();

    void addItem(const PasswordItem &item);
    void updateItem(int index, const PasswordItem &item);
    void deleteItem(int index);

    int countOccurrences(const QString &password) const;
    void clearOldClicksCache(const IClock *clock = nullptr);
    void resetPopularity();
};

} // namespace EasePass::Core
