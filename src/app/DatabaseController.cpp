#include "DatabaseController.h"

#include "AppSettings.h"
#include "LocalFileDatabaseSource.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>

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
            rotateOneTimeTokenIfNeeded();
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

QStringList DatabaseController::knownDatabases() const {
    // Only files that still exist (C#: ManageDatabaseHelper.LoadDatabasesToCombobox skips missing ones).
    QStringList result;
    if (!m_settings)
        return result;
    for (const QString &p : m_settings->databasePaths())
        if (QFileInfo::exists(p) && !result.contains(p))
            result << p;
    return result;
}

QString DatabaseController::defaultDatabasePath() {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath(QStringLiteral("easepass") + DatabaseOperations::databaseExtension());
}

QString DatabaseController::displayNameForPath(const QString &path) {
    return QFileInfo(toLocalPath(path)).completeBaseName();
}

QString DatabaseController::buildDatabasePath(const QString &folderOrUrl, const QString &name) {
    return DatabaseOperations::buildDatabasePath(toLocalPath(folderOrUrl), name);
}

QString DatabaseController::toLocalPath(const QString &pathOrUrl) {
    const QUrl url(pathOrUrl);
    return url.isLocalFile() ? url.toLocalFile() : pathOrUrl;
}

QString DatabaseController::lastDatabase() const { return m_settings ? m_settings->lastDatabase() : QString(); }

bool DatabaseController::open(const QString &pathOrUrl, const QString &password, const QString &secondFactor) {
    const QString path = toLocalPath(pathOrUrl);
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

void DatabaseController::createDatabase(const QString &pathOrUrl, const QString &password, const QString &repeatPassword) {
    const QString path = toLocalPath(pathOrUrl);
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
    if (!path.isEmpty())
        QDir().mkpath(QFileInfo(path).absolutePath()); // e.g. first run: AppData does not exist yet
    beginOperation();
    m_operations->createDatabase(request, [this](const DatabaseOpResult &result) {
        endOperation();
        if (result.ok()) {
            if (m_settings)
                m_settings->setLastDatabase(result.path);
            emit knownDatabasesChanged();
            emit created(result.path);
            if (!m_openAfterCreatePath.isEmpty()) {
                const QString openPath = result.path.isEmpty() ? m_openAfterCreatePath : result.path;
                const QString pw = m_openAfterCreatePassword.toQString();
                m_openAfterCreatePath.clear();
                m_openAfterCreatePassword.wipe();
                open(openPath, pw);
            }
        } else {
            m_openAfterCreatePath.clear();
            m_openAfterCreatePassword.wipe();
            setError(result.message());
        }
    });
}

void DatabaseController::createAndOpen(const QString &path, const QString &password, const QString &repeatPassword) {
    m_openAfterCreatePath = toLocalPath(path);
    m_openAfterCreatePassword = SecretString(password);
    createDatabase(path, password, repeatPassword);
    if (m_operationsRunning == 0) { // validation failed synchronously
        m_openAfterCreatePath.clear();
        m_openAfterCreatePassword.wipe();
    }
}

void DatabaseController::importDatabase(const QString &pathOrUrl, const QString &password, const QString &secondFactor) {
    const QString path = toLocalPath(pathOrUrl);
    if (!m_operations || path.isEmpty()) {
        setError(DatabaseOpResult::describe(DatabaseOpStatus::InvalidPath));
        return;
    }
    setError(QString());
    beginOperation();
    m_operations->importDatabaseFile(path, SecretString(password), SecretString(secondFactor),
                                     [this](const DatabaseOpResult &result) {
                                         endOperation();
                                         setNeedsSecondFactor(result.status == DatabaseOpStatus::NeedsSecondFactor);
                                         if (!result.ok()) {
                                             setError(result.message());
                                             return;
                                         }
                                         if (m_settings)
                                             m_settings->setLastDatabase(result.path);
                                         emit knownDatabasesChanged();
                                         emit imported(result.path);
                                     });
}

void DatabaseController::rotateOneTimeTokenIfNeeded() {
    // C#: LoginPage.TryLogin - OTP databases get a new 12-digit token after every login.
    if (m_service->secondFactorType() != SecondFactorType::OTP)
        return;
    beginOperation();
    m_service->rotateOneTimeSecondFactor([this](const DatabaseOpResult &result, const SecretString &token) {
        endOperation();
        if (result.ok())
            emit oneTimeTokenIssued(token.toQString());
        else
            setError(result.message());
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
