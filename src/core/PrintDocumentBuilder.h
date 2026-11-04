#pragma once

#include <QDateTime>
#include <QList>
#include <QPageLayout>
#include <QPageSize>
#include <QString>
#include <memory>

#include "PasswordItem.h"

class QIODevice;
class QPagedPaintDevice;
class QTextDocument;

namespace EasePass::Core {

/**
 * Lays out password items for printing / PDF export (T10, C#: EasePass/Helper/PrinterHelper.cs).
 * Each item is printed as a block with name, username, email, website, password and notes.
 * Passwords are only included when explicitly requested (plaintext on paper!). Qt Gui only;
 * the dialogs live in EasePass::App::PrintService.
 */
class PrintDocumentBuilder {
public:
    struct Options {
        bool includePasswords = false;
        QString title;            ///< e.g. database name; empty -> "Ease Pass"
        QDateTime printedAt;      ///< shown in the header; invalid -> no date
        QPageSize pageSize = QPageSize(QPageSize::A4);
        QPageLayout::Orientation orientation = QPageLayout::Portrait;
    };

    /// Translated warning the UI must show before printing/exporting with passwords.
    static QString plaintextWarning();

    /// HTML for the item list (all values escaped).
    static QString buildHtml(const QList<PasswordItem> &items, const Options &options);

    /// QTextDocument with the HTML, laid out for @p device's page size.
    static std::unique_ptr<QTextDocument> buildDocument(const QList<PasswordItem> &items, const Options &options,
                                                        QPagedPaintDevice *device);

    /// Renders into @p device (QPdfWriter, QPrinter). Returns the number of pages, 0 on failure.
    static int render(const QList<PasswordItem> &items, const Options &options, QPagedPaintDevice *device);

    /// Writes a PDF to @p out. Returns page count, 0 on failure.
    static int writePdf(const QList<PasswordItem> &items, const Options &options, QIODevice *out);
};

} // namespace EasePass::Core
