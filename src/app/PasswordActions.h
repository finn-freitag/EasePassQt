#pragma once

#include <QObject>
#include <QVariantList>

namespace EasePass::Core {
class DatabaseOperations;
class DatabaseService;
} // namespace EasePass::Core

namespace EasePass::App {

class ClipboardService;
class TotpTimerService;

/**
 * Actions on list rows by DATABASE index (steps.txt T14): the context menu works on the
 * right-clicked row, which need not be the selected item.
 * C#: Views/PasswordsPage.xaml.cs RightclickedItem_* handlers, ExportPasswordsHelper.
 */
class PasswordActions : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool readOnly READ readOnly NOTIFY readOnlyChanged)
public:
    PasswordActions(Core::DatabaseService *service, Core::DatabaseOperations *operations,
                    ClipboardService *clipboard, TotpTimerService *totpTimer, QObject *parent = nullptr);

    bool busy() const { return m_busy; }
    bool readOnly() const;

    Q_INVOKABLE bool copyPassword(int index);
    Q_INVOKABLE bool copyUsername(int index);
    Q_INVOKABLE bool copyEmail(int index);
    /// Copies the current TOTP code (C#: CopyTOTPToken).
    Q_INVOKABLE bool copyTotp(int index);
    /// Current TOTP code of item @p index ("" without valid 2FA).
    Q_INVOKABLE QString totpCode(int index) const;
    /// C#: selecting an item adds a click (popularity sort).
    Q_INVOKABLE bool registerClick(int index);
    Q_INVOKABLE bool deleteItem(int index);
    /// Deletes several items (highest index first so indices stay valid).
    Q_INVOKABLE int deleteItems(const QVariantList &indices);
    /// Exports the given items into a new database file. Empty @p newPassword keeps the
    /// current master password and second factor (C#: Export / "Export new PW").
    Q_INVOKABLE bool exportItems(const QVariantList &indices, const QString &pathOrUrl,
                                 const QString &newPassword = QString());

signals:
    void busyChanged();
    void readOnlyChanged();
    void exportFinished(bool ok, const QString &message, const QString &path);

private:
    bool validIndex(int index) const;
    void setBusy(bool busy);

    Core::DatabaseService *m_service;
    Core::DatabaseOperations *m_operations;
    ClipboardService *m_clipboard;
    TotpTimerService *m_totpTimer;
    bool m_busy = false;
};

} // namespace EasePass::App
