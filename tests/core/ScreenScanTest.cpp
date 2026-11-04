#include "ScreenScanTest.h"

#include "IScreenCapture.h"
#include "IWebcamScanner.h"
#include "PortalScreenCapture.h"
#include "QrCodeScanner.h"
#include "ScreenGeometry.h"
#include "ScreenQrScanner.h"

#include <QPainter>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>

using namespace EasePass::Core;
using namespace EasePass::Core::ScreenGeometry;

Q_DECLARE_METATYPE(EasePass::Core::ScreenQrScanner::Outcome)

namespace {
const QString kQrText = QStringLiteral("otpauth://totp/Example:alice@google.com?secret=JBSWY3DPEHPK3PXP&issuer=Example");

QImage fixture(const char *name) {
    QImage img(QStringLiteral(EASEPASS_TEST_FIXTURES_DIR "/") + QLatin1String(name));
    Q_ASSERT(!img.isNull());
    return img;
}

class FakeScreenCapture : public IScreenCapture {
public:
    ScreenCaptureResult result;
    bool available = true;
    int calls = 0;
    QString name() const override { return QStringLiteral("fake"); }
    bool isAvailable() const override { return available; }
    void capture(Callback cb) override {
        ++calls;
        auto r = result;
        QTimer::singleShot(0, [cb, r]() { cb(r); });
    }
};

ScreenQrScanner::Outcome runScanner(ScreenQrScanner &s, const std::function<void()> &action) {
    QSignalSpy spy(&s, &ScreenQrScanner::finished);
    action();
    if (!spy.wait(5000))
        return {};
    return spy.first().first().value<ScreenQrScanner::Outcome>();
}
} // namespace

void ScreenScanTest::testLogicalToDeviceDprScales() {
    const QRect r(10, 20, 100, 50);
    QCOMPARE(logicalToDevice(r, 1.0), QRect(10, 20, 100, 50));
    QCOMPARE(logicalToDevice(r, 2.0), QRect(20, 40, 200, 100));
    QCOMPARE(logicalToDevice(r, 1.5), QRect(15, 30, 150, 75));
    // Odd values round outwards: x=3 -> 4.5 -> 4; right 3+5=8 -> 12 (exact)
    QCOMPARE(logicalToDevice(QRect(3, 3, 5, 5), 1.5), QRect(QPoint(4, 4), QPoint(11, 11)));
    QCOMPARE(logicalToDevice(QRect(-101, 0, 1, 1), 1.5), QRect(QPoint(-152, 0), QPoint(-151, 1)));
    QVERIFY(logicalToDevice(QRect(), 2.0).isNull());
}

void ScreenScanTest::testUnionGeometryNegativeOrigin() {
    // Left monitor 1920x1080 @1.0 at negative x, right 2560x1440 logical @1.5.
    const QList<ScreenInfo> screens{{QRect(-1920, 200, 1920, 1080), 1.0}, {QRect(0, 0, 2560, 1440), 1.5}};
    QCOMPARE(unionGeometry(screens), QRect(-1920, 0, 4480, 1440));
    QCOMPARE(maxDevicePixelRatio(screens), 1.5);
    QVERIFY(unionGeometry({}).isNull());
    QCOMPARE(maxDevicePixelRatio({}), 1.0);
}

