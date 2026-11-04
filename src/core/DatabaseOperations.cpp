#include "DatabaseOperations.h"

#include "DatabaseFile.h"
#include "LocalFileDatabaseSource.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QMetaObject>

namespace EasePass::Core {

Q_LOGGING_CATEGORY(lcDatabaseOperations, "easepass.core.databaseoperations")

namespace {

/// Reads and decrypts a database file (worker thread).
LoadResult decryptFile(const QString &path, const SecretString &password, const SecretString &secondFactor,
                       const KdfParams &kdf, DatabaseFile &out) {
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return LoadResult::DatabaseNotFound;
    }
    const QByteArray bytes = file.readAll();
    file.close();
    return DatabaseFile::loadFromBytes(bytes, password, out, secondFactor, kdf);
}

Qt::CaseSensitivity pathCaseSensitivity() {
#ifdef Q_OS_WIN
    return Qt::CaseInsensitive;
#else
    return Qt::CaseSensitive;
#endif
}

QString normalizedPath(const QString &path) {
    QFileInfo info(path);
    const QString canonical = info.canonicalFilePath(); // empty if the file does not exist
    return canonical.isEmpty() ? QDir::cleanPath(info.absoluteFilePath()) : canonical;
}

} // namespace

DatabaseOperations::DatabaseOperations(std::shared_ptr<AppSettings> settings, DatabaseService *service,
                                       QObject *parent)
    : DatabaseOperations(std::move(settings), service, KdfParams::production(), parent) {}

DatabaseOperations::DatabaseOperations(std::shared_ptr<AppSettings> settings, DatabaseService *service,
                                       const KdfParams &kdf, QObject *parent)
    : QObject(parent), m_settings(std::move(settings)), m_service(service), m_kdf(kdf) {
    m_pool.setMaxThreadCount(1);
}

DatabaseOperations::~DatabaseOperations() {
    // Queued callbacks addressed to this object are discarded by Qt once it is destroyed.
    m_pool.waitForDone();
}

void DatabaseOperations::waitForBackgroundWork() {
    while (m_pendingJobs > 0) {
        m_pool.waitForDone();
        QCoreApplication::sendPostedEvents(this, QEvent::MetaCall);
    }
}

void DatabaseOperations::post(const std::function<void()> &fn) {
    QMetaObject::invokeMethod(this, fn, Qt::QueuedConnection);
}

// --- Pure helpers -------------------------------------------------------------------------

QString DatabaseOperations::databaseExtension() {
    return QStringLiteral(".epdb");
}

QString DatabaseOperations::sanitizeDatabaseName(const QString &name) {
    // Union of the characters Windows forbids and the path separators of every OS
    // (C#: Path.GetInvalidFileNameChars on Windows: < > : " / \ | ? * and control characters).
    static const QString invalid = QStringLiteral("<>:\"/\\|?*");
    QString result;
    result.reserve(name.size());
    for (const QChar c : name) {
        if (c.unicode() < 0x20 || c.unicode() == 0x7F || invalid.contains(c)) {
            continue;
        }
        result.append(c);
    }
    result = result.trimmed();
    // "." and ".." are not usable file names
    if (result == QLatin1String(".") || result == QLatin1String("..")) {
        return QString();
    }
    return result;
}

QString DatabaseOperations::buildDatabasePath(const QString &folder, const QString &name) {
    if (folder.trimmed().isEmpty()) {
        return QString();
    }
    QString fileName = sanitizeDatabaseName(name);
    if (fileName.isEmpty()) {
        fileName = QStringLiteral("Database"); // C#: CreateDatabaseName()
    }
    return QDir(folder).filePath(fileName + databaseExtension());
}

QString DatabaseOperations::defaultBackupFileName(const QString &databaseName, const QDateTime &time) {
    QString name = sanitizeDatabaseName(databaseName);
    if (name.isEmpty()) {
        name = QStringLiteral("Database");
    }
    return name + QStringLiteral("_backup_") + time.toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss")) +
           databaseExtension();
}

DatabaseOpStatus DatabaseOperations::validateNewPassword(const SecretString &password, const SecretString &repeat) {
    if (password.codePointCount() < MinMasterPasswordLength) {
        return DatabaseOpStatus::PasswordTooShort;
    }
    if (password != repeat) {
        return DatabaseOpStatus::PasswordsDoNotMatch;
    }
    return DatabaseOpStatus::Success;
}

