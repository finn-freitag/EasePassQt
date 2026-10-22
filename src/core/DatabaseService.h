#pragma once

#include "CryptoHelper.h"
#include "DatabaseFile.h"
#include "DeferredSaveManager.h"
#include "IDatabaseSource.h"
#include "PasswordItem.h"
#include "SecretString.h"

#include <QList>
#include <QObject>
#include <QThreadPool>
#include <memory>

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
 */
class DatabaseService : public QObject {
    Q_OBJECT
public:
    enum class State { Closed, Opening, Open };
    Q_ENUM(State)

    explicit DatabaseService(QObject *parent = nullptr);
    /// Tests only: cheaper Argon2 parameters (steps.txt 3.3). Production code uses the other ctor.
    explicit DatabaseService(const KdfParams &kdf, QObject *parent = nullptr);
    ~DatabaseService() override;

    /// Starts opening @p source. Returns false (and does nothing) if @p source is null or another
    /// open is in progress. An already open database is closed first.
    bool open(std::shared_ptr<IDatabaseSource> source, const SecretString &password,
              const SecretString &secondFactor = SecretString());
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
    bool registerClick(int index);

    int saveDelayMs() const { return m_saver->delayMs(); }
    void setSaveDelayMs(int delayMs) { m_saver->setDelayMs(delayMs); }
    /// Schedules a deferred save (debounced).
    void requestSave();
    /// Saves now (asynchronously) if there are unsaved changes.
    void saveNow();

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

private:
    void setState(State state);
    void finishOpen(quint64 openId, LoadResult result, std::shared_ptr<DatabaseFile> db,
                    std::shared_ptr<IDatabaseSource> source);
    void startSaveJob(bool fromSaver);
    void finishSave(bool ok, quint64 generation, quint64 session, bool fromSaver, const QString &description);
    void enqueueLogout(std::shared_ptr<IDatabaseSource> source);
    void onSourceChanged();
    bool canMutate() const;
    void markChanged(bool scheduleSave);

    KdfParams m_kdf;
    QThreadPool m_pool;
    DeferredSaveManager *m_saver = nullptr;

    State m_state = State::Closed;
    std::shared_ptr<IDatabaseSource> m_source;
    std::shared_ptr<DatabaseFile> m_db;

    quint64 m_openId = 0;
    quint64 m_session = 0;
    quint64 m_changeGeneration = 0;
    quint64 m_savedGeneration = 0;
    int m_savesRunning = 0;
    int m_pendingJobs = 0; // jobs whose result callback has not run yet
};

} // namespace EasePass::Core
