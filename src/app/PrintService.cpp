#include "PrintService.h"

#include <QCheckBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QPrintDialog>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QSaveFile>
#include <QStandardPaths>

namespace EasePass::App {

PrintService::PrintService(QObject *parent) : QObject(parent) {}

bool PrintService::askOptions(Options &options, QWidget *parent) {
    QMessageBox box(QMessageBox::Warning, tr("Print passwords"), Core::PrintDocumentBuilder::plaintextWarning(),
                    QMessageBox::Ok | QMessageBox::Cancel, parent);
    auto *check = new QCheckBox(tr("Include passwords"), &box);
    check->setChecked(options.includePasswords);
    box.setCheckBox(check);
    if (box.exec() != QMessageBox::Ok)
        return false;
    options.includePasswords = check->isChecked();
    return true;
}

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
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorString)
            *errorString = file.errorString();
        return false;
    }
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

bool PrintService::exportPdfInteractive(const QList<Core::PasswordItem> &items, const Options &options,
                                        QWidget *parent) {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString name = options.title.isEmpty() ? QStringLiteral("EasePass") : options.title;
    QString path = QFileDialog::getSaveFileName(parent, tr("Export PDF"), dir + QLatin1Char('/') + name + ".pdf",
                                                tr("PDF files (*.pdf)"));
    if (path.isEmpty())
        return false;
    if (!path.endsWith(QLatin1String(".pdf"), Qt::CaseInsensitive))
        path += QLatin1String(".pdf");
    QString err;
    if (!exportPdf(items, options, path, &err)) {
        QMessageBox::warning(parent, tr("Export PDF"), err);
        return false;
    }
    return true;
}

} // namespace EasePass::App