void ScreenScanTest::testMapSelectionToImage_data() {
    QTest::addColumn<QRect>("selection");
    QTest::addColumn<QRect>("capture");
    QTest::addColumn<QSize>("imageSize");
    QTest::addColumn<QRect>("expected");

    QTest::newRow("dpr1") << QRect(100, 100, 50, 50) << QRect(0, 0, 1920, 1080) << QSize(1920, 1080)
                          << QRect(100, 100, 50, 50);
    QTest::newRow("dpr2") << QRect(100, 100, 50, 50) << QRect(0, 0, 1920, 1080) << QSize(3840, 2160)
                          << QRect(200, 200, 100, 100);
    QTest::newRow("dpr1.5") << QRect(100, 100, 50, 50) << QRect(0, 0, 1920, 1080) << QSize(2880, 1620)
                            << QRect(150, 150, 75, 75);
    QTest::newRow("negative origin dpr1") << QRect(-1900, 220, 100, 100) << QRect(-1920, 0, 4480, 1440)
                                          << QSize(4480, 1440) << QRect(20, 220, 100, 100);
    QTest::newRow("negative origin dpr1.5") << QRect(-1900, 220, 100, 100) << QRect(-1920, 0, 4480, 1440)
                                            << QSize(6720, 2160) << QRect(30, 330, 150, 150);
    QTest::newRow("negative y, dpr2") << QRect(10, -500, 20, 20) << QRect(0, -1080, 3840, 2160)
                                      << QSize(7680, 4320) << QRect(20, 1160, 40, 40);
}

void ScreenScanTest::testMapSelectionToImage() {
    QFETCH(QRect, selection);
    QFETCH(QRect, capture);
    QFETCH(QSize, imageSize);
    QFETCH(QRect, expected);
    QCOMPARE(mapSelectionToImage(selection, capture, imageSize), expected);
}

void ScreenScanTest::testMapSelectionClipsAndRejects() {
    const QRect cap(0, 0, 100, 100);
    QCOMPARE(mapSelectionToImage(QRect(90, 90, 50, 50), cap, QSize(200, 200)), QRect(180, 180, 20, 20));
    QVERIFY(mapSelectionToImage(QRect(200, 200, 10, 10), cap, QSize(200, 200)).isNull());
    QVERIFY(mapSelectionToImage(QRect(), cap, QSize(200, 200)).isNull());
    QVERIFY(mapSelectionToImage(QRect(0, 0, 10, 10), cap, QSize()).isNull());
}

void ScreenScanTest::testComposeScreensMixedDpr() {
    QImage left(100, 50, QImage::Format_RGB32);
    left.fill(Qt::red);
    QImage right(300, 150, QImage::Format_RGB32); // 200x100 logical @1.5
    right.fill(Qt::blue);
    const QImage out = composeScreens({{{QRect(-100, 0, 100, 50), 1.0}, left}, {{QRect(0, 0, 200, 100), 1.5}, right}});
    QCOMPARE(out.size(), QSize(450, 150));
    QCOMPARE(out.devicePixelRatio(), 1.5);
    QCOMPARE(QColor(out.pixel(10, 10)), QColor(Qt::red));
    QCOMPARE(QColor(out.pixel(10, 100)), QColor(Qt::black)); // gap below the smaller left screen
    QCOMPARE(QColor(out.pixel(300, 100)), QColor(Qt::blue));
    QVERIFY(composeScreens({}).isNull());
}

void ScreenScanTest::testChooseBackend() {
    using K = ScreenCaptureBackendKind;
    QCOMPARE(chooseScreenCaptureBackend("wayland", true), K::Portal);
    QCOMPARE(chooseScreenCaptureBackend("wayland-egl", true), K::Portal);
    QCOMPARE(chooseScreenCaptureBackend("wayland", false), K::None);
    QCOMPARE(chooseScreenCaptureBackend("xcb", true), K::QtScreen);
    QCOMPARE(chooseScreenCaptureBackend("windows", false), K::QtScreen);
    QCOMPARE(chooseScreenCaptureBackend("cocoa", false), K::QtScreen);
    QCOMPARE(chooseScreenCaptureBackend("offscreen", true), K::None);
}

void ScreenScanTest::testPortalRequestPath() {
    QCOMPARE(PortalScreenshot::requestObjectPath(":1.42", "easepass123"),
             QString("/org/freedesktop/portal/desktop/request/1_42/easepass123"));
    QCOMPARE(PortalScreenshot::requestObjectPath(":1.2.3", "t"), QString("/org/freedesktop/portal/desktop/request/1_2_3/t"));
}

