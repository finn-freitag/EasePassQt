#include "InactivityManager.h"

#include "AppSettings.h"
#include <algorithm>

namespace EasePass::Core {

InactivityManager::InactivityManager(AppSettings *settings, const IClock *clock, QObject *parent)
    : QObject(parent), m_settings(settings), m_clock(clock ? clock : &SystemClock::instance()) {
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &InactivityManager::tick);
}

void InactivityManager::setEnabled(bool enabled) {
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    if (enabled) {
        m_lastActivity = m_clock->currentSecsSinceEpoch();
        m_timer.start();
    } else {
        m_timer.stop();
        setWarning(false);
    }
    emit enabledChanged(enabled);
}

int InactivityManager::timeoutSeconds() const {
    const int minutes =
        m_settings ? m_settings->inactivityLogoutMinutes() : AppSettings::Defaults::InactivityLogoutMinutes;
    return std::max(1, minutes) * 60;
}

int InactivityManager::warningSeconds() const {
    return std::min(m_warningSeconds, timeoutSeconds() / 2);
}

int InactivityManager::secondsUntilLogout() const {
    if (!m_enabled)
        return -1;
    const qint64 left = m_lastActivity + timeoutSeconds() - m_clock->currentSecsSinceEpoch();
    return int(std::max<qint64>(0, left));
}

void InactivityManager::reportActivity() {
    if (!m_enabled)
        return;
    m_lastActivity = m_clock->currentSecsSinceEpoch();
    if (m_warning) {
        setWarning(false);
        emit warningCancelled();
    }
}

void InactivityManager::setWarning(bool on) {
    if (m_warning == on)
        return;
    m_warning = on;
    m_lastCountdown = -1;
    emit warningActiveChanged(on);
}

void InactivityManager::setLogoutPrevented(bool prevented) {
    m_preventCount = std::max(0, m_preventCount + (prevented ? 1 : -1));
    if (m_preventCount > 0) {
        reportActivity(); // cancels a running warning
    } else if (m_enabled) {
        m_lastActivity = m_clock->currentSecsSinceEpoch(); // a full period after the dialog closes
    }
}

void InactivityManager::tick() {
    if (!m_enabled)
        return;
    if (m_preventCount > 0) {
        m_lastActivity = m_clock->currentSecsSinceEpoch();
        return;
    }
    const int left = secondsUntilLogout();
    if (left <= 0) {
        setEnabled(false);
        emit logoutRequested();
        return;
    }
    const int warn = warningSeconds();
    if (warn > 0 && left <= warn) {
        if (!m_warning) {
            setWarning(true);
            emit warningStarted(left);
        }
        if (left != m_lastCountdown) {
            m_lastCountdown = left;
            emit warningCountdown(left);
        }
    }
}

} // namespace EasePass::Core
