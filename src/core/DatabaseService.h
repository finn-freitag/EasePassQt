#pragma once

#include "CryptoHelper.h"
#include "DatabaseFile.h"
#include "DatabaseOperationResult.h"
#include "DeferredSaveManager.h"
#include "IDatabaseSource.h"
#include "PasswordItem.h"
#include "SecondFactor.h"
#include "SecretString.h"

#include <QList>
#include <QObject>
#include <QThreadPool>
#include <functional>
#include <memory>
#include <optional>

namespace EasePass::Core {

/**
 * @brief Owns the currently opened database (replaces the DatabaseManager singleton's
 *        current-database part; known paths stay in AppSettings/DatabaseManager).
 *
 * C#: Core/Database/DatabaseItem.cs (Load/Unlock, SaveAsync, ForceSaveAsync, Dispose).
 *
 * Threading: the service lives on the UI thread; all public functions must be called there.
 * Key derivation (Argon2, slow), encryption and IDatabaseSource I/O run on ONE private worker
 * thread, so jobs (open, saves, logout) are executed strictly in submission order. Results come
 * back as queued signals; no public function blocks except waitForBackgroundWork()/shutdown().
 *
 * Opening: open() returns immediately; openFinished(LoadResult) follows asynchronously. The
 * source's availability is checked first (SourceUnavailable / SourceLocked); login() is called
 * on the source after a successful decryption.
 *
 * Mutations (addItem/updateItem/deleteItem/setItems) are rejected (return false) when no
 * database is open or it is read-only; otherwise itemsChanged() is emitted and a deferred save
 * is scheduled (DeferredSaveManager, default 5 s). Each save snapshots the database on the UI
 * thread and encrypts/writes it on the worker; failures are reported via saveFailed() and the
 * database stays dirty so the next save/close retries.
 *
 * close()/lock(): flushes unsaved changes (the save runs in the background with its own
 * snapshot), wipes master password/second factor, drops the items and calls logout() on the
 * source after the final save. shutdown() (also called by the destructor) additionally waits
 * for all background work - use it on application exit.
 *
 * Management operations on the open database (T6; C#: Views/ManageDatabasePage.xaml.cs,
 * Views/DialogPages/ChangePasswordPage.xaml.cs, Dialogs/ManageSecondFactorDialog.cs,
 * Helper/Security/ExportPasswordsHelper.cs, Helper/Database/ManageDatabaseHelper.cs):
 * changeMasterPassword(), setSecondFactor()/removeSecondFactor(), rotateOneTimeSecondFactor(),
 * importItems(), exportDatabase(), forceSave(). Their callbacks always run asynchronously on the
 * service's thread (also for validation errors) and are dropped if the service is destroyed.
 * Operations that change the master password or second factor save immediately; if that save
 * fails, the previous secrets are restored in memory (unless they were changed again meanwhile)
 * so the file and the password the user knows stay consistent.
 *
 * Temporary databases (C#: Helper/Database/TemporaryDatabaseHelper.cs, DatabaseItem.IsTemporaryDatabase):
 * open(..., temporary = true) treats the database as read-only regardless of the source and
 * displayName() appends " (Temp)". Callers must not add temporary databases to the known list.
 */
class DatabaseService : public QObject {
    Q_OBJECT
public:
    enum class State { Closed, Opening, Open };
    Q_ENUM(State)

    /// C#: ManageDatabaseHelper.ImportIntoDatabase / ImportPasswordsDialog ("Add" vs "Override").
    enum class ImportMode {
        Append,  ///< add all items after the existing ones; duplicates are kept (as in C#)
        Override ///< replace all existing items
    };
    Q_ENUM(ImportMode)

    /// Options for exportDatabase() (C#: ExportPasswordsHelper.Export).
    struct ExportOptions {
        /// Encrypt the copy with this password instead of the current one ("export with new
        /// password"). The second factor and its settings are kept, as in C#.
        std::optional<SecretString> newPassword;
        /// Items to write; std::nullopt = all items (SelectExportPasswordsDialog result otherwise).
        std::optional<QList<PasswordItem>> items;
    };

