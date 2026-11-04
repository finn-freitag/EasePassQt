#include "DatabaseController.h"

#include "AppSettings.h"
#include "LocalFileDatabaseSource.h"

namespace EasePass::App {

using namespace EasePass::Core;

DatabaseController::DatabaseController(DatabaseService *service, DatabaseOperations *operations,
                                       std::shared_ptr<AppSettings> settings, QObject *parent)
    : QObject(parent), m_service(service), m_operations(operations), m_settings(std::move(settings)),
      m_sourceFactory([](const QString &path) { return std::make_shared<LocalFileDatabaseSource>(path); }) {
    connect(m_service, &DatabaseService::stateChanged, this, [this] {
        emit stateChanged();
        emit busyChanged();
        emit dirtyChanged();
    });
    connect(m_service, &DatabaseService::readOnlyChanged, this, &DatabaseController::stateChanged);
    connect(m_service, &DatabaseService::sourceChanged, this, &DatabaseController::stateChanged);
    connect(m_service, &DatabaseService::itemsChanged, this, &DatabaseController::dirtyChanged);
    connect(m_service, &DatabaseService::savingChanged, this, [this] {
        emit savingChanged();
        emit dirtyChanged();
    });
    connect(m_service, &DatabaseService::saveFailed, this, [this](const QString &msg) { setError(msg); });
    connect(m_service, &DatabaseService::openFinished, this, [this](LoadResult result) {
        if (result == LoadResult::Success) {
            setError(QString());
            setNeedsSecondFactor(false);
            if (m_settings && !m_openingPath.isEmpty()) {
                m_settings->setLastDatabase(m_openingPath);
                emit knownDatabasesChanged();
            }
        } else {
            setNeedsSecondFactor(result == LoadResult::NeedsSecondFactor);
            const QString msg = DatabaseOpResult::describe(DatabaseOpResult::fromLoadResult(result));
            setError(msg);
            emit openFailed(msg);
        }
        m_openingPath.clear();
        emit busyChanged();
    });
    if (m_settings) {
        connect(m_settings.get(), &AppSettings::databasePathsChanged, this, &DatabaseController::knownDatabasesChanged);
        connect(m_settings.get(), &AppSettings::lastDatabaseChanged, this, &DatabaseController::knownDatabasesChanged);
    }
}

QString DatabaseController::databaseName() const { return m_service->isOpen() ? m_service->displayName() : QString(); }

QStringList DatabaseController::knownDatabases() const { return m_settings ? m_settings->databasePaths() : QStringList(); }

QString DatabaseController::lastDatabase() const { return m_settings ? m_settings->lastDatabase() : QString(); }

bool DatabaseController::open(const QString &path, const QString &password, const QString &secondFactor) {
    if (busy())
        return false;
    if (path.isEmpty()) {
        setError(DatabaseOpResult::describe(DatabaseOpStatus::InvalidPath));
        return false;
    }
    auto source = m_sourceFactory(path);
    if (!source) {
        setError(DatabaseOpResult::describe(DatabaseOpStatus::NotFound));
        return false;
    }
    setError(QString());
    m_openingPath = path;
    const bool started = m_service->open(source, SecretString(password), SecretString(secondFactor));
    if (!started) {
        m_openingPath.clear();
        setError(DatabaseOpResult::describe(DatabaseOpStatus::IoError));
    }
    emit busyChanged();
    return started;
}

void DatabaseController::lock() {
    setNeedsSecondFactor(false);
    m_service->lock();
}

void DatabaseController::saveNow() {
    if (!m_service->isOpen()) {
        setError(DatabaseOpResult::describe(DatabaseOpStatus::NoDatabaseOpen));
        return;
    }
    m_service->saveNow();
}

void DatabaseController::createDatabase(const QString &path, const QString &password, const QString &repeatPassword) {
    if (!m_operations) {
        setError(DatabaseOpResult::describe(DatabaseOpStatus::IoError));
        return;
    }
    DatabaseOperations::CreateRequest request;
    request.path = path;
    request.password = SecretString(password);
    request.passwordRepeat = SecretString(repeatPassword);
    const DatabaseOpStatus valid = DatabaseOperations::validateCreate(request);
    if (valid != DatabaseOpStatus::Success) {
        setError(DatabaseOpResult::describe(valid));
        return;
    }
    setError(QString());
    beginOperation();
    m_operations->createDatabase(request, [this](const DatabaseOpResult &result) {
        endOperation();
        if (result.ok()) {
            emit knownDatabasesChanged();
            emit created(result.path);
        } else {
            setError(result.message());
        }
    });
}

void DatabaseController::setError(const QString &message) {
    if (m_errorMessage == message)
        return;
    m_errorMessage = message;
    emit errorMessageChanged();
}

void DatabaseController::setNeedsSecondFactor(bool on) {
    if (m_needsSecondFactor == on)
        return;
    m_needsSecondFactor = on;
    emit needsSecondFactorChanged();
}

void DatabaseController::beginOperation() {
    ++m_operationsRunning;
    emit busyChanged();
}

void DatabaseController::endOperation() {
    m_operationsRunning = std::max(0, m_operationsRunning - 1);
    emit busyChanged();
}

} // namespace EasePass::App
