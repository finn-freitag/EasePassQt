#include "AutoLogoutService.h"

#include "DatabaseService.h"
#include "InactivityManager.h"

#include <QLoggingCategory>
#include <algorithm>

Q_LOGGING_CATEGORY(lcAutoLogout, "easepass.app.autologout")

namespace EasePass::App {

using namespace EasePass::Core;

AutoLogoutService::AutoLogoutService(DatabaseService *service, InactivityManager *inactivity, QObject *parent)
    : QObject(parent), m_service(service), m_inactivity(inactivity) {
    connect(m_service, &DatabaseService::opened, m_inactivity, [this] { m_inactivity->setEnabled(true); });
    connect(m_service, &DatabaseService::closed, m_inactivity, [this] { m_inactivity->setEnabled(false); });
    connect(m_inactivity, &InactivityManager::enabledChanged, this, &AutoLogoutService::enabledChanged);
    connect(m_inactivity, &InactivityManager::warningActiveChanged, this, [this](bool active) {
        // The countdown must be current when QML reacts to the warning (dialog's start value).
        if (active)
            setSecondsLeft(std::max(0, m_inactivity->secondsUntilLogout()));
        emit warningActiveChanged();
    });
    connect(m_inactivity, &InactivityManager::warningStarted, this, &AutoLogoutService::setSecondsLeft);
    connect(m_inactivity, &InactivityManager::warningCountdown, this, &AutoLogoutService::setSecondsLeft);
    connect(m_inactivity, &InactivityManager::logoutRequested, this, [this] {
        if (!m_service->isOpen())
            return;
        qCInfo(lcAutoLogout) << "locking the database after" << m_inactivity->timeoutSeconds() << "s without input";
        m_service->lock();
        emit loggedOut();
    });
    if (m_service->isOpen())
        m_inactivity->setEnabled(true);
}

bool AutoLogoutService::warningActive() const { return m_inactivity->isWarningActive(); }
bool AutoLogoutService::enabled() const { return m_inactivity->isEnabled(); }

void AutoLogoutService::reportActivity() { m_inactivity->reportActivity(); }

void AutoLogoutService::preventLogout(bool prevent) { m_inactivity->setLogoutPrevented(prevent); }

void AutoLogoutService::setSecondsLeft(int s) {
    if (m_secondsLeft == s)
        return;
    m_secondsLeft = s;
    emit secondsLeftChanged();
}

} // namespace EasePass::App