QString DatabaseOperations::localPath(const IDatabaseSource *source) {
    if (const auto *local = qobject_cast<const LocalFileDatabaseSource *>(source)) {
        return local->path();
    }
    return QString();
}

bool DatabaseOperations::isSamePath(const QString &a, const QString &b) {
    if (a.isEmpty() || b.isEmpty()) {
        return false;
    }
    return normalizedPath(a).compare(normalizedPath(b), pathCaseSensitivity()) == 0;
}

DatabaseOpStatus DatabaseOperations::validateCreate(const CreateRequest &request) {
    // C#: CreateDatabaseDialog.Dialog_Closing order: passwords match, path valid, length, exists.
    if (request.password != request.passwordRepeat) {
        return DatabaseOpStatus::PasswordsDoNotMatch;
    }
    const QFileInfo info(request.path);
    if (request.path.trimmed().isEmpty() || info.fileName().isEmpty() ||
        sanitizeDatabaseName(info.fileName()) != info.fileName()) {
        return DatabaseOpStatus::InvalidPath;
    }
    if (request.password.codePointCount() < MinMasterPasswordLength) {
        return DatabaseOpStatus::PasswordTooShort;
    }
    if (info.exists()) {
        return DatabaseOpStatus::PathAlreadyExists;
    }
    if (!SecondFactor::isSupported(request.secondFactorType)) {
        return DatabaseOpStatus::UnsupportedSecondFactorType;
    }
    if (SecondFactor::requiresToken(request.secondFactorType) && request.secondFactor.isEmpty()) {
        return DatabaseOpStatus::InvalidSecondFactor;
    }
    return DatabaseOpStatus::Success;
}

// --- Known list helpers -------------------------------------------------------------------

bool DatabaseOperations::isOpenDatabase(const QString &path) const {
    if (!m_service || !m_service->isOpen()) {
        return false;
    }
    return isSamePath(localPath(m_service->source()), path);
}

QStringList DatabaseOperations::knownExistingPaths() const {
    QStringList result;
    if (!m_settings) {
        return result;
    }
    // C#: Database.GetAllDatabasePaths only returns paths whose file exists.
    for (const QString &p : m_settings->databasePaths()) {
        if (QFile::exists(p)) {
            result << p;
        }
    }
    return result;
}

void DatabaseOperations::replaceKnownPath(const QString &oldPath, const QString &newPath) {
    if (!m_settings) {
        return;
    }
    QStringList paths = m_settings->databasePaths();
    bool changed = false;
    for (int i = paths.size() - 1; i >= 0; --i) {
        if (paths.at(i) == oldPath || isSamePath(paths.at(i), oldPath)) {
            if (newPath.isEmpty()) {
                paths.removeAt(i);
            } else {
                paths[i] = newPath; // keep the position in the list
            }
            changed = true;
        }
    }
    if (changed) {
        m_settings->setDatabasePaths(paths);
    }
    const QString last = m_settings->lastDatabase();
    if (!last.isEmpty() && (last == oldPath || isSamePath(last, oldPath))) {
        m_settings->setLastDatabase(newPath);
    }
}

// --- File operations ----------------------------------------------------------------------

void DatabaseOperations::createDatabase(const CreateRequest &request, DatabaseOpCallback callback) {
    const DatabaseOpStatus status = validateCreate(request);
    if (status != DatabaseOpStatus::Success) {
        post([callback, status, path = request.path]() {
            if (callback) {
                callback(DatabaseOpResult(status, path));
            }
        });
        return;
    }

    // C#: Database.CreateNewDatabase (UseSecondFactor = false, SecondFactorType = None); the
    // optional second factor is applied directly instead of a later ManageSecondFactor step.
    auto db = std::make_shared<DatabaseFile>();
    db->masterPassword = request.password;
    db->useSecondFactor = request.secondFactorType != SecondFactorType::None;
    db->secondFactorType = static_cast<int>(request.secondFactorType);
    if (db->useSecondFactor) {
        db->secondFactor = request.secondFactor;
    }
    const QString path = QFileInfo(request.path).absoluteFilePath();
    const bool addToKnown = request.addToKnownDatabases;

    ++m_pendingJobs;
    m_pool.start([this, db = std::move(db), path, addToKnown, callback, kdf = m_kdf]() mutable {
        DatabaseOpStatus result = DatabaseOpStatus::Success;
        if (QFileInfo::exists(path)) {
            result = DatabaseOpStatus::PathAlreadyExists; // created meanwhile: never overwrite
        } else {
            const QByteArray bytes = db->serialize(kdf);
            db.reset();
            LocalFileDatabaseSource target(path);
            if (bytes.isEmpty() || !target.saveBytes(bytes)) {
                result = DatabaseOpStatus::IoError;
            }
        }
        QMetaObject::invokeMethod(
            this,
            [this, result, path, addToKnown, callback]() {
                --m_pendingJobs;
                if (result == DatabaseOpStatus::Success && addToKnown && m_settings) {
                    m_settings->addDatabasePath(path);
                }
                if (result == DatabaseOpStatus::IoError) {
                    qCWarning(lcDatabaseOperations) << "Creating database failed:" << path;
                }
                if (callback) {
                    callback(DatabaseOpResult(result, path));
                }
            },
            Qt::QueuedConnection);
    });
}

