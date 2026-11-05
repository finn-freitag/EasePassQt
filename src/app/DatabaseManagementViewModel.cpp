#include "DatabaseManagementViewModel.h"

#include "AppSettings.h"
#include "ClipboardService.h"
#include "DatabaseOperations.h"
#include "DatabaseService.h"
#include "SecondFactor.h"

#include <QDateTime>
#include <QFileInfo>
#include <QPointer>
#include <QUrl>
#include <QVariantMap>
#include <algorithm>

namespace EasePass::App {

using namespace EasePass::Core;

namespace {
QString localPath(const QString &pathOrUrl) {
    const QUrl url(pathOrUrl);
    return url.isLocalFile() ? url.toLocalFile() : pathOrUrl;
}
} // namespace

DatabaseManagementViewModel::DatabaseManagementViewModel(DatabaseService *service, DatabaseOperations *operations,
                                                         std::shared_ptr<AppSettings> settings,
                                                         ClipboardService *clipboard, QObject *parent)
    : QObject(parent), m_service(service), m_operations(operations), m_settings(std::move(settings)),
      m_clipboard(clipboard) {
    auto stateUpdate = [this] {
        emit stateChanged();
        emit databasesChanged();
        emit itemsChanged();
    };
    connect(m_service, &DatabaseService::stateChanged, this, stateUpdate);
    connect(m_service, &DatabaseService::sourceChanged, this, stateUpdate);
    connect(m_service, &DatabaseService::readOnlyChanged, this, &DatabaseManagementViewModel::stateChanged);
    connect(m_service, &DatabaseService::databaseSettingsChanged, this, &DatabaseManagementViewModel::stateChanged);
    connect(m_service, &DatabaseService::itemsChanged, this, &DatabaseManagementViewModel::itemsChanged);
    connect(m_service, &DatabaseService::closed, this, [this] { cancelImport(); });
    if (m_settings) {
        connect(m_settings.get(), &AppSettings::databasePathsChanged, this,
                &DatabaseManagementViewModel::databasesChanged);
        connect(m_settings.get(), &AppSettings::lastDatabaseChanged, this,
                &DatabaseManagementViewModel::databasesChanged);
    }
}

// --- State ------------------------------------------------------------------------------------

QString DatabaseManagementViewModel::loadedPath() const {
    return m_service->isOpen() ? DatabaseOperations::localPath(m_service->source()) : QString();
}

bool DatabaseManagementViewModel::databaseOpen() const { return m_service->isOpen(); }
bool DatabaseManagementViewModel::readOnly() const { return m_service->isReadOnly(); }
bool DatabaseManagementViewModel::temporary() const { return m_service->isTemporary(); }
QString DatabaseManagementViewModel::databaseName() const { return m_service->displayName(); }
bool DatabaseManagementViewModel::secondFactorEnabled() const { return m_service->isOpen() && m_service->usesSecondFactor(); }

int DatabaseManagementViewModel::secondFactorType() const {
    return secondFactorEnabled() ? static_cast<int>(m_service->secondFactorType()) : 0;
}

QVariantList DatabaseManagementViewModel::databases() const {
    QVariantList result;
    const QString loaded = loadedPath();
    bool loadedListed = false;
    QStringList seen;
    const QStringList paths = m_settings ? m_settings->databasePaths() : QStringList();
    for (const QString &path : paths) {
        if (path.isEmpty() || seen.contains(path))
            continue;
        seen << path;
        const bool exists = QFileInfo::exists(path);
        const bool isLoaded = !loaded.isEmpty() && DatabaseOperations::isSamePath(path, loaded);
        loadedListed = loadedListed || isLoaded;
        QString state;
        if (isLoaded)
            state = tr("Loaded");
        else if (!exists)
            state = tr("File not found");
        result << QVariantMap{{QStringLiteral("name"), nameForPath(path)},
                              {QStringLiteral("path"), path},
                              {QStringLiteral("exists"), exists},
                              {QStringLiteral("loaded"), isLoaded},
                              {QStringLiteral("temporary"), isLoaded && m_service->isTemporary()},
                              {QStringLiteral("state"), state}};
    }
    // The open database is not in the known list when it was opened temporarily (C#: the temp
    // database shows up in the database box until "Load temp database" adds it).
    if (m_service->isOpen() && !loadedListed) {
        const QString path = loaded;
        result << QVariantMap{{QStringLiteral("name"), path.isEmpty() ? m_service->displayName() : nameForPath(path)},
                              {QStringLiteral("path"), path},
                              {QStringLiteral("exists"), true},
                              {QStringLiteral("loaded"), true},
                              {QStringLiteral("temporary"), m_service->isTemporary()},
                              {QStringLiteral("state"), m_service->isTemporary() ? tr("Loaded (temporary)") : tr("Loaded")}};
    }
    return result;
}

QStringList DatabaseManagementViewModel::itemNames() const {
    QStringList names;
    if (!m_service->isOpen())
        return names;
    for (const PasswordItem &item : m_service->items())
        names << item.displayName;
    return names;
}

QStringList DatabaseManagementViewModel::importCandidates() const {
    QStringList names;
    for (const PasswordItem &item : m_importItems)
        names << item.displayName;
    return names;
}

QString DatabaseManagementViewModel::nameForPath(const QString &path) {
    return QFileInfo(localPath(path)).completeBaseName();
}

// --- File operations --------------------------------------------------------------------------

void DatabaseManagementViewModel::copyPath(const QString &path) {
    if (m_clipboard && !path.isEmpty())
        m_clipboard->copy(path, false);
}

QString DatabaseManagementViewModel::rename(const QString &path, const QString &newName) {
    const DatabaseOpResult result = m_operations->renameDatabase(localPath(path), newName);
    if (!result.ok()) {
        emit message(Error, result.message());
        return result.message();
    }
    emit databasesChanged();
    emit message(Success, tr("Database renamed to %1").arg(nameForPath(result.path)));
    return QString();
}

QString DatabaseManagementViewModel::deleteDatabase(const QString &pathOrUrl, const QString &typedName) {
    const QString path = localPath(pathOrUrl);
    if (typedName != nameForPath(path)) {
        const QString msg = tr("The entered name does not match the database name.");
        emit message(Error, msg);
        return msg;
    }
    const DatabaseOpResult result = m_operations->deleteDatabase(path);
    if (!result.ok()) {
        emit message(Error, result.message());
        return result.message();
    }
    emit databasesChanged();
    emit message(Success, tr("Database %1 deleted").arg(nameForPath(path)));
    return QString();
}

// --- Open database: password + second factor ----------------------------------------------

void DatabaseManagementViewModel::changePassword(const QString &current, const QString &newPassword,
                                                 const QString &repeat) {
    begin();
    QPointer<DatabaseManagementViewModel> self(this);
    m_service->changeMasterPassword(SecretString(current), SecretString(newPassword), SecretString(repeat),
                                    [self](const DatabaseOpResult &result) {
                                        if (!self)
                                            return;
                                        self->end();
                                        // C#: InfoMessages.SuccessfullyChangedPassword
                                        const QString msg = result.ok() ? tr("Password changed successfully")
                                                                        : result.message();
                                        if (result.ok())
                                            emit self->message(Success, msg);
                                        emit self->passwordChangeFinished(result.ok(), msg);
                                    });
}

QString DatabaseManagementViewModel::currentSecondFactorToken() const {
    const DatabaseFile *db = m_service->database();
    return db && db->useSecondFactor ? db->secondFactor.toQString() : QString();
}

QString DatabaseManagementViewModel::generateSecondFactorToken() {
    return SecondFactor::generateToken(SecondFactor::DefaultTokenLength).toQString();
}

void DatabaseManagementViewModel::applySecondFactor(bool enabled, int type, const QString &token) {
    const SecondFactorType t = enabled ? SecondFactor::typeFromInt(type) : SecondFactorType::None;
    begin();
    QPointer<DatabaseManagementViewModel> self(this);
    m_service->setSecondFactor(t, enabled ? SecretString(token) : SecretString(),
                               [self, enabled](const DatabaseOpResult &result) {
                                   if (!self)
                                       return;
                                   self->end();
                                   QString msg = result.message();
                                   if (result.ok())
                                       msg = enabled ? tr("Second factor enabled") : tr("Second factor removed");
                                   emit self->message(result.ok() ? Success : Error, msg);
                                   emit self->secondFactorFinished(result.ok(), msg);
                               });
}

// --- Export / backup -----------------------------------------------------------------------

QString DatabaseManagementViewModel::defaultBackupFileName() const {
    const QString name = m_service->isOpen() && m_service->source() ? m_service->source()->databaseName()
                                                                    : QStringLiteral("Database");
    return DatabaseOperations::defaultBackupFileName(name, QDateTime::currentDateTime());
}

QList<PasswordItem> DatabaseManagementViewModel::itemsAt(const QVariantList &indexes,
                                                         const QList<PasswordItem> &from) const {
    QList<int> rows;
    for (const QVariant &v : indexes) {
        bool ok = false;
        const int i = v.toInt(&ok);
        if (ok && i >= 0 && i < from.size() && !rows.contains(i))
            rows << i;
    }
    std::sort(rows.begin(), rows.end()); // keep the database order
    QList<PasswordItem> items;
    for (int i : rows)
        items << from.at(i);
    return items;
}

void DatabaseManagementViewModel::startExport(const QString &target, const QVariantList &indexes,
                                              const QString *newPassword) {
    DatabaseService::ExportOptions options;
    options.items = itemsAt(indexes, m_service->items());
    if (newPassword)
        options.newPassword = SecretString(*newPassword);
    begin();
    QPointer<DatabaseManagementViewModel> self(this);
    m_operations->exportDatabase(localPath(target), options, [self](const DatabaseOpResult &result) {
        if (!self)
            return;
        self->end();
        // C#: InfoMessages.ExportDBSuccess / ExportDBFailed
        const QString msg = result.ok() ? tr("Database exported successfully")
                                        : tr("Export failed: %1").arg(result.message());
        emit self->message(result.ok() ? Success : Error, msg);
        emit self->exportFinished(result.ok(), msg);
    });
}

void DatabaseManagementViewModel::exportDatabase(const QString &target, const QVariantList &indexes) {
    startExport(target, indexes, nullptr);
}

void DatabaseManagementViewModel::exportWithNewPassword(const QString &target, const QVariantList &indexes,
                                                        const QString &newPassword, const QString &repeat) {
    const DatabaseOpStatus valid =
        DatabaseOperations::validateNewPassword(SecretString(newPassword), SecretString(repeat));
    if (valid != DatabaseOpStatus::Success) {
        const QString msg = DatabaseOpResult::describe(valid);
        // Asynchronous like every other result, so QML handles it in one place.
        QMetaObject::invokeMethod(this, [this, msg] { emit exportFinished(false, msg); }, Qt::QueuedConnection);
        return;
    }
    startExport(target, indexes, &newPassword);
}

QString DatabaseManagementViewModel::validateNewPassword(const QString &password, const QString &repeat) {
    const DatabaseOpStatus status = DatabaseOperations::validateNewPassword(SecretString(password), SecretString(repeat));
    return status == DatabaseOpStatus::Success ? QString() : DatabaseOpResult::describe(status);
}

void DatabaseManagementViewModel::backup(const QString &target) {
    begin();
    QPointer<DatabaseManagementViewModel> self(this);
    m_operations->backupDatabase(localPath(target), [self](const DatabaseOpResult &result) {
        if (!self)
            return;
        self->end();
        const QString msg = result.ok() ? tr("Backup saved to %1").arg(result.path)
                                        : tr("Backup failed: %1").arg(result.message());
        emit self->message(result.ok() ? Success : Error, msg);
        emit self->exportFinished(result.ok(), msg);
    });
}

// --- Import into the open database -------------------------------------------------------

void DatabaseManagementViewModel::readImportFile(const QString &path, const QString &password,
                                                 const QString &secondFactor) {
    cancelImport();
    begin();
    QPointer<DatabaseManagementViewModel> self(this);
    m_operations->readItemsFromFile(localPath(path), SecretString(password), SecretString(secondFactor),
                                    [self](const DatabaseOpResult &result, const QList<PasswordItem> &items) {
                                        if (!self)
                                            return;
                                        self->end();
                                        if (result.ok()) {
                                            self->m_importItems = items;
                                            emit self->importCandidatesChanged();
                                        }
                                        emit self->importFileRead(result.ok(), result.message(),
                                                                  result.status == DatabaseOpStatus::NeedsSecondFactor);
                                    });
}

bool DatabaseManagementViewModel::importSelected(const QVariantList &indexes, bool overrideExisting) {
    const QList<PasswordItem> items = itemsAt(indexes, m_importItems);
    if (items.isEmpty()) {
        emit message(Warning, tr("No passwords selected"));
        return false;
    }
    const bool ok = m_service->importItems(items, overrideExisting ? DatabaseService::ImportMode::Override
                                                                   : DatabaseService::ImportMode::Append);
    if (!ok) {
        emit message(Error, DatabaseOpResult::describe(m_service->isOpen() ? DatabaseOpStatus::ReadOnly
                                                                           : DatabaseOpStatus::NoDatabaseOpen));
        return false;
    }
    cancelImport();
    emit message(Success, tr("%n password(s) imported", nullptr, int(items.size())));
    return true;
}

void DatabaseManagementViewModel::cancelImport() {
    if (m_importItems.isEmpty())
        return;
    m_importItems.clear();
    emit importCandidatesChanged();
}

void DatabaseManagementViewModel::begin() {
    ++m_running;
    emit busyChanged();
}

void DatabaseManagementViewModel::end() {
    m_running = std::max(0, m_running - 1);
    emit busyChanged();
}

} // namespace EasePass::App
