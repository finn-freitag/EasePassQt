#pragma once

#include "DatabaseOperations.h"
#include "DatabaseService.h"

#include <QObject>
#include <functional>
#include <memory>

namespace EasePass::App {

/**
 * QML-facing wrapper around DatabaseService + DatabaseOperations (steps.txt T12):
 * open / lock / create / save with busy and error state. Errors are user-visible,
 * translated strings (DatabaseOpResult::describe).
 * C#: Views/LoginPage.xaml.cs, Views/CreateDatabasePage, MainWindow lock handling.
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
public:
    using SourceFactory = std::function<std::shared_ptr<Core::IDatabaseSource>(const QString &path)>;

    DatabaseController(Core::DatabaseService *service, Core::DatabaseOperations *operations,
                       std::shared_ptr<Core::AppSettings> settings, QObject *parent = nullptr);

    /// Replaces how paths become sources (default: LocalFileDatabaseSource). Tests inject fakes.
    void setSourceFactory(SourceFactory factory) { m_sourceFactory = std::move(factory); }

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
    bool isFirstRun() const { return knownDatabases().isEmpty(); }

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
    /// Loads another database (C#: ManageDatabasePage.Load_DatabaseItem_Click): selects @p path on
    /// the login page and locks the open database, so the user enters that database's password
    /// there. Returns false if @p path is the open database, unknown or missing.
    Q_INVOKABLE bool switchTo(const QString &path);
    /// C#: "Load temp database" (TemporaryDatabaseHelper.LoadImportedDatabase): the open temporary
    /// database becomes a normal one and is added to the known databases.
    Q_INVOKABLE bool makePermanent();

signals:
    void stateChanged();
    void busyChanged();
    void errorMessageChanged();
    void needsSecondFactorChanged();
    void dirtyChanged();
    void savingChanged();
    void knownDatabasesChanged();
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
    QString m_openingPath;
    QString m_errorMessage;
    bool m_needsSecondFactor = false;
    int m_operationsRunning = 0;
    QString m_openAfterCreatePath;
    Core::SecretString m_openAfterCreatePassword;
};

} // namespace EasePass::App