DatabaseOpResult DatabaseOperations::renameDatabase(const QString &path, const QString &newName) {
    const QString name = sanitizeDatabaseName(newName);
    if (path.isEmpty() || name.isEmpty()) {
        return {DatabaseOpStatus::InvalidPath, path};
    }
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        return {DatabaseOpStatus::NotFound, path};
    }
    if (isOpenDatabase(path)) {
        return {DatabaseOpStatus::IsLoadedDatabase, path};
    }

    const QString extension = info.suffix().isEmpty() ? databaseExtension() : QStringLiteral(".") + info.suffix();
    const QString newPath = info.absoluteDir().filePath(name + extension);
    if (newPath == info.absoluteFilePath()) {
        return {DatabaseOpStatus::Success, newPath};
    }
    // A case-only rename on a case-insensitive file system resolves to the same file.
    if (QFileInfo::exists(newPath) && !isSamePath(newPath, path)) {
        return {DatabaseOpStatus::PathAlreadyExists, newPath};
    }
    if (!QFile::rename(path, newPath)) {
        qCWarning(lcDatabaseOperations) << "Renaming database failed:" << path << "->" << newPath;
        return {DatabaseOpStatus::IoError, path};
    }

    const QString oldBackup = LocalFileDatabaseSource::backupPathFor(path);
    const QString newBackup = LocalFileDatabaseSource::backupPathFor(newPath);
    if (QFile::exists(oldBackup) && !QFile::exists(newBackup) && !QFile::rename(oldBackup, newBackup)) {
        qCWarning(lcDatabaseOperations) << "Could not rename backup" << oldBackup;
    }

    replaceKnownPath(path, newPath);
    return {DatabaseOpStatus::Success, newPath};
}

DatabaseOpResult DatabaseOperations::deleteDatabase(const QString &path) {
    if (path.isEmpty()) {
        return {DatabaseOpStatus::InvalidPath, path};
    }
    const bool exists = QFileInfo::exists(path);

    // C#: ManageDatabasePage.DeleteDatabase checks "only database" before "loaded database".
    if (exists) {
        const QStringList known = knownExistingPaths();
        if (known.size() <= 1 && (known.isEmpty() || isSamePath(known.first(), path))) {
            return {DatabaseOpStatus::LastDatabase, path};
        }
    }
    if (isOpenDatabase(path)) {
        return {DatabaseOpStatus::IsLoadedDatabase, path};
    }

    if (exists && !QFile::remove(path)) {
        qCWarning(lcDatabaseOperations) << "Deleting database failed:" << path;
        return {DatabaseOpStatus::IoError, path};
    }
    // The backup holds the same (encrypted) data: remove it, too.
    const QString backup = LocalFileDatabaseSource::backupPathFor(path);
    if (QFile::exists(backup) && !QFile::remove(backup)) {
        qCWarning(lcDatabaseOperations) << "Could not delete backup" << backup;
    }

    replaceKnownPath(path, QString());
    return {DatabaseOpStatus::Success, path};
}

