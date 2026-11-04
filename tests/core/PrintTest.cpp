#include "PrintTest.h"

#include "PrintDocumentBuilder.h"
#include "PrintService.h"

#include <QBuffer>
#include <QFile>
#include <QPdfWriter>
#include <QTemporaryDir>
#include <QTest>
#include <QTextDocument>

using namespace EasePass::Core;
using Options = PrintDocumentBuilder::Options;

namespace {
PasswordItem makeItem(int i) {
    PasswordItem it;
    it.displayName = QStringLiteral("Account %1").arg(i);
    it.username = QStringLiteral("user%1").arg(i);
    it.email = QStringLiteral("user%1@example.com").arg(i);
    it.website = QStringLiteral("https://site%1.example").arg(i);
    it.password = QStringLiteral("S3cret-%1!").arg(i);
    it.notes = QStringLiteral("note line 1\nnote line 2");
    return it;
}

QList<PasswordItem> makeItems(int n) {
    QList<PasswordItem> out;
    for (int i = 0; i < n; ++i)
        out.append(makeItem(i));
    return out;
}

QString plainText(const QList<PasswordItem> &items, const Options &o) {
    QTextDocument doc;
    doc.setHtml(PrintDocumentBuilder::buildHtml(items, o));
    return doc.toPlainText();
}

int pdfPageObjects(const QByteArray &pdf) {
    // Page objects are written uncompressed by QPdfWriter: "/Type /Page" but not "/Type /Pages".
    int n = 0;
    qsizetype pos = 0;
    while ((pos = pdf.indexOf("/Type /Page", pos)) >= 0) {
        pos += 11;
        if (pos < pdf.size() && pdf.at(pos) != 's')
            ++n;
    }
    return n;
}
} // namespace

void PrintTest::testHtmlContainsFieldsAndEscapes() {
    PasswordItem evil = makeItem(1);
    evil.displayName = QStringLiteral("<b>Bank & Co</b>");
    Options o;
    o.title = QStringLiteral("My DB");
    const QString html = PrintDocumentBuilder::buildHtml({evil}, o);
    QVERIFY(html.contains("&lt;b&gt;Bank &amp; Co&lt;/b&gt;"));
    QVERIFY(!html.contains("<b>Bank"));

    const QString text = plainText({makeItem(1)}, o);
    for (const QString &s : {QString("My DB"), QString("Account 1"), QString("user1"), QString("user1@example.com"),
                             QString("https://site1.example"), QString("note line 2")})
        QVERIFY2(text.contains(s), qPrintable(s));
}

void PrintTest::testPasswordsExcludedByDefault() {
    Options o;
    QVERIFY(!o.includePasswords);
    const QString text = plainText(makeItems(3), o);
    QVERIFY(!text.contains("S3cret"));
    QVERIFY(!text.contains("Password"));
    QVERIFY(text.contains("passwords not included"));
    QVERIFY(!PrintDocumentBuilder::plaintextWarning().isEmpty());
}

void PrintTest::testPasswordsIncludedWhenRequested() {
    Options o;
    o.includePasswords = true;
    const QString text = plainText(makeItems(3), o);
    QVERIFY(text.contains("S3cret-0!"));
    QVERIFY(text.contains("S3cret-2!"));
}

void PrintTest::testPdfHasPagesAndMetadata() {
    QByteArray bytes;
    QBuffer buf(&bytes);
    QVERIFY(buf.open(QIODevice::WriteOnly));
    Options o;
    o.title = QStringLiteral("Test DB");
    const int pages = PrintDocumentBuilder::writePdf(makeItems(3), o, &buf);
    buf.close();
    QCOMPARE(pages, 1);
    QVERIFY(bytes.startsWith("%PDF-"));
    QCOMPARE(pdfPageObjects(bytes), 1);
}

void PrintTest::testLongListPaginates() {
    QByteArray bytes;
    QBuffer buf(&bytes);
    QVERIFY(buf.open(QIODevice::WriteOnly));
    Options o;
    o.includePasswords = true;
    const int pages = PrintDocumentBuilder::writePdf(makeItems(200), o, &buf);
    buf.close();
    QVERIFY2(pages > 5, qPrintable(QString::number(pages)));
    QCOMPARE(pdfPageObjects(bytes), pages);

    // Landscape A4 needs a different number of pages than portrait.
    QByteArray b2;
    QBuffer buf2(&b2);
    QVERIFY(buf2.open(QIODevice::WriteOnly));
    o.orientation = QPageLayout::Landscape;
    const int landscape = PrintDocumentBuilder::writePdf(makeItems(200), o, &buf2);
    QVERIFY(landscape > pages);
}

void PrintTest::testEmptyListStillOnePage() {
    QByteArray bytes;
    QBuffer buf(&bytes);
    QVERIFY(buf.open(QIODevice::WriteOnly));
    QCOMPARE(PrintDocumentBuilder::writePdf({}, Options{}, &buf), 1);
    QCOMPARE(PrintDocumentBuilder::writePdf({}, Options{}, nullptr), 0);
    QCOMPARE(PrintDocumentBuilder::render({}, Options{}, nullptr), 0);
}

void PrintTest::testPrintServiceExportPdfFile() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("out.pdf");
    EasePass::App::PrintService service;
    QString err;
    QVERIFY(service.exportPdf(makeItems(5), Options{}, path, &err));
    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray bytes = f.readAll();
    QVERIFY(bytes.startsWith("%PDF-"));
    QVERIFY(pdfPageObjects(bytes) >= 1);
}

void PrintTest::testPrintServiceExportPdfBadPath() {
    QTemporaryDir dir;
    EasePass::App::PrintService service;
    QString err;
    QVERIFY(!service.exportPdf(makeItems(1), Options{}, dir.filePath("missing/dir/out.pdf"), &err));
    QVERIFY(!err.isEmpty());
}
