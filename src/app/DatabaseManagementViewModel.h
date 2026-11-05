#pragma once

#include "PasswordItem.h"

#include <QList>
#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <memory>

namespace EasePass::Core {
class AppSettings;
class DatabaseOperations;
class DatabaseService;
} // namespace EasePass::Core

namespace EasePass::App {

class ClipboardService;

/**
 * Database management page (steps.txt T16; C#: Views/ManageDatabasePage.xaml.cs and its dialogs
 * ChangePasswordDialog, ManageSecondFactorDialog, AddSecondFactorConfirmationDialog,
 * ConfirmDeleteDatabaseDialog, SelectExportPasswordsDialog, ImportPasswordsDialog,
 * ExportPasswordsHelper, ManageDatabaseHelper).
 *
 * `databases` lists every known database file with its state; file operations (rename, delete,
 * copy path) work on any entry. Operations that need the decrypted database (change password,
 * second factor, import into, export, backup, print) work on the OPEN database: C# unlocked a
 * second, unloaded database in place for them, the Qt service holds exactly one open database
 * (see steps.txt T16 deviations). Loading another database goes through the login page
 * (DatabaseController::switchTo).
 *
 * Results of asynchronous operations arrive as signals with a translated message; `message`
 * additionally carries every user-visible outcome for the page's InfoBar.
 */
class DatabaseManagementViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList databases READ databases NOTIFY databasesChanged)
    Q_PROPERTY(QString loadedPath READ loadedPath NOTIFY databasesChanged)
    Q_PROPERTY(bool databaseOpen READ databaseOpen NOTIFY stateChanged)
    Q_PROPERTY(bool readOnly READ readOnly NOTIFY stateChanged)
    Q_PROPERTY(bool temporary READ temporary NOTIFY stateChanged)
    Q_PROPERTY(QString databaseName READ databaseName NOTIFY stateChanged)
    Q_PROPERTY(bool secondFactorEnabled READ secondFactorEnabled NOTIFY stateChanged)
    Q_PROPERTY(int secondFactorType READ secondFactorType NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QStringList itemNames READ itemNames NOTIFY itemsChanged)
    Q_PROPERTY(QStringList importCandidates READ importCandidates NOTIFY importCandidatesChanged)
public:
    /// Same values as Theme.Severity in QML.
    enum Severity { Info, Success, Warning, Error };
    Q_ENUM(Severity)

    DatabaseManagementViewModel(Core::DatabaseService *service, Core::DatabaseOperations *operations,
                                std::shared_ptr<Core::AppSettings> settings, ClipboardService *clipboard,
                                QObject *parent = nullptr);

    /// One map per known database: name, path, exists, loaded, temporary, state (translated text).
    /// The open temporary database is listed as well (it is not in the known list).
    QVariantList databases() const;
    QString loadedPath() const;
    bool databaseOpen() const;
    bool readOnly() const;
    bool temporary() const;
    QString databaseName() const;
    bool secondFactorEnabled() const;
    /// Core::SecondFactorType as int (0 none, 1 token, 2 OTP).
    int secondFactorType() const;
    bool busy() const { return m_running > 0; }
    /// Display names of the open database's items (selection for export).
    QStringList itemNames() const;
    /// Display names of the items read by readImportFile(), waiting for importSelected().
    QStringList importCandidates() const;

    /// File name without extension; the text the user must type to confirm a delete.
    Q_INVOKABLE static QString nameForPath(const QString &path);
    /// Copies @p path to the clipboard (not a secret, no auto-clear).
    Q_INVOKABLE void copyPath(const QString &path);

    /// Renames the database file. Returns an empty string or the error message.
    Q_INVOKABLE QString rename(const QString &path, const QString &newName);
    /**
     * Deletes the database file after a typed confirmation: @p typedName must equal
     * nameForPath(@p path). Returns an empty string or the error message.
     */
    Q_INVOKABLE QString deleteDatabase(const QString &path, const QString &typedName);

    /// Result: passwordChangeFinished.
    Q_INVOKABLE void changePassword(const QString &current, const QString &newPassword, const QString &repeat);

    /// Current token of the open database (empty if it has none); shown in the manage dialog.
    Q_INVOKABLE QString currentSecondFactorToken() const;
    /// A new 12-digit token (C#: TokenHelper.Generate(12)).
    Q_INVOKABLE static QString generateSecondFactorToken();
    /// Result: secondFactorFinished. @p type: 1 token, 2 OTP; ignored when !enabled.
    Q_INVOKABLE void applySecondFactor(bool enabled, int type, const QString &token);

    /// Default file name for a backup of the open database.
    Q_INVOKABLE QString defaultBackupFileName() const;
    /// Export of the items at @p indexes (database order) with the current password. Result: exportFinished.
    Q_INVOKABLE void exportDatabase(const QString &target, const QVariantList &indexes);
    /// Same with a new master password (validated like a new database password).
    Q_INVOKABLE void exportWithNewPassword(const QString &target, const QVariantList &indexes,
                                           const QString &newPassword, const QString &repeat);
    /// Empty string or the message why @p password / @p repeat cannot be a new master password.
    Q_INVOKABLE static QString validateNewPassword(const QString &password, const QString &repeat);
    /// Copy of the whole database. Result: exportFinished.
    Q_INVOKABLE void backup(const QString &target);

    /// Decrypts another database file; on success importCandidates holds its items. Result: importFileRead.
    Q_INVOKABLE void readImportFile(const QString &path, const QString &password, const QString &secondFactor);
    /// Imports the candidates at @p indexes (Append or Override). Returns false if nothing was imported.
    Q_INVOKABLE bool importSelected(const QVariantList &indexes, bool overrideExisting);
    Q_INVOKABLE void cancelImport();

signals:
    void databasesChanged();
    void stateChanged();
    void busyChanged();
    void itemsChanged();
    void importCandidatesChanged();
    void message(int severity, const QString &text);
    void passwordChangeFinished(bool ok, const QString &message);
    void secondFactorFinished(bool ok, const QString &message);
    void exportFinished(bool ok, const QString &message);
    void importFileRead(bool ok, const QString &message, bool needsSecondFactor);

private:
    void begin();
    void end();
    QList<Core::PasswordItem> itemsAt(const QVariantList &indexes, const QList<Core::PasswordItem> &from) const;
    void startExport(const QString &target, const QVariantList &indexes, const QString *newPassword);

    Core::DatabaseService *m_service;
    Core::DatabaseOperations *m_operations;
    std::shared_ptr<Core::AppSettings> m_settings;
    ClipboardService *m_clipboard;
    QList<Core::PasswordItem> m_importItems;
    int m_running = 0;
};

} // namespace EasePass::App
