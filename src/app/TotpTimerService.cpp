#include "TotpTimerService.h"

#include "IClock.h"

namespace EasePass::App {

TotpTimerService::TotpTimerService(const Core::IClock *clock, QObject *parent)
    : QObject(parent), m_clock(clock ? clock : &Core::SystemClock::instance()) {
    m_timer.setInterval(1000);
    m_timer.setTimerType(Qt::CoarseTimer);
    connect(&m_timer, &QTimer::timeout, this, &TotpTimerService::tickNow);
}

qint64 TotpTimerService::now() const { return m_clock->currentSecsSinceEpoch(); }

void TotpTimerService::acquire() {
    if (m_subscribers++ == 0) {
        m_timer.start();
        emit runningChanged();
    }
}

void TotpTimerService::release() {
    if (m_subscribers == 0)
        return;
    if (--m_subscribers == 0) {
        m_timer.stop();
        emit runningChanged();
    }
}

void TotpTimerService::tickNow() { emit tick(now()); }

} // namespace EasePass::App
