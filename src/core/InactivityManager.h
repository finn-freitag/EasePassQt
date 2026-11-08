#pragma once

#include <QObject>
#include <QPointer>
#include <QTimer>

#include "IClock.h"
#include <algorithm>

namespace EasePass::Core {

class AppSettings;

/**
 * Logs the user out after AppSettings::inactivityLogoutMinutes without input.
 * C#: EasePass/Manager/InactivityManager.cs (polls once per second and compares against the
 * last input time). The UI layer forwards input events via reportActivity() (e.g. from an
 * application-wide event filter) and enables the manager only while a database is unlocked.
 *
 * Unlike C#, a warning phase precedes the logout: warningStarted()/warningCountdown(secondsLeft)
 * during the last warningSeconds(), warningCancelled() on activity, then logoutRequested().
 * Evaluation happens in tick() against IClock; an internal 1 s timer calls it while enabled.
 */
class InactivityManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool warningActive READ isWarningActive NOTIFY warningActiveChanged)
public:
    static constexpr int DefaultWarningSeconds = 30;

    explicit InactivityManager(AppSettings *settings, const IClock *clock = nullptr, QObject *parent = nullptr);

    bool isEnabled() const { return m_enabled; }
    /// Enabling (re)starts the inactivity period. Disabling cancels any warning.
    void setEnabled(bool enabled);

    /// Length of the warning phase; capped to half the logout timeout. 0 = no warning.
    void setWarningSeconds(int seconds) { m_warningSeconds = std::max(0, seconds); }
    int warningSeconds() const;

    /// Timeout in seconds taken from the settings.
    int timeoutSeconds() const;
    /// Seconds until logout (or -1 when disabled).
    int secondsUntilLogout() const;
    bool isWarningActive() const { return m_warning; }

    /// While prevented (C#: InactivityManager.PreventAutologout, e.g. while the second factor
    /// dialog shows a new token), the inactivity period keeps restarting and no warning or
    /// logout happens. Nested calls are counted.
    void setLogoutPrevented(bool prevented);
    bool isLogoutPrevented() const { return m_preventCount > 0; }

    /// Called by the UI for every user input event. Resets the inactivity period.
    Q_INVOKABLE void reportActivity();
    /// Evaluates the state; called by the internal timer once per second.
    void tick();

signals:
    void enabledChanged(bool enabled);
    void warningActiveChanged(bool active);
    void warningStarted(int secondsLeft);
    void warningCountdown(int secondsLeft);
    void warningCancelled();
    /// Emitted once when the timeout has elapsed; the manager disables itself afterwards.
    void logoutRequested();

private:
    void setWarning(bool on);

    QPointer<AppSettings> m_settings;
    const IClock *m_clock;
    QTimer m_timer;
    qint64 m_lastActivity = 0;
    int m_warningSeconds = DefaultWarningSeconds;
    int m_lastCountdown = -1;
    bool m_enabled = false;
    bool m_warning = false;
    int m_preventCount = 0;
};

} // namespace EasePass::Core
