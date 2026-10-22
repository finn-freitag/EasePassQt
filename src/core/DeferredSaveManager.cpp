#include "DeferredSaveManager.h"

#include <utility>

namespace EasePass::Core {

DeferredSaveManager::DeferredSaveManager(SaveFunction saveFunction, int delayMs, QObject *parent)
    : QObject(parent), m_saveFunction(std::move(saveFunction)) {
    m_timer.setSingleShot(true);
    m_timer.setInterval(delayMs < 0 ? 0 : delayMs);
    connect(&m_timer, &QTimer::timeout, this, &DeferredSaveManager::trigger);
}

int DeferredSaveManager::delayMs() const {
    return m_timer.interval();
}

void DeferredSaveManager::setDelayMs(int delayMs) {
    m_timer.setInterval(delayMs < 0 ? 0 : delayMs);
}

void DeferredSaveManager::requestSave() {
    m_timer.start(); // restarts a running timer => debounce
}

void DeferredSaveManager::flush() {
    if (!m_timer.isActive()) {
        return;
    }
    m_timer.stop();
    trigger();
}

void DeferredSaveManager::saveNow() {
    requestSave();
    flush();
}

void DeferredSaveManager::cancelPending() {
    const bool wasScheduled = isScheduled();
    m_timer.stop();
    m_rerunQueued = false;
    if (wasScheduled && !m_saving) {
        emit idle();
    }
}

bool DeferredSaveManager::isScheduled() const {
    return m_timer.isActive() || m_rerunQueued;
}

bool DeferredSaveManager::isSaving() const {
    return m_saving;
}

void DeferredSaveManager::trigger() {
    if (m_saving) {
        // Coalesce: everything requested while saving is written by ONE follow-up save.
        m_rerunQueued = true;
        return;
    }
    m_saving = true;
    emit saveStarted();
    if (m_saveFunction) {
        m_saveFunction();
    } else {
        saveFinished(false, QStringLiteral("No save function"));
    }
}

void DeferredSaveManager::saveFinished(bool ok, const QString &error) {
    if (!m_saving) {
        return;
    }
    m_saving = false;
    emit saveCompleted(ok, error);
    if (m_rerunQueued) {
        m_rerunQueued = false;
        trigger();
        return;
    }
    if (isIdle()) {
        emit idle();
    }
}

} // namespace EasePass::Core
