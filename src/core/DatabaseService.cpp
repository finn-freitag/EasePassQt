#include "DatabaseService.h"

#include "LocalFileDatabaseSource.h"

#include <QCoreApplication>
#include <QLoggingCategory>
#include <QMetaObject>
#include <utility>

namespace EasePass::Core {

Q_LOGGING_CATEGORY(lcDatabaseService, "easepass.core.databaseservice")

DatabaseService::DatabaseService(QObject *parent) : DatabaseService(KdfParams::production(), parent) {}

DatabaseService::DatabaseService(const KdfParams &kdf, QObject *parent) : QObject(parent), m_kdf(kdf) {
    // One worker thread: open, saves and logout run strictly in submission order and a
    // source never sees concurrent calls.
    m_pool.setMaxThreadCount(1);
    m_saver = new DeferredSaveManager([this]() { startSaveJob(true); }, DeferredSaveManager::DefaultDelayMs, this);
}

DatabaseService::~DatabaseService() {
    shutdown();
}

// --- Open / close ---------------------------------------------------------------------------

bool DatabaseService::open(std::shared_ptr<IDatabaseSource> source, const SecretString &password,
                           const SecretString &secondFactor, bool temporary) {
    if (!source || m_state == State::Opening) {
        return false;
    }
    if (m_state == State::Open) {
        close();
    }

    const quint64 openId = ++m_openId;
    m_openingTemporary = temporary;
    setState(State::Opening);

    LoadResult early = LoadResult::Success;
    switch (source->availability()) {
    case IDatabaseSource::Availability::Unavailable:
        early = LoadResult::SourceUnavailable;
        break;
    case IDatabaseSource::Availability::LockedByOtherUser:
        early = LoadResult::SourceLocked;
        break;
    case IDatabaseSource::Availability::Available:
    case IDatabaseSource::Availability::UnknownState:
        break; // UnknownState: let getBytes() decide
    }

    ++m_pendingJobs;
    if (early != LoadResult::Success) {
        // Deliver asynchronously as well, so callers see the same behaviour for every result.
        QMetaObject::invokeMethod(
            this, [this, openId, early]() { finishOpen(openId, early, nullptr, nullptr); }, Qt::QueuedConnection);
        return true;
    }

    m_pool.start([this, openId, source = std::move(source), password = SecretString(password),
                  secondFactor = SecretString(secondFactor), kdf = m_kdf]() mutable {
        auto db = std::make_shared<DatabaseFile>();
        LoadResult result = LoadResult::DatabaseNotFound;
        if (std::optional<QByteArray> bytes = source->getBytes()) {
            result = DatabaseFile::loadFromBytes(*bytes, password, *db, secondFactor, kdf);
        }
        if (result == LoadResult::Success) {
            source->login();
        } else {
            db.reset();
        }
        password.clear();
        secondFactor.clear();
        // The source is released on the UI thread (QObject affinity), never here.
        QMetaObject::invokeMethod(
            this,
            [this, openId, result, db = std::move(db), source = std::move(source)]() mutable {
                finishOpen(openId, result, std::move(db), std::move(source));
            },
            Qt::QueuedConnection);
    });
    return true;
}

void DatabaseService::finishOpen(quint64 openId, LoadResult result, std::shared_ptr<DatabaseFile> db,
                                 std::shared_ptr<IDatabaseSource> source) {
    --m_pendingJobs;

    if (openId != m_openId || m_state != State::Opening) {
        // Superseded: close() was called while opening.
        if (db) {
            db->wipeSecrets();
        }
        if (result == LoadResult::Success && source) {
            enqueueLogout(std::move(source));
        }
        return;
    }

    if (result != LoadResult::Success || !db || !source) {
        setState(State::Closed);
        emit openFinished(result == LoadResult::Success ? LoadResult::Error : result);
        return;
    }

    m_db = std::move(db);
    m_source = std::move(source);
    m_temporary = m_openingTemporary;
    m_db->isReadOnly = m_source->isReadOnly() || m_temporary;
    connect(m_source.get(), &IDatabaseSource::changed, this, &DatabaseService::onSourceChanged);
    ++m_session;
    m_changeGeneration = 0;
    m_savedGeneration = 0;

    setState(State::Open);
    emit opened();
    emit itemsChanged();
    emit openFinished(LoadResult::Success);
}

void DatabaseService::close() {
    if (m_state == State::Closed) {
        return;
    }
    if (m_state == State::Opening) {
        ++m_openId; // the running open job's result will be discarded (and logged out)
        setState(State::Closed);
        return;
    }

    // Flush: the final save takes its own snapshot, so the secrets below can be wiped now.
    m_saver->cancelPending();
    if (isDirty() && !isReadOnly()) {
        startSaveJob(false);
    }

    disconnect(m_source.get(), nullptr, this, nullptr);
    enqueueLogout(std::move(m_source));
    m_source.reset();

    m_db->wipeSecrets();
    m_db->items.clear();
    m_db.reset();
    m_temporary = false;
    ++m_session;
    m_changeGeneration = 0;
    m_savedGeneration = 0;

    setState(State::Closed);
    emit itemsChanged();
    emit closed();
}

void DatabaseService::shutdown() {
    close();
    waitForBackgroundWork();
}

void DatabaseService::waitForBackgroundWork() {
    while (m_pendingJobs > 0) {
        m_pool.waitForDone();
        QCoreApplication::sendPostedEvents(this, QEvent::MetaCall);
    }
}

void DatabaseService::enqueueLogout(std::shared_ptr<IDatabaseSource> source) {
    if (!source) {
        return;
    }
    ++m_pendingJobs;
    m_pool.start([this, source = std::move(source)]() mutable {
        source->logout();
        QMetaObject::invokeMethod(
            this, [this, source = std::move(source)]() { --m_pendingJobs; }, Qt::QueuedConnection);
    });
}

void DatabaseService::setState(State state) {
    if (m_state == state) {
        return;
    }
    m_state = state;
    emit stateChanged(state);
}

void DatabaseService::onSourceChanged() {
    if (!m_source || !m_db) {
        return;
    }
    const bool readOnly = m_source->isReadOnly() || m_temporary;
    if (readOnly != m_db->isReadOnly) {
        m_db->isReadOnly = readOnly;
        emit readOnlyChanged(readOnly);
    }
    emit sourceChanged();
}

bool DatabaseService::makePermanent() {
    if (!isOpen() || !m_temporary) {
        return false;
    }
    m_temporary = false;
    // DatabaseOperations::openTemporaryDatabase forces the local file source read-only.
    if (auto *local = dynamic_cast<LocalFileDatabaseSource *>(m_source.get())) {
        local->setReadOnly(false);
    }
    onSourceChanged(); // read-only state + display name
    return true;
}

bool DatabaseService::isReadOnly() const {
    return m_db && m_db->isReadOnly;
}

// --- Items ----------------------------------------------------------------------------------

const QList<PasswordItem> &DatabaseService::items() const {
    static const QList<PasswordItem> empty;
    return m_db ? m_db->items : empty;
}

int DatabaseService::countOccurrences(const QString &password) const {
    return m_db ? m_db->countOccurrences(password) : 0;
}

bool DatabaseService::canMutate() const {
    return m_state == State::Open && m_db && !m_db->isReadOnly;
}

void DatabaseService::markChanged(bool scheduleSave) {
    ++m_changeGeneration;
    emit itemsChanged();
    if (scheduleSave) {
        m_saver->requestSave();
    }
}

bool DatabaseService::addItem(const PasswordItem &item) {
    if (!canMutate()) {
        return false;
    }
    m_db->addItem(item);
    markChanged(true);
    return true;
}

bool DatabaseService::updateItem(int index, const PasswordItem &item) {
    if (!canMutate() || index < 0 || index >= m_db->items.size()) {
        return false;
    }
    m_db->updateItem(index, item);
    markChanged(true);
    return true;
}

bool DatabaseService::deleteItem(int index) {
    if (!canMutate() || index < 0 || index >= m_db->items.size()) {
        return false;
    }
    m_db->deleteItem(index);
    markChanged(true);
    return true;
}

bool DatabaseService::setItems(const QList<PasswordItem> &items) {
    if (!canMutate()) {
        return false;
    }
    m_db->items = items;
    markChanged(true);
    return true;
}

bool DatabaseService::registerClick(int index, const IClock *clock) {
    if (!canMutate() || index < 0 || index >= m_db->items.size()) {
        return false;
    }
    m_db->items[index].registerClick(clock);
    ++m_changeGeneration; // no itemsChanged(): only popularity data changed, no save scheduled
    return true;
}

bool DatabaseService::resetPopularity() {
    if (!canMutate()) {
        return false;
    }
    m_db->resetPopularity();
    markChanged(true);
    return true;
}

// --- Saving ---------------------------------------------------------------------------------

void DatabaseService::requestSave() {
    if (canMutate()) {
        m_saver->requestSave();
    }
}

void DatabaseService::saveNow() {
    if (canMutate() && isDirty()) {
        m_saver->saveNow();
    }
}

void DatabaseService::startSaveJob(bool fromSaver, SaveDone done) {
    if (!m_db || !m_source || m_db->isReadOnly) {
        // C#: DatabaseItem.SaveDatabase returns false for read-only databases.
        if (fromSaver) {
            m_saver->saveFinished(false, tr("No writable database is open."));
        }
        if (done) {
            QMetaObject::invokeMethod(this, [done]() { done(false); }, Qt::QueuedConnection);
        }
        return;
    }

    // Snapshot on the UI thread (cheap: implicitly shared items, copied secrets). The worker
    // only touches the snapshot, which wipes its secrets when destroyed.
    auto snapshot = std::make_shared<DatabaseFile>(*m_db);
    std::shared_ptr<IDatabaseSource> source = m_source;
    const quint64 generation = m_changeGeneration;
    const quint64 session = m_session;
    const QString description = m_source->sourceDescription();

    ++m_pendingJobs;
    if (m_savesRunning++ == 0) {
        emit savingChanged(true);
    }

    m_pool.start([this, snapshot = std::move(snapshot), source = std::move(source), generation, session, fromSaver,
                  description, kdf = m_kdf, done = std::move(done)]() mutable {
        const QByteArray bytes = snapshot->serialize(kdf);
        snapshot.reset();
        const bool ok = !bytes.isEmpty() && source->saveBytes(bytes);
        QMetaObject::invokeMethod(
            this,
            [this, ok, generation, session, fromSaver, description, source = std::move(source),
             done = std::move(done)]() { finishSave(ok, generation, session, fromSaver, description, done); },
            Qt::QueuedConnection);
    });
}

void DatabaseService::finishSave(bool ok, quint64 generation, quint64 session, bool fromSaver,
                                 const QString &description, const SaveDone &done) {
    --m_pendingJobs;
    --m_savesRunning;

    if (ok && session == m_session && generation > m_savedGeneration) {
        m_savedGeneration = generation;
    }

    const QString message = ok ? QString() : tr("Could not save the database to \"%1\".").arg(description);
    if (!ok) {
        qCWarning(lcDatabaseService) << "Saving database failed:" << description;
    }

    emit saveFinished(ok);
    if (!ok) {
        emit saveFailed(message);
    }
    if (fromSaver) {
        m_saver->saveFinished(ok, message);
    }
    if (m_savesRunning == 0) {
        emit savingChanged(false);
    }
    if (done) {
        done(ok);
    }
}

void DatabaseService::forceSave(DatabaseOpCallback callback) {
    const DatabaseOpStatus status = mutationStatus();
    if (status != DatabaseOpStatus::Success) {
        postResult(callback, status);
        return;
    }
    // C#: ForceSaveAsync cancels pending scheduled saves and saves immediately.
    m_saver->cancelPending();
    const QString description = m_source->sourceDescription();
    startSaveJob(false, [callback = std::move(callback), description](bool ok) {
        if (callback) {
            callback(DatabaseOpResult(ok ? DatabaseOpStatus::Success : DatabaseOpStatus::IoError, description));
        }
    });
}

// --- Management operations (T6) ---------------------------------------------------------------

DatabaseOpStatus DatabaseService::mutationStatus() const {
    if (m_state != State::Open || !m_db || !m_source) {
        return DatabaseOpStatus::NoDatabaseOpen;
    }
    if (m_db->isReadOnly) {
        return DatabaseOpStatus::ReadOnly;
    }
    return DatabaseOpStatus::Success;
}

void DatabaseService::postResult(const DatabaseOpCallback &callback, const DatabaseOpResult &result) {
    if (!callback) {
        return;
    }
    QMetaObject::invokeMethod(this, [callback, result]() { callback(result); }, Qt::QueuedConnection);
}

QString DatabaseService::displayName() const {
    if (!isOpen() || !m_source) {
        return QString();
    }
    // C#: DatabaseItem.MakeDatabaseName
    QString name = m_source->databaseName();
    if (m_temporary) {
        name += tr(" (Temp)");
    }
    if (isReadOnly()) {
        name += tr(" (ReadOnly)");
    }
    return name;
}

SecondFactorType DatabaseService::secondFactorType() const {
    if (!m_db || !m_db->useSecondFactor) {
        return SecondFactorType::None;
    }
    return SecondFactor::typeFromInt(m_db->secondFactorType);
}

void DatabaseService::applySecretChange(const std::function<void(DatabaseFile &)> &change, SaveDone done) {
    // Keep the previous values to restore them if the immediate save fails: otherwise a later
    // deferred save would silently write secrets the user was told were NOT applied.
    const SecretString oldPassword = m_db->masterPassword;
    const SecretString oldSecondFactor = m_db->secondFactor;
    const bool oldUseSecondFactor = m_db->useSecondFactor;
    const int oldSecondFactorType = m_db->secondFactorType;

    change(*m_db);
    const quint64 secretGeneration = ++m_secretGeneration;
    const quint64 session = m_session;
    ++m_changeGeneration;
    emit databaseSettingsChanged();

    m_saver->cancelPending(); // the immediate save below contains every pending change
    startSaveJob(false, [this, secretGeneration, session, oldPassword, oldSecondFactor, oldUseSecondFactor,
                         oldSecondFactorType, done = std::move(done)](bool ok) {
        if (!ok && m_db && session == m_session && secretGeneration == m_secretGeneration) {
            m_db->masterPassword = oldPassword;
            m_db->secondFactor = oldSecondFactor;
            m_db->useSecondFactor = oldUseSecondFactor;
            m_db->secondFactorType = oldSecondFactorType;
            ++m_secretGeneration;
            emit databaseSettingsChanged();
        }
        if (done) {
            done(ok);
        }
    });
}

void DatabaseService::changeMasterPassword(const SecretString &currentPassword, const SecretString &newPassword,
                                           const SecretString &repeatPassword, DatabaseOpCallback callback) {
    DatabaseOpStatus status = mutationStatus();
    // C#: ChangePasswordPage.ChangePassword checks length, then the current password, then the repeat.
    if (status == DatabaseOpStatus::Success && newPassword.codePointCount() < MinMasterPasswordLength) {
        status = DatabaseOpStatus::PasswordTooShort;
    }
    if (status == DatabaseOpStatus::Success && (currentPassword.isEmpty() || currentPassword != m_db->masterPassword)) {
        status = DatabaseOpStatus::WrongPassword;
    }
    if (status == DatabaseOpStatus::Success && newPassword != repeatPassword) {
        status = DatabaseOpStatus::PasswordsDoNotMatch;
    }
    if (status != DatabaseOpStatus::Success) {
        postResult(callback, status);
        return;
    }

    const QString description = m_source->sourceDescription();
    applySecretChange([&newPassword](DatabaseFile &db) { db.masterPassword = newPassword; },
                      [callback = std::move(callback), description](bool ok) {
                          if (callback) {
                              callback(DatabaseOpResult(ok ? DatabaseOpStatus::Success : DatabaseOpStatus::IoError,
                                                        description));
                          }
                      });
}

void DatabaseService::setSecondFactor(SecondFactorType type, const SecretString &token, DatabaseOpCallback callback) {
    DatabaseOpStatus status = mutationStatus();
    if (status == DatabaseOpStatus::Success && !SecondFactor::isSupported(type)) {
        status = DatabaseOpStatus::UnsupportedSecondFactorType;
    }
    if (status == DatabaseOpStatus::Success && SecondFactor::requiresToken(type) && token.isEmpty()) {
        status = DatabaseOpStatus::InvalidSecondFactor;
    }
    if (status != DatabaseOpStatus::Success) {
        postResult(callback, status);
        return;
    }

    const QString description = m_source->sourceDescription();
    applySecretChange(
        [type, &token](DatabaseFile &db) {
            // C#: ManageSecondFactorDialog sets the token to null when the second factor is disabled.
            db.useSecondFactor = type != SecondFactorType::None;
            db.secondFactorType = static_cast<int>(type);
            if (db.useSecondFactor) {
                db.secondFactor = token;
            } else {
                db.secondFactor.clear();
            }
        },
        [callback = std::move(callback), description](bool ok) {
            if (callback) {
                callback(DatabaseOpResult(ok ? DatabaseOpStatus::Success : DatabaseOpStatus::IoError, description));
            }
        });
}

void DatabaseService::rotateOneTimeSecondFactor(TokenCallback callback) {
    DatabaseOpStatus status = mutationStatus();
    if (status == DatabaseOpStatus::Success && secondFactorType() != SecondFactorType::OTP) {
        status = DatabaseOpStatus::InvalidSecondFactor;
    }
    if (status != DatabaseOpStatus::Success) {
        if (callback) {
            QMetaObject::invokeMethod(
                this, [callback, status]() { callback(DatabaseOpResult(status), SecretString()); },
                Qt::QueuedConnection);
        }
        return;
    }

    // C#: LoginPage.TryLogin -> TokenHelper.Generate(12), ShowSecondFactorDialog, SaveAsync
    SecretString token = SecondFactor::generateToken();
    const QString description = m_source->sourceDescription();
    applySecretChange([&token](DatabaseFile &db) { db.secondFactor = token; },
                      [callback = std::move(callback), token, description](bool ok) {
                          if (callback) {
                              callback(DatabaseOpResult(ok ? DatabaseOpStatus::Success : DatabaseOpStatus::IoError,
                                                        description),
                                       ok ? token : SecretString());
                          }
                      });
}

bool DatabaseService::importItems(const QList<PasswordItem> &items, ImportMode mode) {
    if (!canMutate()) {
        return false;
    }
    if (mode == ImportMode::Override) {
        m_db->items.clear();
    } else if (items.isEmpty()) {
        return true;
    }
    m_db->items.append(items);
    markChanged(true);
    return true;
}

void DatabaseService::exportDatabase(std::shared_ptr<IDatabaseSource> target, const ExportOptions &options,
                                     DatabaseOpCallback callback) {
    DatabaseOpStatus status = DatabaseOpStatus::Success;
    if (m_state != State::Open || !m_db || !m_source) {
        status = DatabaseOpStatus::NoDatabaseOpen;
    } else if (!target) {
        status = DatabaseOpStatus::InvalidPath;
    } else if (target == m_source) {
        status = DatabaseOpStatus::SameAsOpenDatabase;
    } else if (target->isReadOnly()) {
        status = DatabaseOpStatus::ReadOnly;
    } else if (options.newPassword && options.newPassword->codePointCount() < MinMasterPasswordLength) {
        status = DatabaseOpStatus::PasswordTooShort;
    }
    if (status != DatabaseOpStatus::Success) {
        postResult(callback, DatabaseOpResult(status, target ? target->sourceDescription() : QString()));
        return;
    }

    // C#: ExportPasswordsHelper.Export copies MasterPassword (or the new one), SecondFactor and
    // Settings, then SetNewPasswords(selected items) and ForceSaveAsync on the new source.
    auto snapshot = std::make_shared<DatabaseFile>(*m_db);
    snapshot->isReadOnly = false;
    if (options.newPassword) {
        snapshot->masterPassword = *options.newPassword;
    }
    if (options.items) {
        snapshot->items = *options.items;
    }
    const QString description = target->sourceDescription();

    ++m_pendingJobs;
    m_pool.start([this, snapshot = std::move(snapshot), target = std::move(target), description, kdf = m_kdf,
                  callback = std::move(callback)]() mutable {
        const QByteArray bytes = snapshot->serialize(kdf);
        snapshot.reset();
        const bool ok = !bytes.isEmpty() && target->saveBytes(bytes);
        if (!ok) {
            qCWarning(lcDatabaseService) << "Exporting database failed:" << description;
        }
        // The target is released on the UI thread (QObject affinity), never here.
        QMetaObject::invokeMethod(
            this,
            [this, ok, description, target = std::move(target), callback = std::move(callback)]() {
                --m_pendingJobs;
                if (callback) {
                    callback(DatabaseOpResult(ok ? DatabaseOpStatus::Success : DatabaseOpStatus::IoError, description));
                }
            },
            Qt::QueuedConnection);
    });
}

} // namespace EasePass::Core
