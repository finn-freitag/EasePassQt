#include "ClipboardService.h"

#include "ClipboardManager.h"

namespace EasePass::App {

ClipboardService::ClipboardService(Core::ClipboardManager *manager, QObject *parent)
    : QObject(parent), m_manager(manager) {
    connect(m_manager, &Core::ClipboardManager::copied, this, [this] {
        emit copied();
        emit stateChanged();
    });
    connect(m_manager, &Core::ClipboardManager::cleared, this, &ClipboardService::stateChanged);
    connect(m_manager, &Core::ClipboardManager::clearSkipped, this, &ClipboardService::stateChanged);
}

bool ClipboardService::clearPending() const { return m_manager->isClearPending(); }

int ClipboardService::secondsUntilClear() const { return m_manager->secondsUntilClear(); }

void ClipboardService::copy(const QString &text, bool secret) {
    if (text.isEmpty())
        return;
    m_manager->copy(text, secret);
}

void ClipboardService::clearNow() {
    m_manager->clearNow();
    emit stateChanged();
}

} // namespace EasePass::App
