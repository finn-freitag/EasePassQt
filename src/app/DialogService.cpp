#include "DialogService.h"

namespace EasePass::App {

int DialogService::ask(const QString &title, const QString &message, const QString &primaryText,
                       const QString &secondaryText, const QString &closeText, Callback callback) {
    const int id = m_nextId++;
    m_pending.insert(id, std::move(callback));
    emit pendingCountChanged();
    emit dialogRequested(id, title, message, primaryText, secondaryText, closeText);
    return id;
}

int DialogService::confirm(const QString &title, const QString &message, const QString &primaryText,
                           Callback callback) {
    return ask(title, message, primaryText, QString(), tr("Cancel"), std::move(callback));
}

bool DialogService::respond(int id, Result result) {
    auto it = m_pending.find(id);
    if (it == m_pending.end())
        return false;
    Callback cb = std::move(it.value());
    m_pending.erase(it);
    emit pendingCountChanged();
    emit dialogClosed(id, result);
    if (cb)
        cb(result);
    return true;
}

void DialogService::cancelAll() {
    const QList<int> ids = m_pending.keys();
    for (int id : ids)
        respond(id, Result::None);
}

void DialogService::showInfo(Severity severity, const QString &title, const QString &message) {
    emit infoRequested(severity, title, message);
}

} // namespace EasePass::App
