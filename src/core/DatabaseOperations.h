#pragma once

#include "AppSettings.h"
#include "CryptoHelper.h"
#include "DatabaseOperationResult.h"
#include "DatabaseService.h"
#include "PasswordItem.h"
#include "SecondFactor.h"
#include "SecretString.h"

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QThreadPool>
#include <functional>
#include <memory>

namespace EasePass::Core {

class IDatabaseSource;

/**
 * @brief Database management operations on local database files (T6).
 *
 * C#: Views/ManageDatabasePage.xaml.cs, Helper/Database/ManageDatabaseHelper.cs,
 * Helper/Database/TemporaryDatabaseHelper.cs, Helper/Security/ExportPasswordsHelper.cs,
 * Core/Database/Database.cs (CreateNewDatabase, GetItemsFromFile, Add/RemoveDatabasePath),
 * Dialogs/CreateDatabaseDialog.cs + Views/DialogPages/CreateDatabaseDialogPage.xaml.cs.
 *
 * Operations on the OPEN database (change password, second factor, import items, export,
 * backup) live in DatabaseService; this class adds the file-level ones and convenience wrappers
 * that create LocalFileDatabaseSource objects for paths. The known-databases list is kept in
 * AppSettings::databasePaths.
 *
 * Threading: lives on the UI thread. Operations that need Argon2 (create, import database file,
 * read items) run on ONE private worker thread; their callbacks run asynchronously on the UI
 * thread, also for validation errors, and are dropped if this object is destroyed (the
 * destructor waits for running jobs). Rename/delete are fast and synchronous.
 *
 * Copying a database path to the clipboard (C#: CopyDatabasePath_Click) is UI work; use
 * localPath() to get the path of a source.
 */
class DatabaseOperations : public QObject {
    Q_OBJECT
public:
    using ItemsCallback = std::function<void(const DatabaseOpResult &, const QList<PasswordItem> &)>;

    struct CreateRequest {
        /// Full path of the new file (see buildDatabasePath()).
        QString path;
        SecretString password;
        SecretString passwordRepeat;
        /// Optional second factor (Token/OTP need secondFactor; see SecondFactor::generateToken()).
        SecondFactorType secondFactorType = SecondFactorType::None;
        SecretString secondFactor;
        /// C#: ManageDatabaseHelper.CreateDatabase adds the new path to the known list.
        bool addToKnownDatabases = true;
    };

    /// @p settings may be null (known list not updated); @p service may be null (no
    /// "is loaded database" checks, no operations on the open database).
    explicit DatabaseOperations(std::shared_ptr<AppSettings> settings, DatabaseService *service = nullptr,
                                QObject *parent = nullptr);
    /// Tests only: cheaper Argon2 parameters (steps.txt 3.3).
    DatabaseOperations(std::shared_ptr<AppSettings> settings, DatabaseService *service, const KdfParams &kdf,
                       QObject *parent = nullptr);
    ~DatabaseOperations() override;

    // --- Pure helpers --------------------------------------------------------------------

    /// File extension of databases (".epdb").
    static QString databaseExtension();
    /// Removes characters that are invalid in file names on any supported OS (C#:
    /// CreateDatabaseDialogPage.databaseName_TextChanged with Path.GetInvalidFileNameChars) and trims.
    static QString sanitizeDatabaseName(const QString &name);
    /// <folder>/<sanitized name or "Database">.epdb (C#: CreateDatabaseDialogPage.CreateDatabasePath).
    /// Returns an empty string if @p folder is empty.
    static QString buildDatabasePath(const QString &folder, const QString &name);
    /// "<name>_backup_yyyy-MM-dd_HH-mm-ss.epdb" for backup exports.
    static QString defaultBackupFileName(const QString &databaseName, const QDateTime &time);
    /// Validates a new master password: PasswordTooShort (checked first, as in C#), PasswordsDoNotMatch.
    static DatabaseOpStatus validateNewPassword(const SecretString &password, const SecretString &repeat);
    /// Path of a LocalFileDatabaseSource, empty for other sources.
    static QString localPath(const IDatabaseSource *source);
    /// Compares file paths (canonical when the files exist; case-insensitive on Windows).
    static bool isSamePath(const QString &a, const QString &b);

