#include "PasswordActions.h"

#include "ClipboardService.h"
#include "DatabaseController.h"
#include "DatabaseOperations.h"
#include "DatabaseService.h"
#include "TotpHelper.h"
#include "TotpTimerService.h"

#include <algorithm>

namespace EasePass::App {

using namespace EasePass::Core;

PasswordActions::PasswordActions(DatabaseService *service, DatabaseOperations *operations,
                                 ClipboardService *clipboard, TotpTimerService *totpTimer, QObject *parent)
    : QObject(parent), m_service(service), m_operations(operations), m_clipboard(clipboard), m_totpTimer(totpTimer) {
    connect(m_service, &DatabaseService::readOnlyChanged, this, &PasswordActions::readOnlyChanged);
    connect(m_service, &DatabaseService::stateChanged, this, &PasswordActions::readOnlyChanged);
}

bool PasswordActions::readOnly() const { return m_service->isOpen() && m_service->isReadOnly(); }

bool PasswordActions::validIndex(int index) const {
    return m_service->isOpen() && index >= 0 && index < m_service->itemCount();
}

bool PasswordActions::copyPassword(int index) {
    if (!validIndex(index) || m_service->items().at(index).password.isEmpty())
        return false;
    m_clipboard->copy(m_service->items().at(index).password, true);
    return true;
}

bool PasswordActions::copyUsername(int index) {
    if (!validIndex(index) || m_service->items().at(index).username.isEmpty())
        return false;
    m_clipboard->copy(m_service->items().at(index).username, false);
    return true;
}

bool PasswordActions::copyEmail(int index) {
    if (!validIndex(index) || m_service->items().at(index).email.isEmpty())
        return false;
    m_clipboard->copy(m_service->items().at(index).email, false);
    return true;
}

QString PasswordActions::totpCode(int index) const {
    if (!validIndex(index))
        return {};
    const PasswordItem &item = m_service->items().at(index);
    if (!item.has2FA())
        return {};
    bool okDigits = false, okPeriod = false;
    const int digits = item.digits.toInt(&okDigits);
    const int period = item.interval.toInt(&okPeriod);
    if (!okDigits || !okPeriod || digits < 4 || digits > 10 || period < 1)
        return {};
    return TotpHelper::generateToken(item.secret.trimmed(), m_totpTimer->now(), digits, period, item.algorithm);
}

bool PasswordActions::copyTotp(int index) {
    const QString code = totpCode(index);
    if (code.isEmpty())
        return false;
    m_clipboard->copy(code, true);
    return true;
}

bool PasswordActions::registerClick(int index) {
    if (!validIndex(index) || readOnly())
        return false;
    return m_service->registerClick(index, m_totpTimer->clock());
}

bool PasswordActions::deleteItem(int index) { return validIndex(index) && m_service->deleteItem(index); }

int PasswordActions::deleteItems(const QVariantList &indices) {
    QList<int> list;
    for (const QVariant &v : indices)
        list << v.toInt();
    std::sort(list.begin(), list.end(), std::greater<int>());
    list.erase(std::unique(list.begin(), list.end()), list.end());
    int deleted = 0;
    for (int i : list)
        deleted += deleteItem(i) ? 1 : 0;
    return deleted;
}

bool PasswordActions::exportItems(const QVariantList &indices, const QString &pathOrUrl, const QString &newPassword) {
    if (m_busy || !m_service->isOpen())
        return false;
    QList<PasswordItem> items;
    for (const QVariant &v : indices) {
        const int i = v.toInt();
        if (validIndex(i))
            items << m_service->items().at(i);
    }
    if (items.isEmpty())
        return false;
    DatabaseService::ExportOptions options;
    options.items = items;
    if (!newPassword.isEmpty())
        options.newPassword = SecretString(newPassword);
    const QString path = DatabaseController::toLocalPath(pathOrUrl);
    setBusy(true);
    m_operations->exportDatabase(path, options, [this, path](const DatabaseOpResult &result) {
        setBusy(false);
        emit exportFinished(result.ok(), result.ok() ? QString() : result.message(), path);
    });
    return true;
}

void PasswordActions::setBusy(bool busy) {
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

} // namespace EasePass::App
