#include "DatabaseService.h"

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
                           const SecretString &secondFactor) {
    if (!source || m_state == State::Opening) {
        return false;
    }
    if (m_state == State::Open) {
        close();
    }

    const quint64 openId = ++m_openId;
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
    m_db->isReadOnly = m_source->isReadOnly();
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
    const bool readOnly = m_source->isReadOnly();
    if (readOnly != m_db->isReadOnly) {
        m_db->isReadOnly = readOnly;
        emit readOnlyChanged(readOnly);
    }
    emit sourceChanged();
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

void DatabaseService::startSaveJob(bool fromSaver) {
    if (!m_db || !m_source || m_db->isReadOnly) {
        // C#: DatabaseItem.SaveDatabase returns false for read-only databases.
        if (fromSaver) {
            m_saver->saveFinished(false, tr("No writable database is open."));
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
                  description, kdf = m_kdf]() mutable {
        const QByteArray bytes = snapshot->serialize(kdf);
        snapshot.reset();
        const bool ok = !bytes.isEmpty() && source->saveBytes(bytes);
        QMetaObject::invokeMethod(
            this,
            [this, ok, generation, session, fromSaver, description, source = std::move(source)]() {
                finishSave(ok, generation, session, fromSaver, description);
            },
            Qt::QueuedConnection);
    });
}

void DatabaseService::finishSave(bool ok, quint64 generation, quint64 session, bool fromSaver,
                                 const QString &description) {
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
}

} // namespace EasePass::Core