void ScreenScanTest::testPortalResponseParsing() {
    using S = PortalScreenshot::Response::Status;
    auto r = PortalScreenshot::parseResponse(0, {{"uri", "file:///tmp/Screenshot%20x.png"}});
    QCOMPARE(r.status, S::Success);
    QCOMPARE(r.localFile, QString("/tmp/Screenshot x.png"));
    QCOMPARE(PortalScreenshot::parseResponse(1, {}).status, S::Cancelled);
    r = PortalScreenshot::parseResponse(2, {});
    QCOMPARE(r.status, S::Failed);
    QVERIFY(!r.errorString.isEmpty());
    QCOMPARE(PortalScreenshot::parseResponse(0, {}).status, S::Failed);
    QCOMPARE(PortalScreenshot::parseResponse(0, {{"uri", "https://evil.example/x.png"}}).status, S::Failed);
}

void ScreenScanTest::testScanDecision() {
    using K = QrScanDecision::Kind;
    QCOMPARE(QrScanDecision::decide({}).kind, K::NoneFound);
    auto d = QrScanDecision::decide({"a"});
    QCOMPARE(d.kind, K::Accept);
    QCOMPARE(d.text, QString("a"));
    d = QrScanDecision::decide({"a", "a", ""});
    QCOMPARE(d.kind, K::Accept);
    d = QrScanDecision::decide({"a", "b"});
    QCOMPARE(d.kind, K::UserSelect);
    QCOMPARE(d.candidates, (QStringList{"a", "b"}));
}

void ScreenScanTest::testScanImageAllAsync() {
    QObject ctx;
    QStringList got;
    bool done = false;
    const Qt::HANDLE mainThread = QThread::currentThreadId();
    Qt::HANDLE cbThread = nullptr;
    QrCodeScanner::scanImageAllAsync(fixture("qr_valid.png"), &ctx, [&](const QStringList &found) {
        got = found;
        cbThread = QThread::currentThreadId();
        done = true;
    });
    QVERIFY(!done); // never synchronous
    QTRY_VERIFY_WITH_TIMEOUT(done, 5000);
    QCOMPARE(got, QStringList{kQrText});
    QCOMPARE(cbThread, mainThread);
    QVERIFY(QrCodeScanner::scanImageAll(fixture("non_qr.png")).isEmpty());
}

void ScreenScanTest::testScreenQrScannerFindsCode() {
    auto cap = std::make_shared<FakeScreenCapture>();
    // Desktop-like image with the QR somewhere in it.
    QImage desktop(1600, 900, QImage::Format_RGB32);
    desktop.fill(Qt::white);
    QPainter(&desktop).drawImage(700, 300, fixture("qr_valid.png"));
    cap->result.image = desktop;
    cap->result.logicalGeometry = QRect(0, 0, 1600, 900);

    ScreenQrScanner scanner(cap);
    auto o = runScanner(scanner, [&] { scanner.start(); });
    QCOMPARE(o.status, ScreenQrScanner::Status::Found);
    QCOMPARE(o.text, kQrText);
    QCOMPARE(cap->calls, 1);
    QVERIFY(!scanner.isBusy());
    scanner.clear();
    QVERIFY(scanner.lastCapture().image.isNull());
}

void ScreenScanTest::testScreenQrScannerNoCode() {
    auto cap = std::make_shared<FakeScreenCapture>();
    cap->result.image = fixture("non_qr.png");
    cap->result.logicalGeometry = QRect(QPoint(0, 0), cap->result.image.size());
    ScreenQrScanner scanner(cap);
    QCOMPARE(runScanner(scanner, [&] { scanner.start(); }).status, ScreenQrScanner::Status::NoneFound);
}

