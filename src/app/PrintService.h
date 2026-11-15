#pragma once

#include <QObject>

#include "PrintDocumentBuilder.h"

class QWidget;

namespace EasePass::App {

/**
 * Printing and PDF export with Qt PrintSupport; the print and preview dialogs are the
 * system's (Qt Widgets). The caller (PrintViewModel and its QML dialog) shows
 * PrintDocumentBuilder::plaintextWarning() and asks whether to include passwords.
 */
class PrintService : public QObject {
    Q_OBJECT
public:
    using Options = Core::PrintDocumentBuilder::Options;
    explicit PrintService(QObject *parent = nullptr);

    /// QPrintDialog, then prints. Returns false if cancelled or failed.
    bool print(const QList<Core::PasswordItem> &items, const Options &options, QWidget *parent);
    /// QPrintPreviewDialog (can print from there).
    void preview(const QList<Core::PasswordItem> &items, const Options &options, QWidget *parent);
    /// Writes a PDF file (no dialog). Returns false on I/O error; @p errorString set then.
    bool exportPdf(const QList<Core::PasswordItem> &items, const Options &options, const QString &filePath,
                   QString *errorString = nullptr);
};

} // namespace EasePass::App