    /// Synchronous part of createDatabase(): Success or the first failing check, in C# order
    /// (CreateDatabaseDialog.Dialog_Closing: match, path, length, exists) plus second factor checks.
    static DatabaseOpStatus validateCreate(const CreateRequest &request);

    // --- File operations -------------------------------------------------------------------

    /**
     * Creates a new, empty database (C#: Database.CreateNewDatabase). Fails with the
     * validateCreate() errors or IoError; never overwrites an existing file.
     * result.path = the new file.
     */
    void createDatabase(const CreateRequest &request, DatabaseOpCallback callback);

    /**
     * Renames a database file inside its folder to "<sanitized newName><old extension>" and updates
     * the known list / last database (the C# dialog exists but was never wired up). The "<path>.bak"
     * backup is renamed along. Errors: InvalidPath (empty name), NotFound, PathAlreadyExists,
     * IsLoadedDatabase (the open database), IoError. result.path = the new path.
     */
    DatabaseOpResult renameDatabase(const QString &path, const QString &newName);

    /**
     * Deletes a database file (and its "<path>.bak") and removes it from the known list
     * (C#: ManageDatabasePage.DeleteDatabase). Errors: LastDatabase (the only known database),
     * IsLoadedDatabase, IoError. A missing file is only removed from the list (Success).
     * Asking for confirmation (C#: ConfirmDeleteDatabaseDialog) is the UI's job.
     */
    DatabaseOpResult deleteDatabase(const QString &path);

    /**
     * Verifies @p password (and @p secondFactor) for an existing database file and adds it to the
     * known list on success (C#: ManageDatabaseHelper.ImportDatabase). Errors: InvalidPath,
     * NotFound, WrongPassword, NeedsSecondFactor, WrongFormat. result.path = @p path.
     */
    void importDatabaseFile(const QString &path, const SecretString &password, const SecretString &secondFactor,
                            DatabaseOpCallback callback);

    /**
     * Decrypts a database file and returns its items, e.g. to import them into the open database
     * after the user selected them (C#: Database.GetItemsFromFile + ImportPasswordsDialog; then
     * call DatabaseService::importItems()). Does not touch the known list.
     */
    void readItemsFromFile(const QString &path, const SecretString &password, const SecretString &secondFactor,
                           ItemsCallback callback);

    /**
     * readItemsFromFile() + DatabaseService::importItems() with all items (no selection step).
     * Errors: NoDatabaseOpen, ReadOnly, decryption errors.
     */
    void importPasswordsFromFile(const QString &path, const SecretString &password, const SecretString &secondFactor,
                                 DatabaseService::ImportMode mode, DatabaseOpCallback callback);

    /**
     * Writes a copy of the open database to @p targetPath (export, export with new password,
     * export selected items). Refuses the file of the open database (SameAsOpenDatabase).
     * See DatabaseService::exportDatabase().
     */
    void exportDatabase(const QString &targetPath, const DatabaseService::ExportOptions &options,
                        DatabaseOpCallback callback);
    /// Backup = exportDatabase() with all items and the current password.
    void backupDatabase(const QString &targetPath, DatabaseOpCallback callback);

    /**
     * Opens @p path as a temporary, read-only database (C#: TemporaryDatabaseHelper; databases
     * opened by file association / "load temporary database"). The path is NOT added to the known
     * list. Result: DatabaseService::openFinished. Returns false if no service is set or
     * DatabaseService::open() rejected the request.
     */
    bool openTemporaryDatabase(const QString &path, const SecretString &password,
                               const SecretString &secondFactor = SecretString());

    /// Blocks until all background jobs are done and their callbacks were delivered.
    void waitForBackgroundWork();

private:
    void post(const std::function<void()> &fn);
    bool isOpenDatabase(const QString &path) const;
    QStringList knownExistingPaths() const;
    void replaceKnownPath(const QString &oldPath, const QString &newPath);

    std::shared_ptr<AppSettings> m_settings;
    QPointer<DatabaseService> m_service;
    KdfParams m_kdf;
    QThreadPool m_pool;
    int m_pendingJobs = 0;
};

} // namespace EasePass::Core
