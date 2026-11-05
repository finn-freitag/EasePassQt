#pragma once

#include <QObject>

namespace EasePass::Core {
class DatabaseService;
class InactivityManager;
} // namespace EasePass::Core

namespace EasePass::App {

/**
 * Inactivity logout for the QML app (steps.txt T19; C#: MainWindow.InactivityHelper_InactivityStarted,
 * Helper/Logout/AutoLogoutContentDialog). Enables Core::InactivityManager while a database is
 * open, exposes the warning countdown to QML (AutoLogoutDialog) and locks the database when
 * the time is up. Input events reach reportActivity() through ActivityEventFilter.
 *
 * preventLogout(true/false) pairs suspend the logout while a dialog must not be interrupted
 * (C#: ManageSecondFactorDialog sets PreventAutologout while the new token is shown).
 */
class AutoLogoutService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool warningActive READ warningActive NOTIFY warningActiveChanged)
    Q_PROPERTY(int secondsLeft READ secondsLeft NOTIFY secondsLeftChanged)
    Q_PROPERTY(bool enabled READ enabled NOTIFY enabledChanged)
public:
    AutoLogoutService(Core::DatabaseService *service, Core::InactivityManager *inactivity, QObject *parent = nullptr);

    bool warningActive() const;
    int secondsLeft() const { return m_secondsLeft; }
    bool enabled() const;
    Core::InactivityManager *inactivity() const { return m_inactivity; }

    /// User input (event filter) and the dialog's "Stay logged in" button.
    Q_INVOKABLE void reportActivity();
    Q_INVOKABLE void preventLogout(bool prevent);

signals:
    void warningActiveChanged();
    void secondsLeftChanged();
    void enabledChanged();
    /// After the automatic lock (C#: InfoMessages.AutomaticallyLoggedOut).
    void loggedOut();

private:
    void setSecondsLeft(int s);

    Core::DatabaseService *m_service;
    Core::InactivityManager *m_inactivity;
    int m_secondsLeft = 0;
};

} // namespace EasePass::App
