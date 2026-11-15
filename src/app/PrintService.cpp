#include "PrintService.h"

#include <QPrintDialog>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QFile>
#include <QSaveFile>

#include "FilePermissions.h"

namespace EasePass::App {

PrintService::PrintService(QObject *parent) : QObject(parent) {}

bool PrintService::print(const QList<Core::PasswordItem> &items, const Options &options, QWidget *parent) {
    QPrinter printer(QPrinter::HighResolution);
    printer.setDocName(options.title.isEmpty() ? QStringLiteral("Ease Pass") : options.title);
    printer.setPageSize(options.pageSize);
    printer.setPageOrientation(options.orientation);
    QPrintDialog dialog(&printer, parent);
    dialog.setWindowTitle(tr("Print"));
    if (dialog.exec() != QDialog::Accepted)
        return false;
    return Core::PrintDocumentBuilder::render(items, options, &printer) > 0;
}

void PrintService::preview(const QList<Core::PasswordItem> &items, const Options &options, QWidget *parent) {
    QPrinter printer(QPrinter::HighResolution);
    printer.setPageSize(options.pageSize);
    printer.setPageOrientation(options.orientation);
    QPrintPreviewDialog dialog(&printer, parent);
    connect(&dialog, &QPrintPreviewDialog::paintRequested, &dialog, [&](QPrinter *p) {
        Core::PrintDocumentBuilder::render(items, options, p);
    });
    dialog.exec();
}

bool PrintService::exportPdf(const QList<Core::PasswordItem> &items, const Options &options, const QString &filePath,
                             QString *errorString) {
    QSaveFile file(filePath);
    const bool isNewFile = !QFile::exists(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorString)
            *errorString = file.errorString();
        return false;
    }
    // T23: the PDF may contain plaintext passwords; a new file is owner-only.
    if (isNewFile)
        Core::restrictToOwner(file);
    if (Core::PrintDocumentBuilder::writePdf(items, options, &file) <= 0) {
        file.cancelWriting();
        if (errorString)
            *errorString = tr("Creating the PDF failed.");
        return false;
    }
    if (!file.commit()) {
        if (errorString)
            *errorString = file.errorString();
        return false;
    }
    return true;
}

} // namespace EasePass::App
