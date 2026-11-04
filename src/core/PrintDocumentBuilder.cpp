#include "PrintDocumentBuilder.h"

#include <QAbstractTextDocumentLayout>
#include <QCoreApplication>
#include <QLocale>
#include <QPagedPaintDevice>
#include <QPdfWriter>
#include <QTextDocument>

namespace EasePass::Core {

namespace {
QString tr(const char *s, int n = -1) {
    return QCoreApplication::translate("PrintDocumentBuilder", s, nullptr, n);
}

QString esc(const QString &s) {
    return s.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br/>"));
}

void row(QString &html, const QString &label, const QString &value, bool mono = false) {
    if (value.trimmed().isEmpty())
        return;
    html += QStringLiteral("<tr><td class='label' width='22%'>%1</td><td%2>%3</td></tr>")
                .arg(esc(label), mono ? QStringLiteral(" class='mono'") : QString(), esc(value));
}
} // namespace

QString PrintDocumentBuilder::plaintextWarning() {
    return tr("The printout or PDF contains your data in plain text. Anyone who gets hold of it can read it. "
              "Store it securely and destroy it when no longer needed.");
}

QString PrintDocumentBuilder::buildHtml(const QList<PasswordItem> &items, const Options &options) {
    const QString title = options.title.trimmed().isEmpty() ? QStringLiteral("Ease Pass") : options.title;
    QString html;
    html += QStringLiteral("<html><head><style>"
                           "h1 { font-size: 16pt; margin-bottom: 0; }"
                           ".meta { color: #555; font-size: 9pt; }"
                           "h2 { font-size: 12pt; margin-top: 14pt; margin-bottom: 2pt; }"
                           "table { border-collapse: collapse; }"
                           "td { padding: 1pt 6pt 1pt 0; vertical-align: top; font-size: 10pt; }"
                           "td.label { color: #555; }"
                           ".mono { font-family: monospace; }"
                           "</style></head><body>");
    html += QStringLiteral("<h1>%1</h1>").arg(esc(title));
    QString meta = tr("%n entries", int(items.size()));
    if (options.printedAt.isValid())
        meta += QStringLiteral(" · ") + QLocale().toString(options.printedAt, QLocale::ShortFormat);
    if (!options.includePasswords)
        meta += QStringLiteral(" · ") + tr("passwords not included");
    html += QStringLiteral("<p class='meta'>%1</p>").arg(esc(meta));

    for (const PasswordItem &item : items) {
        const QString name = item.displayName.trimmed().isEmpty() ? tr("(no name)") : item.displayName;
        html += QStringLiteral("<h2>%1</h2><table cellspacing='0' cellpadding='2' width='100%'>").arg(esc(name));
        row(html, tr("Username"), item.username);
        row(html, tr("Email"), item.email);
        row(html, tr("Website"), item.website);
        if (options.includePasswords)
            row(html, tr("Password"), item.password, true);
        row(html, tr("Notes"), item.notes);
        html += QStringLiteral("</table>");
    }
    html += QStringLiteral("</body></html>");
    return html;
}

std::unique_ptr<QTextDocument> PrintDocumentBuilder::buildDocument(const QList<PasswordItem> &items,
                                                                   const Options &options, QPagedPaintDevice *device) {
    auto doc = std::make_unique<QTextDocument>();
    doc->setHtml(buildHtml(items, options));
    if (device) {
        // Size the document in device units so QTextDocument::print() does not rescale it.
        const QRectF paint = device->pageLayout().paintRectPixels(device->logicalDpiX());
        doc->documentLayout()->setPaintDevice(device);
        doc->setPageSize(paint.size());
    }
    return doc;
}

int PrintDocumentBuilder::render(const QList<PasswordItem> &items, const Options &options, QPagedPaintDevice *device) {
    if (!device)
        return 0;
    auto doc = buildDocument(items, options, device);
    const int pages = doc->pageCount();
    doc->print(device);
    return pages;
}

int PrintDocumentBuilder::writePdf(const QList<PasswordItem> &items, const Options &options, QIODevice *out) {
    if (!out || !out->isWritable())
        return 0;
    QPdfWriter writer(out);
    writer.setResolution(300);
    writer.setPageSize(options.pageSize);
    writer.setPageOrientation(options.orientation);
    writer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);
    writer.setTitle(options.title.isEmpty() ? QStringLiteral("Ease Pass") : options.title);
    writer.setCreator(QStringLiteral("Ease Pass"));
    return render(items, options, &writer);
}

} // namespace EasePass::Core