    using TokenCallback = std::function<void(const DatabaseOpResult &, const SecretString &token)>;

    explicit DatabaseService(QObject *parent = nullptr);
    /// Tests only: cheaper Argon2 parameters (steps.txt 3.3). Production code uses the other ctor.
    explicit DatabaseService(const KdfParams &kdf, QObject *parent = nullptr);
    ~DatabaseService() override;

    /// Starts opening @p source. Returns false (and does nothing) if @p source is null or another
    /// open is in progress. An already open database is closed first. @p temporary: see class doc.
    bool open(std::shared_ptr<IDatabaseSource> source, const SecretString &password,
              const SecretString &secondFactor = SecretString(), bool temporary = false);
    void close();
    void lock() { close(); }
    /// close() + wait until every background job (final save, logout) has finished.
    void shutdown();
    /// Blocks until all background jobs are done and their results were delivered.
    void waitForBackgroundWork();

    State state() const { return m_state; }
    bool isOpen() const { return m_state == State::Open; }
    bool isOpening() const { return m_state == State::Opening; }
    bool isReadOnly() const;
    bool isDirty() const { return isOpen() && m_changeGeneration != m_savedGeneration; }
    bool isSaving() const { return m_savesRunning > 0; }

    IDatabaseSource *source() const { return m_source.get(); }
    std::shared_ptr<IDatabaseSource> sharedSource() const { return m_source; }
    /// Read access to the opened database (settings flags etc.); nullptr when not open.
    const DatabaseFile *database() const { return m_db.get(); }

    const QList<PasswordItem> &items() const;
    int itemCount() const { return items().size(); }
    int countOccurrences(const QString &password) const;

    bool addItem(const PasswordItem &item);
    bool updateItem(int index, const PasswordItem &item);
    bool deleteItem(int index);
    bool setItems(const QList<PasswordItem> &items);
    /// Registers a click (popularity). Marks the database dirty but schedules no save: the
    /// click is persisted with the next save or on close.
    bool registerClick(int index, const IClock *clock = nullptr);
    /// Clears all click history across all items and schedules a deferred save (C#: ResetPopularity_Click).
    bool resetPopularity();

    int saveDelayMs() const { return m_saver->delayMs(); }
    void setSaveDelayMs(int delayMs) { m_saver->setDelayMs(delayMs); }
    /// Schedules a deferred save (debounced).
    void requestSave();
    /// Saves now (asynchronously) if there are unsaved changes.
    void saveNow();
    /// Cancels a pending deferred save and saves immediately, even without changes
    /// (C#: DatabaseItem.ForceSaveAsync). @p callback gets Success or IoError.
    void forceSave(DatabaseOpCallback callback = {});

    // --- Management operations (T6) -------------------------------------------------------

    bool isTemporary() const { return isOpen() && m_temporary; }
    /// Database name plus " (Temp)" / " (ReadOnly)" (C#: DatabaseItem.MakeDatabaseName). Empty when closed.
    QString displayName() const;
    bool usesSecondFactor() const { return m_db && m_db->useSecondFactor; }
    SecondFactorType secondFactorType() const;

    /**
     * Changes the master password and saves immediately (C#: ChangePasswordPage.ChangePassword).
     * Checks in C# order: new password too short -> PasswordTooShort, @p currentPassword wrong ->
     * WrongPassword (compared in constant time with the password used to open the database),
     * @p newPassword != @p repeatPassword -> PasswordsDoNotMatch. Also NoDatabaseOpen, ReadOnly,
     * IoError (save failed; the old password stays valid).
     */
    void changeMasterPassword(const SecretString &currentPassword, const SecretString &newPassword,
                              const SecretString &repeatPassword, DatabaseOpCallback callback = {});

