#include "QrCodeScannerTest.h"
#include "QrCodeScanner.h"
#include "QrCodeGenerator.h"
#include <QTest>
#include <QGuiApplication>
#include <QClipboard>
#include <QImage>

using namespace EasePass::Core;

namespace {
const QString EXPECTED_URI = "otpauth://totp/Example:alice@google.com?secret=JBSWY3DPEHPK3PXP&issuer=Example";
}

void QrCodeScannerTest::testDecodeSampleQrNormal() {
    QString path = QStringLiteral(EASEPASS_TEST_FIXTURES_DIR "/qr_valid.png");
    QVERIFY(QFile::exists(path));

    QString result = QrCodeScanner::scanImageFile(path);
    QCOMPARE(result, EXPECTED_URI);
}

void QrCodeScannerTest::testDecodeSampleQrRotated() {
    QString path = QStringLiteral(EASEPASS_TEST_FIXTURES_DIR "/qr_rotated.png");
    QVERIFY(QFile::exists(path));

    QString result = QrCodeScanner::scanImageFile(path);
    QCOMPARE(result, EXPECTED_URI);
}

void QrCodeScannerTest::testDecodeSampleQrInverted() {
    QString path = QStringLiteral(EASEPASS_TEST_FIXTURES_DIR "/qr_inverted.png");
    QVERIFY(QFile::exists(path));

    QString result = QrCodeScanner::scanImageFile(path);
    QCOMPARE(result, EXPECTED_URI);
}

void QrCodeScannerTest::testDecodeSampleQrNoisy() {
    QString path = QStringLiteral(EASEPASS_TEST_FIXTURES_DIR "/qr_noisy.png");
    QVERIFY(QFile::exists(path));

    QString result = QrCodeScanner::scanImageFile(path);
    QCOMPARE(result, EXPECTED_URI);
}

void QrCodeScannerTest::testNonQrImageReturnsEmpty() {
    QString path = QStringLiteral(EASEPASS_TEST_FIXTURES_DIR "/non_qr.png");
    QVERIFY(QFile::exists(path));

    QString result = QrCodeScanner::scanImageFile(path);
    QVERIFY(result.isEmpty());
}

void QrCodeScannerTest::testNullImageAndMissingFileReturnsEmpty() {
    QImage nullImg;
    QVERIFY(QrCodeScanner::scanImage(nullImg).isEmpty());

    QVERIFY(QrCodeScanner::scanImageFile("/nonexistent/file/path.png").isEmpty());
}

void QrCodeScannerTest::testClipboardOtpauthText() {
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard) {
        QSKIP("No clipboard available in this environment");
    }

    // Injected otpauth text
    QString injected = "otpauth://totp/Injected:test?secret=JBSWY3DPEHPK3PXP";
    clipboard->setText(injected);
    QCOMPARE(QrCodeScanner::scanClipboard(), injected);

    // Non-otpauth plain text returns empty
    clipboard->setText("regular text string");
    QVERIFY(QrCodeScanner::scanClipboard().isEmpty());

    clipboard->clear();
}

void QrCodeScannerTest::testClipboardImageScanning() {
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard) {
        QSKIP("No clipboard available in this environment");
    }

    QString path = QStringLiteral(EASEPASS_TEST_FIXTURES_DIR "/qr_valid.png");
    QImage img(path);
    QVERIFY(!img.isNull());

    clipboard->setImage(img);
    QString result = QrCodeScanner::scanClipboard();
    QCOMPARE(result, EXPECTED_URI);

    clipboard->clear();
}

// Export 2FA (T15): generated codes decode to the same text, incl. non-ASCII issuer names.
void QrCodeScannerTest::testGenerateRoundTrip() {
    const QStringList texts{EXPECTED_URI,
                            QStringLiteral("otpauth://totp/B%C3%A4ckerei:j%C3%BCrgen?secret=GEZDGNBVGY3TQOJQ&issuer=B%C3%A4ckerei"),
                            QStringLiteral("Grüße äöü"), QString(500, QLatin1Char('x'))};
    for (const QString &text : texts) {
        const QImage img = QrCodeGenerator::generate(text, 4);
        QVERIFY(!img.isNull());
        QCOMPARE(img.width(), img.height());
        QCOMPARE(QrCodeScanner::scanImage(img), text);
    }
}

void QrCodeScannerTest::testGenerateEmptyAndScale() {
    QVERIFY(QrCodeGenerator::generate(QString()).isNull());
    QCOMPARE(QrCodeGenerator::moduleCount(QString()), 0);
    QVERIFY(QrCodeGenerator::generate(QString(10000, QLatin1Char('x'))).isNull()); // too long
    const int modules = QrCodeGenerator::moduleCount(EXPECTED_URI);
    QVERIFY(modules >= 21);
    QCOMPARE((modules - 21) % 4, 0); // QR versions grow by 4 modules
    const QImage img = QrCodeGenerator::generate(EXPECTED_URI, 3);
    QCOMPARE(img.width(), (modules + 8) * 3); // 4-module quiet zone on each side
    QCOMPARE(img.pixelColor(0, 0), QColor(Qt::white));
    QCOMPARE(img.pixelColor(4 * 3, 4 * 3), QColor(Qt::black)); // finder pattern corner
}
