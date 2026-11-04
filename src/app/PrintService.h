#pragma once

#include <QObject>

#include "PrintDocumentBuilder.h"

class QWidget;

namespace EasePass::App {

/**
 * Printing and PDF export with Qt PrintSupport (T10, D7). Callable from Widgets now and from
 * QML later (T12). The caller is responsible for showing PrintDocumentBuilder::plaintextWarning()
 * and asking whether to include passwords (askOptions() does both with a Widgets dialog).
 */
class PrintService : public QObject {
    Q_OBJECT
public:
    using Options = Core::PrintDocumentBuilder::Options;
    explicit PrintService(QObject *parent = nullptr);

    /// Shows the plaintext warning with an "Include passwords" checkbox. False if cancelled.
    bool askOptions(Options &options, QWidget *parent);

    /// QPrintDialog, then prints. Returns false if cancelled or failed.
    bool print(const QList<Core::PasswordItem> &items, const Options &options, QWidget *parent);
    /// QPrintPreviewDialog (can print from there).
    void preview(const QList<Core::PasswordItem> &items, const Options &options, QWidget *parent);
    /// Writes a PDF file (no dialog). Returns false on I/O error; @p errorString set then.
    bool exportPdf(const QList<Core::PasswordItem> &items, const Options &options, const QString &filePath,
                   QString *errorString = nullptr);
    /// File dialog + exportPdf().
    bool exportPdfInteractive(const QList<Core::PasswordItem> &items, const Options &options, QWidget *parent);
};

} // namespace EasePass::App