void DatabaseOperations::importDatabaseFile(const QString &path, const SecretString &password,
                                            const SecretString &secondFactor, DatabaseOpCallback callback) {
    if (path.isEmpty()) {
        post([callback]() {
            if (callback) {
                callback(DatabaseOpResult(DatabaseOpStatus::InvalidPath));
            }
        });
        return;
    }
    const QString absolute = QFileInfo(path).absoluteFilePath();
    ++m_pendingJobs;
    m_pool.start([this, absolute, password = SecretString(password), secondFactor = SecretString(secondFactor), callback, kdf = m_kdf]() mutable {
        LoadResult result;
        {
            DatabaseFile db;
            result = decryptFile(absolute, password, secondFactor, kdf, db);
            db.items.clear();
        }
        password.clear();
        secondFactor.clear();
        QMetaObject::invokeMethod(
            this,
            [this, result, absolute, callback]() {
                --m_pendingJobs;
                // C#: ManageDatabaseHelper.ImportDatabase adds the path only after a successful unlock.
                if (result == LoadResult::Success && m_settings) {
                    m_settings->addDatabasePath(absolute);
                }
                if (callback) {
                    callback(DatabaseOpResult(DatabaseOpResult::fromLoadResult(result), absolute));
                }
            },
            Qt::QueuedConnection);
    });
}

void DatabaseOperations::readItemsFromFile(const QString &path, const SecretString &password,
                                           const SecretString &secondFactor, ItemsCallback callback) {
    if (path.isEmpty()) {
        post([callback]() {
            if (callback) {
                callback(DatabaseOpResult(DatabaseOpStatus::InvalidPath), {});
            }
        });
        return;
    }
    ++m_pendingJobs;
    m_pool.start([this, path, password = SecretString(password), secondFactor = SecretString(secondFactor), callback, kdf = m_kdf]() mutable {
        QList<PasswordItem> items;
        LoadResult result;
        {
            DatabaseFile db;
            result = decryptFile(path, password, secondFactor, kdf, db);
            if (result == LoadResult::Success) {
                items = std::move(db.items);
            }
        }
        password.clear();
        secondFactor.clear();
        QMetaObject::invokeMethod(
            this,
            [this, result, path, items = std::move(items), callback]() {
                --m_pendingJobs;
                if (callback) {
                    callback(DatabaseOpResult(DatabaseOpResult::fromLoadResult(result), path), items);
                }
            },
            Qt::QueuedConnection);
    });
}

void DatabaseOperations::importPasswordsFromFile(const QString &path, const SecretString &password,
                                                 const SecretString &secondFactor, DatabaseService::ImportMode mode,
                                                 DatabaseOpCallback callback) {
    readItemsFromFile(path, password, secondFactor,
                      [this, mode, callback](const DatabaseOpResult &read, const QList<PasswordItem> &items) {
                          DatabaseOpResult result = read;
                          if (read.ok()) {
                              if (!m_service || !m_service->isOpen()) {
                                  result.status = DatabaseOpStatus::NoDatabaseOpen;
                              } else if (!m_service->importItems(items, mode)) {
                                  result.status = DatabaseOpStatus::ReadOnly;
                              }
                          }
                          if (callback) {
                              callback(result);
                          }
                      });
}

void DatabaseOperations::exportDatabase(const QString &targetPath, const DatabaseService::ExportOptions &options,
                                        DatabaseOpCallback callback) {
    DatabaseOpStatus status = DatabaseOpStatus::Success;
    if (!m_service || !m_service->isOpen()) {
        status = DatabaseOpStatus::NoDatabaseOpen;
    } else if (targetPath.trimmed().isEmpty()) {
        status = DatabaseOpStatus::InvalidPath;
    } else if (isOpenDatabase(targetPath)) {
        status = DatabaseOpStatus::SameAsOpenDatabase;
    }
    if (status != DatabaseOpStatus::Success) {
        post([callback, status, targetPath]() {
            if (callback) {
                callback(DatabaseOpResult(status, targetPath));
            }
        });
        return;
    }
    auto target = std::make_shared<LocalFileDatabaseSource>(QFileInfo(targetPath).absoluteFilePath());
    m_service->exportDatabase(std::move(target), options, std::move(callback));
}

void DatabaseOperations::backupDatabase(const QString &targetPath, DatabaseOpCallback callback) {
    exportDatabase(targetPath, DatabaseService::ExportOptions{}, std::move(callback));
}

bool DatabaseOperations::openTemporaryDatabase(const QString &path, const SecretString &password,
                                               const SecretString &secondFactor) {
    if (!m_service || path.isEmpty()) {
        return false;
    }
    auto source = std::make_shared<LocalFileDatabaseSource>(path, /*readOnly=*/true);
    return m_service->open(std::move(source), password, secondFactor, /*temporary=*/true);
}

} // namespace EasePass::Core
