#include "PrintViewModel.h"

#include "DatabaseService.h"
#include "PrintDocumentBuilder.h"
#include "PrintService.h"

#include <QDateTime>

namespace EasePass::App {

namespace {
PrintService::Options makeOptions(const Core::DatabaseService *service, bool includePasswords) {
    PrintService::Options o;
    o.includePasswords = includePasswords;
    o.title = service->displayName();
    o.printedAt = QDateTime::currentDateTime();
    return o;
}
} // namespace

PrintViewModel::PrintViewModel(Core::DatabaseService *service, PrintService *printService, QObject *parent)
    : QObject(parent), m_service(service), m_print(printService) {
    connect(m_service, &Core::DatabaseService::stateChanged, this, &PrintViewModel::canPrintChanged);
}

QString PrintViewModel::plaintextWarning() { return Core::PrintDocumentBuilder::plaintextWarning(); }

bool PrintViewModel::canPrint() const { return m_service->isOpen(); }

bool PrintViewModel::exportPdf(const QString &target, bool includePasswords) {
    if (!canPrint()) {
        setError(tr("No database is open"));
        return false;
    }
    const QUrl url(target);
    const QString path = url.isLocalFile() ? url.toLocalFile() : target;
    QString error;
    const bool ok = m_print->exportPdf(m_service->items(), makeOptions(m_service, includePasswords), path, &error);
    setError(ok ? QString() : error);
    return ok;
}

bool PrintViewModel::print(bool includePasswords) {
    if (!canPrint()) {
        setError(tr("No database is open"));
        return false;
    }
    setError(QString());
    return m_print->print(m_service->items(), makeOptions(m_service, includePasswords), nullptr);
}

void PrintViewModel::preview(bool includePasswords) {
    if (!canPrint()) {
        setError(tr("No database is open"));
        return;
    }
    setError(QString());
    m_print->preview(m_service->items(), makeOptions(m_service, includePasswords), nullptr);
}

void PrintViewModel::setError(const QString &e) {
    if (e == m_lastError)
        return;
    m_lastError = e;
    emit lastErrorChanged();
}

} // namespace EasePass::App