    /**
     * Sets the second factor and saves immediately (C#: ManageDatabasePage.ManageSecondFactor_Click).
     * None removes it; Token/OTP need a non-empty @p token (see SecondFactor::generateToken());
     * Authenticator -> UnsupportedSecondFactorType. Asking the user for confirmation before
     * enabling (C#: AddSecondFactorConfirmationDialog) is the UI's job.
     */
    void setSecondFactor(SecondFactorType type, const SecretString &token, DatabaseOpCallback callback = {});
    void removeSecondFactor(DatabaseOpCallback callback = {}) {
        setSecondFactor(SecondFactorType::None, SecretString(), std::move(callback));
    }

    /**
     * OTP second factor: generates a new token, stores it and saves immediately. Call after every
     * successful login of an OTP database (C#: LoginPage.TryLogin). The callback receives the new
     * token only on Success - show it to the user then; on failure the old token stays valid.
     * Fails with InvalidSecondFactor if the database does not use an OTP second factor.
     */
    void rotateOneTimeSecondFactor(TokenCallback callback);

    /**
     * Imports items into the open database and schedules a deferred save
     * (C#: ManageDatabaseHelper.ImportIntoDatabase: Override clears first, then AddRange).
     * Returns false if no writable database is open.
     */
    bool importItems(const QList<PasswordItem> &items, ImportMode mode = ImportMode::Append);

    /**
     * Writes a copy of the open database to @p target (export, export with new password, export
     * selected items, backup; C#: ExportPasswordsHelper.Export, ManageDatabasePage.Export_BackupDatabase_Click).
     * Works for read-only databases, too. Errors: NoDatabaseOpen, InvalidPath (null target),
     * SameAsOpenDatabase (target is the open source), ReadOnly (target read-only),
     * PasswordTooShort (new password), IoError. result.path = target->sourceDescription().
     */
    void exportDatabase(std::shared_ptr<IDatabaseSource> target, const ExportOptions &options,
                        DatabaseOpCallback callback = {});

signals:
    void stateChanged(EasePass::Core::DatabaseService::State state);
    void openFinished(EasePass::Core::LoadResult result);
    void opened();
    void closed();
    void itemsChanged();
    void readOnlyChanged(bool readOnly);
    void sourceChanged();
    void savingChanged(bool saving);
    void saveFinished(bool ok);
    void saveFailed(const QString &message);
    /// Master password or second factor settings changed (or were restored after a failed save).
    void databaseSettingsChanged();

private:
    using SaveDone = std::function<void(bool ok)>;

    void setState(State state);
    void finishOpen(quint64 openId, LoadResult result, std::shared_ptr<DatabaseFile> db,
                    std::shared_ptr<IDatabaseSource> source);
    void startSaveJob(bool fromSaver, SaveDone done = {});
    void finishSave(bool ok, quint64 generation, quint64 session, bool fromSaver, const QString &description,
                    const SaveDone &done);
    void enqueueLogout(std::shared_ptr<IDatabaseSource> source);
    void onSourceChanged();
    bool canMutate() const;
    void markChanged(bool scheduleSave);
    /// Validation for operations that modify the open database: NoDatabaseOpen / ReadOnly / Success.
    DatabaseOpStatus mutationStatus() const;
    void postResult(const DatabaseOpCallback &callback, const DatabaseOpResult &result);
    /// Applies @p change to the secrets/settings, saves immediately and restores the previous
    /// values if that save fails. @p done is called on the UI thread with the save result.
    void applySecretChange(const std::function<void(DatabaseFile &)> &change, SaveDone done);

    KdfParams m_kdf;
    QThreadPool m_pool;
    DeferredSaveManager *m_saver = nullptr;

    State m_state = State::Closed;
    std::shared_ptr<IDatabaseSource> m_source;
    std::shared_ptr<DatabaseFile> m_db;
    bool m_temporary = false;
    bool m_openingTemporary = false;

    quint64 m_openId = 0;
    quint64 m_session = 0;
    quint64 m_changeGeneration = 0;
    quint64 m_savedGeneration = 0;
    quint64 m_secretGeneration = 0;
    int m_savesRunning = 0;
    int m_pendingJobs = 0; // jobs whose result callback has not run yet
};

} // namespace EasePass::Core