void ScreenScanTest::testScreenQrScannerRegionOnScaledMultiMonitor() {
    // Capture at scale 2 of a desktop spanning x = -1000..1000 logical.
    const QImage qr = fixture("qr_valid.png");
    QImage img(4000, 1600, QImage::Format_RGB32);
    img.fill(Qt::white);
    const QPoint qrDevice(400, 200); // device pixels -> logical (-800, 100)
    QPainter(&img).drawImage(qrDevice, qr);
    auto cap = std::make_shared<FakeScreenCapture>();
    cap->result.image = img;
    cap->result.logicalGeometry = QRect(-1000, 0, 2000, 800);

    ScreenQrScanner scanner(cap);
    runScanner(scanner, [&] { scanner.start(); });

    const QRect qrLogical(-800 - 10, 100 - 10, qr.width() / 2 + 20, qr.height() / 2 + 20);
    auto o = runScanner(scanner, [&] { scanner.scanRegion(qrLogical); });
    QCOMPARE(o.status, ScreenQrScanner::Status::Found);
    QCOMPARE(o.text, kQrText);

    // A region next to it has no code.
    o = runScanner(scanner, [&] { scanner.scanRegion(QRect(500, 100, 300, 300)); });
    QCOMPARE(o.status, ScreenQrScanner::Status::NoneFound);
    // Outside the capture.
    o = runScanner(scanner, [&] { scanner.scanRegion(QRect(5000, 0, 10, 10)); });
    QCOMPARE(o.status, ScreenQrScanner::Status::NoneFound);
}

void ScreenScanTest::testScreenQrScannerCaptureFailureAndCancel() {
    auto cap = std::make_shared<FakeScreenCapture>();
    cap->result.errorString = "portal broke";
    ScreenQrScanner scanner(cap);
    auto o = runScanner(scanner, [&] { scanner.start(); });
    QCOMPARE(o.status, ScreenQrScanner::Status::Failed);
    QCOMPARE(o.errorString, QString("portal broke"));

    cap->result = {};
    cap->result.cancelled = true;
    QCOMPARE(runScanner(scanner, [&] { scanner.start(); }).status, ScreenQrScanner::Status::Cancelled);
    QVERIFY(!scanner.isBusy());
}

void ScreenScanTest::testScreenQrScannerUnavailableBackend() {
    ScreenQrScanner none(nullptr);
    auto o = runScanner(none, [&] { none.start(); });
    QCOMPARE(o.status, ScreenQrScanner::Status::Failed);
    QVERIFY(!o.errorString.isEmpty());

    auto cap = std::make_shared<FakeScreenCapture>();
    cap->available = false;
    ScreenQrScanner scanner(cap);
    QCOMPARE(runScanner(scanner, [&] { scanner.start(); }).status, ScreenQrScanner::Status::Failed);
    QCOMPARE(cap->calls, 0);
}

void ScreenScanTest::testWebcamControllerDetectsAndThrottles() {
    qint64 nowMs = 1000;
    WebcamQrScanController ctrl([&] { return nowMs; });
    ctrl.setMinIntervalMs(200);
    QSignalSpy detected(&ctrl, &WebcamQrScanController::codeDetected);
    QSignalSpy finished(&ctrl, &WebcamQrScanController::scanFinished);

    QVERIFY(!ctrl.submitFrame(QImage()));
    QVERIFY(ctrl.submitFrame(fixture("non_qr.png")));
    QVERIFY(!ctrl.submitFrame(fixture("qr_valid.png"))); // one in flight
    QVERIFY(finished.wait(5000));
    QCOMPARE(detected.count(), 0);

    nowMs += 100;
    QVERIFY(!ctrl.submitFrame(fixture("qr_valid.png"))); // throttled
    nowMs += 100;
    QVERIFY(ctrl.submitFrame(fixture("qr_valid.png")));
    QVERIFY(detected.wait(5000));
    QCOMPARE(detected.first().first().toString(), kQrText);
    QVERIFY(ctrl.hasDetected());

    nowMs += 1000;
    QVERIFY(!ctrl.submitFrame(fixture("qr_valid.png"))); // done until reset
    ctrl.reset();
    QVERIFY(ctrl.submitFrame(fixture("qr_valid.png")));
    QTRY_COMPARE_WITH_TIMEOUT(detected.count(), 2, 5000);
    QCOMPARE(ctrl.framesScanned(), 3);
}
