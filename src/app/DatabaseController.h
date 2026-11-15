#pragma once

#include "DatabaseOperations.h"
#include "DatabaseService.h"
#include "RemoteDatabase.h"

#include <QObject>
#include <functional>
#include <memory>

namespace EasePass::App {

/**
 * QML-facing wrapper around DatabaseService + DatabaseOperations (steps.txt T12):
 * open / lock / create / save with busy and error state. Errors are user-visible,
 * translated strings (DatabaseOpResult::describe).
 * C#: Views/LoginPage.xaml.cs, Views/CreateDatabasePage, MainWindow lock handling.
 *
 * Databases of database-source plugins (setRemoteDatabases) are listed after the local files
 * and are opened, selected and remembered by their key instead of a path
 * (C#: Database.GetAllUnloadedDatabases appends ExtensionManager.DatabaseSources).
 */
class DatabaseController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isOpen READ isOpen NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(bool needsSecondFactor READ needsSecondFactor NOTIFY needsSecondFactorChanged)
    Q_PROPERTY(QString databaseName READ databaseName NOTIFY stateChanged)
    Q_PROPERTY(bool readOnly READ readOnly NOTIFY stateChanged)
    Q_PROPERTY(bool temporary READ temporary NOTIFY stateChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY dirtyChanged)
    Q_PROPERTY(bool saving READ saving NOTIFY savingChanged)
    Q_PROPERTY(QStringList knownDatabases READ knownDatabases NOTIFY knownDatabasesChanged)
    Q_PROPERTY(QString lastDatabase READ lastDatabase NOTIFY knownDatabasesChanged)
    Q_PROPERTY(bool isFirstRun READ isFirstRun NOTIFY knownDatabasesChanged)
    Q_PROPERTY(bool hasRemoteProviders READ hasRemoteProviders NOTIFY remoteProvidersChanged)
    Q_PROPERTY(bool remoteLoading READ remoteLoading NOTIFY remoteProvidersChanged)
public:
    using SourceFactory = std::function<std::shared_ptr<Core::IDatabaseSource>(const QString &path)>;

    DatabaseController(Core::DatabaseService *service, Core::DatabaseOperations *operations,
                       std::shared_ptr<Core::AppSettings> settings, QObject *parent = nullptr);

    /// Replaces how paths become sources (default: LocalFileDatabaseSource). Tests inject fakes.
    void setSourceFactory(SourceFactory factory) { m_sourceFactory = std::move(factory); }

    /// Databases of plugins; replaces the previous list.
    void setRemoteDatabases(QList<RemoteDatabase> databases);
    /// @p refresh asks the plugins again (refreshRemoteDatabases); null = no such plugin is installed.
    void setRemoteProviders(std::function<void()> refresh, bool loading);
    bool hasRemoteProviders() const { return bool(m_remoteRefresh); }
    bool remoteLoading() const { return m_remoteLoading; }
    Q_INVOKABLE void refreshRemoteDatabases();
    Q_INVOKABLE bool isRemote(const QString &pathOrKey) const { return RemoteDatabase::isKey(pathOrKey); }
    /// Name for the database picker: file name without extension, or the plugin database's name.
    Q_INVOKABLE QString databaseLabel(const QString &pathOrKey) const;
    /// The full path, or where the plugin says the database is stored.
    Q_INVOKABLE QString databaseDescription(const QString &pathOrKey) const;
    /// Path or key of the open database (empty if none).
    QString openDatabase() const;

    bool isOpen() const { return m_service->isOpen(); }
    bool busy() const { return m_service->isOpening() || m_operationsRunning > 0; }
    QString errorMessage() const { return m_errorMessage; }
    bool needsSecondFactor() const { return m_needsSecondFactor; }
    QString databaseName() const;
    bool readOnly() const { return m_service->isReadOnly(); }
    bool temporary() const { return m_service->isTemporary(); }
    bool dirty() const { return m_service->isDirty(); }
    bool saving() const { return m_service->isSaving(); }
    QStringList knownDatabases() const;
    QString lastDatabase() const;
    /// No known database file exists yet -> show the Register (OOBE) page.
    bool isFirstRun() const;

    /// C#: DefaultSettingsValues.databasePaths -> <AppData>/easepass.epdb.
    Q_INVOKABLE static QString defaultDatabasePath();
    /// File name without extension, for the database picker.
    Q_INVOKABLE static QString displayNameForPath(const QString &path);
    /// <folder>/<sanitized name>.epdb (C#: CreateDatabaseDialog).
    Q_INVOKABLE static QString buildDatabasePath(const QString &folderOrUrl, const QString &name);
    /// Accepts local paths and file:// URLs (QML FileDialog).
    Q_INVOKABLE static QString toLocalPath(const QString &pathOrUrl);

    Q_INVOKABLE bool open(const QString &path, const QString &password, const QString &secondFactor = QString());
    Q_INVOKABLE void lock();
    Q_INVOKABLE void saveNow();
    Q_INVOKABLE void createDatabase(const QString &path, const QString &password, const QString &repeatPassword);
    /// Register page / advanced create: create, then open it with the same password.
    Q_INVOKABLE void createAndOpen(const QString &path, const QString &password, const QString &repeatPassword);
    /// C#: ManageDatabaseHelper.ImportDatabase - verifies the password, adds the file to the
    /// known databases and selects it (does not open it).
    Q_INVOKABLE void importDatabase(const QString &path, const QString &password,
                                    const QString &secondFactor = QString());
    Q_INVOKABLE void clearError() { setError(QString()); }
    /// Another database was picked on the login page: the error and the second-factor
    /// question belong to the previous one.
    Q_INVOKABLE void resetLoginPrompt();
    /// Loads another database (C#: ManageDatabasePage.Load_DatabaseItem_Click): selects @p path on
    /// the login page and locks the open database, so the user enters that database's password
    /// there. Returns false if @p path is the open database, unknown or missing.
    Q_INVOKABLE bool switchTo(const QString &path);
    /// C#: "Load temp database" (TemporaryDatabaseHelper.LoadImportedDatabase): the open temporary
    /// database becomes a normal one and is added to the known databases.
    Q_INVOKABLE bool makePermanent();

    /**
     * Database from the command line / a second launch (T19). A known database is selected on
     * the login page; another existing file is listed as temporary database and opened with
     * DatabaseService temporary = true (C#: TemporaryDatabaseHelper.HandleImportTempDatabase).
     * Returns false for a missing file.
     */
    Q_INVOKABLE bool setStartupDatabase(const QString &path);
    /// The temporary entry offered on the login page (empty if none).
    Q_INVOKABLE QString temporaryPath() const { return m_temporaryPath; }
    Q_INVOKABLE bool isTemporaryPath(const QString &path) const;

signals:
    void stateChanged();
    void busyChanged();
    void errorMessageChanged();
    void needsSecondFactorChanged();
    void dirtyChanged();
    void savingChanged();
    void knownDatabasesChanged();
    void remoteProvidersChanged();
    void openFailed(const QString &message);
    void created(const QString &path);
    void imported(const QString &path);
    /// One-time second factor rotated after login; the UI must show it once (C#: ShowSecondFactorDialog).
    void oneTimeTokenIssued(const QString &token);

private:
    void setError(const QString &message);
    void setNeedsSecondFactor(bool on);
    void beginOperation();
    void endOperation();
    void rotateOneTimeTokenIfNeeded();

    Core::DatabaseService *m_service;
    Core::DatabaseOperations *m_operations;
    std::shared_ptr<Core::AppSettings> m_settings;
    SourceFactory m_sourceFactory;
    QList<RemoteDatabase> m_remote;
    std::function<void()> m_remoteRefresh;
    bool m_remoteLoading = false;
    QString m_openingPath;
    QString m_temporaryPath;
    QString m_errorMessage;
    bool m_needsSecondFactor = false;
    int m_operationsRunning = 0;
    QString m_openAfterCreatePath;
    Core::SecretString m_openAfterCreatePassword;
};

} // namespace EasePass::App
