// T19: single instance, window geometry, log file (rotation + redaction), inactivity logout,
// activity filter, command line and startup database.
#include "AppTestSupport.h"
#include "AppTests.h"

#include "ActivityEventFilter.h"
#include "AppContext.h"
#include "AppSettings.h"
#include "AutoLogoutService.h"
#include "CommandLine.h"
#include "DatabaseController.h"
#include "DatabaseOperations.h"
#include "FileLogger.h"
#include "InactivityManager.h"
#include "ISettingsStore.h"
#include "ItemViewModel.h"
#include "SingleInstance.h"
#include "WindowStateController.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLocalSocket>
#include <QLoggingCategory>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QRegularExpression>
#include <QUuid>
#include <QWindow>
#include <atomic>
#include <thread>

using namespace EasePass::Core;
using namespace EasePass::App;
using namespace AppTest;

Q_LOGGING_CATEGORY(lcBehaviourTest, "easepass.test.behaviour")

namespace {
QString uniqueKey() { return QStringLiteral("easepass-test-") + QUuid::createUuid().toString(QUuid::Id128).left(12); }

QStringList logFiles(const QString &dir) {
    return QDir(dir).entryList({QStringLiteral("easepass_*.log")}, QDir::Files, QDir::Name);
}

QByteArray readAll(const QString &dir) {
    QByteArray all;
    for (const QString &f : logFiles(dir)) {
        QFile file(QDir(dir).filePath(f));
        if (file.open(QIODevice::ReadOnly))
            all += file.readAll();
    }
    return all;
}
} // namespace

// --- Single instance -----------------------------------------------------------------------

void AppBehaviourTest::testSingleInstanceTwoClients() {
    QTemporaryDir dir;
    const QString key = uniqueKey();
    SingleInstance primary(key, dir.path());
    QVERIFY(primary.tryBecomePrimary());
    QVERIFY(primary.isPrimary());

    SingleInstance second(key, dir.path());
    QVERIFY(!second.tryBecomePrimary()); // lock held
    QVERIFY(!second.isPrimary());

    // Two clients at the same time (raw sockets, asynchronous so the primary's loop runs).
    QSignalSpy spy(&primary, &SingleInstance::activationRequested);
    QLocalSocket a;
    QLocalSocket b;
    a.connectToServer(primary.serverName());
    b.connectToServer(primary.serverName());
    QVERIFY(a.waitForConnected(2000));
    QVERIFY(b.waitForConnected(2000));
    a.write(QJsonDocument(QJsonArray{"--db", "/tmp/a.epdb"}).toJson(QJsonDocument::Compact) + '\n');
    b.write(QJsonDocument(QJsonArray{"/tmp/b.epdb"}).toJson(QJsonDocument::Compact) + '\n');
    QTRY_COMPARE(spy.count(), 2);
    QStringList all;
    for (const auto &call : spy)
        all += call.at(0).toStringList();
    QVERIFY(all.contains("/tmp/a.epdb"));
    QVERIFY(all.contains("/tmp/b.epdb"));
    QTRY_VERIFY(a.canReadLine() && b.canReadLine());
    QCOMPARE(a.readLine(), QByteArray("ok\n"));
    QCOMPARE(b.readLine(), QByteArray("ok\n"));

    // The blocking sender used by a second process (run on another thread here).
    std::atomic<bool> sent{false};
    std::thread sender([&] { sent = second.sendToPrimary({"--db", "/tmp/c.epdb"}); });
    QTRY_COMPARE(spy.count(), 3);
    sender.join();
    QVERIFY(sent);
    QCOMPARE(spy.last().at(0).toStringList(), QStringList({"--db", "/tmp/c.epdb"}));
}

void AppBehaviourTest::testSingleInstanceHandover() {
    QTemporaryDir dir;
    const QString key = uniqueKey();
    {
        SingleInstance first(key, dir.path());
        QVERIFY(first.tryBecomePrimary());
    } // first instance quits
    SingleInstance next(key, dir.path());
    QVERIFY(next.tryBecomePrimary());
    // Nobody listens for another key.
    SingleInstance other(uniqueKey(), dir.path());
    QVERIFY(!other.sendToPrimary({}, 200));
}

// --- Window geometry -------------------------------------------------------------------------

void AppBehaviourTest::testFitToScreens() {
    const QList<QRect> screens{QRect(0, 0, 1920, 1080), QRect(1920, 0, 1280, 1024)};
    const QRect onFirst(100, 100, 800, 600);
    QCOMPARE(WindowStateController::fitToScreens(onFirst, screens), onFirst);
    const QRect onSecond(2000, 50, 800, 600);
    QCOMPARE(WindowStateController::fitToScreens(onSecond, screens), onSecond);
    const QRect partly(1850, 10, 800, 600); // title area crosses both screens
    QCOMPARE(WindowStateController::fitToScreens(partly, screens), partly);

    // Monitor unplugged: centered on the first (primary) screen.
    const QRect gone(4000, 100, 800, 600);
    const QRect moved = WindowStateController::fitToScreens(gone, screens);
    QCOMPARE(moved.size(), gone.size());
    QCOMPARE(moved.center(), screens.first().center());
    // Title bar above the screen: not reachable -> moved.
    QVERIFY(WindowStateController::fitToScreens(QRect(100, -500, 800, 600), screens) != QRect(100, -500, 800, 600));
    // Larger than the screen: shrunk to fit.
    const QRect huge = WindowStateController::fitToScreens(QRect(5000, 0, 4000, 3000), screens);
    QVERIFY(screens.first().contains(huge));
    QCOMPARE(WindowStateController::fitToScreens(gone, {}), gone);

    QCOMPARE(WindowStateController::toWindowState(0), Qt::WindowMaximized);
    QCOMPARE(WindowStateController::toWindowState(1), Qt::WindowNoState); // never start minimized
    QCOMPARE(WindowStateController::toWindowState(2), Qt::WindowNoState);
    QCOMPARE(WindowStateController::fromWindowState(Qt::WindowMaximized), 0);
    QCOMPARE(WindowStateController::fromWindowState(Qt::WindowMinimized | Qt::WindowMaximized), 1);
    QCOMPARE(WindowStateController::fromWindowState(Qt::WindowNoState), 2);
    QVERIFY(WindowStateController::isDefaultGeometry(QRect(0, 0, 1100, 700)));
}

void AppBehaviourTest::testWindowGeometryRoundTrip() {
    AppSettings settings(std::make_shared<InMemorySettingsStore>());
    settings.setWindowGeometry(QRect(10, 20, 500, 400));
    settings.setWindowState(WindowStateController::Restored);
    {
        QWindow window;
        WindowStateController controller(&settings);
        controller.attach(&window);
        QCOMPARE(window.geometry(), QRect(10, 20, 500, 400));
        QCOMPARE(window.windowStates(), Qt::WindowStates(Qt::WindowNoState));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.setGeometry(30, 40, 600, 450);
        QTRY_COMPARE(window.geometry(), QRect(30, 40, 600, 450));
        QTest::qWait(50); // let the platform report the new geometry
        window.setWindowStates(Qt::WindowMaximized);
        QTRY_VERIFY(window.windowStates() & Qt::WindowMaximized);
        controller.save();
    }
    // The normal geometry is kept, the state says maximized (C#: GetWindowSizePosStateIndependent).
    QCOMPARE(settings.windowGeometry(), QRect(30, 40, 600, 450));
    QCOMPARE(settings.windowState(), int(WindowStateController::Maximized));

    QWindow again;
    WindowStateController restore(&settings);
    restore.attach(&again);
    QCOMPARE(again.windowStates(), Qt::WindowStates(Qt::WindowMaximized));

    // Never saved: only the size is applied, the window manager picks the position.
    AppSettings fresh(std::make_shared<InMemorySettingsStore>());
    QWindow third;
    third.setPosition(5, 6);
    WindowStateController def(&fresh);
    def.attach(&third);
    QCOMPARE(third.size(), QSize(1100, 700));
    QCOMPARE(third.position(), QPoint(5, 6));
}

// --- Log file --------------------------------------------------------------------------------

void AppBehaviourTest::testFileLoggerRotation() {
    QTemporaryDir dir;
    // Old files from earlier sessions: two too old, five recent.
    const QDateTime now = QDateTime::currentDateTime();
    for (int i = 0; i < 7; ++i) {
        const QString path = dir.filePath(QStringLiteral("easepass_old%1.log").arg(i));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("old\n");
        f.setFileTime(i < 2 ? now.addDays(-40) : now.addDays(-i), QFileDevice::FileModificationTime);
    }
    QFile unrelated(dir.filePath("notes.txt"));
    QVERIFY(unrelated.open(QIODevice::WriteOnly));
    unrelated.close();

    FileLogger::Options o;
    o.directory = dir.path();
    o.maxFiles = 4;
    o.maxFileBytes = 2048;
    o.appVersion = "9.9.9";
    FileLogger logger(o);
    QVERIFY(logger.open());
    QCOMPARE(logger.removedFiles(), 4); // 2 too old + 2 over the limit (3 kept + the new one)
    QCOMPARE(logFiles(dir.path()).size(), 4);
    QVERIFY(QFile::exists(dir.filePath("notes.txt")));

    const QString first = logger.currentFile();
    QMessageLogContext ctx;
    for (int i = 0; i < 100; ++i)
        logger.write(QtInfoMsg, ctx, QStringLiteral("message number %1 with some padding text").arg(i));
    logger.flush();
    QVERIFY(logger.currentFile() != first); // continued in "..._2.log" etc.
    QVERIFY(QRegularExpression("_\\d+\\.log$").match(logger.currentFile()).hasMatch());
    QFile header(first);
    QVERIFY(header.open(QIODevice::ReadOnly));
    const QByteArray text = header.readAll();
    QVERIFY(text.startsWith("Ease Pass Version: 9.9.9"));
    QVERIFY(text.contains("Qt Version:"));
    QVERIFY(text.contains("[info] message number 0"));
    QVERIFY(QFileInfo(first).size() <= 2048 + 200);
}

void AppBehaviourTest::testFileLoggerRedaction() {
    QCOMPARE(FileLogger::redact("open otpauth://totp/Bank:me?secret=JBSWY3DPEHPK3PXP&issuer=Bank now"),
             QStringLiteral("open otpauth://[redacted] now"));
    QCOMPARE(FileLogger::redact("password=hunter2 user=bob"), QStringLiteral("password=[redacted] user=bob"));
    QCOMPARE(FileLogger::redact("Secret: \"a b c\""), QStringLiteral("Secret: [redacted]"));
    QCOMPARE(FileLogger::redact("token = 123456789012"), QStringLiteral("token = [redacted]"));
    QCOMPARE(FileLogger::redact("nothing to hide"), QStringLiteral("nothing to hide"));

    // A real session with the logger installed: the master password, item passwords and the
    // TOTP secret must never reach the log file.
    QTemporaryDir dir;
    FileLogger::Options o;
    o.directory = dir.path();
    FileLogger logger(o);
    QVERIFY(logger.open());
    logger.install();
    {
        AppContext::Deps deps;
        deps.settingsStore = std::make_shared<InMemorySettingsStore>();
        deps.clipboard = std::make_shared<FakeClipboard>();
        deps.networkClient = std::make_shared<FakeRangeClient>();
        deps.screenCapture = std::make_shared<FakeScreenCapture>();
        FakeClock clock;
        deps.clock = &clock;
        deps.kdf = kFastKdf;
        AppContext ctx(deps);
        PasswordItem item = makeItem("Bank", "Item-Passw0rd-XYZ");
        item.secret = "JBSWY3DPEHPK3PXP";
        QVERIFY(openItems(*ctx.databaseService(), {item}));
        ctx.itemViewModel()->load(0);
        ctx.itemViewModel()->copyPassword();
        DatabaseController *ctl = ctx.databaseController();
        ctl->open("/nonexistent/x.epdb", QString::fromLatin1(kPassword)); // error path
        qCWarning(lcBehaviourTest) << "careless code: password=" << kPassword; // redacted
        ctx.databaseService()->lock();
        ctx.databaseService()->waitForBackgroundWork();
    }
    logger.uninstall();
    logger.flush();
    const QByteArray log = readAll(dir.path());
    QVERIFY(log.contains("careless code"));
    QVERIFY(!log.contains(kPassword));
    QVERIFY(!log.contains("Item-Passw0rd-XYZ"));
    QVERIFY(!log.contains("JBSWY3DPEHPK3PXP"));
}

// --- Inactivity --------------------------------------------------------------------------------

void AppBehaviourTest::testAutoLogoutFlow() {
    FakeClock clock;
    AppSettings settings(std::make_shared<InMemorySettingsStore>());
    settings.setInactivityLogoutMinutes(2);
    DatabaseService svc(kFastKdf);
    InactivityManager inactivity(&settings, &clock);
    AutoLogoutService service(&svc, &inactivity);
    QVERIFY(!service.enabled());
    QVERIFY(openItems(svc, {makeItem("A")}));
    QVERIFY(service.enabled());

    QSignalSpy warning(&service, &AutoLogoutService::warningActiveChanged);
    QSignalSpy loggedOut(&service, &AutoLogoutService::loggedOut);
    clock.now += 80; // 40 s left, the warning starts 30 s before
    inactivity.tick();
    QVERIFY(!service.warningActive());
    clock.now += 30; // 10 s left
    inactivity.tick();
    QVERIFY(service.warningActive());
    QCOMPARE(service.secondsLeft(), 10);
    service.reportActivity();
    QVERIFY(!service.warningActive());
    clock.now += 119;
    inactivity.tick();
    QVERIFY(svc.isOpen());

    service.preventLogout(true);
    service.preventLogout(true); // nested
    clock.now += 500;
    inactivity.tick();
    QVERIFY(svc.isOpen());
    service.preventLogout(false);
    clock.now += 500;
    inactivity.tick();
    QVERIFY(svc.isOpen()); // still prevented once
    service.preventLogout(false);
    clock.now += 121;
    inactivity.tick();
    QVERIFY(!svc.isOpen());
    QCOMPARE(loggedOut.count(), 1);
    QVERIFY(!service.enabled());
    QVERIFY(warning.count() >= 2);
    svc.shutdown();
}

void AppBehaviourTest::testActivityEventFilter() {
    int count = 0;
    ActivityEventFilter filter([&] { ++count; });
    QObject target;
    target.installEventFilter(&filter);
    QKeyEvent key(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, "a");
    QCoreApplication::sendEvent(&target, &key); // not spontaneous (synthesized)
    QCOMPARE(count, 0);
    filter.setRequireSpontaneous(false);
    QCoreApplication::sendEvent(&target, &key);
    QCOMPARE(count, 1);
    QEvent timer(QEvent::Timer);
    QCoreApplication::sendEvent(&target, &timer);
    QEvent paint(QEvent::UpdateRequest);
    QCoreApplication::sendEvent(&target, &paint);
    QCOMPARE(count, 1);
    QVERIFY(ActivityEventFilter::isUserInput(&key, false));
    QVERIFY(!ActivityEventFilter::isUserInput(&timer, false));
}

// --- Command line / startup database ---------------------------------------------------------

void AppBehaviourTest::testCommandLine() {
    QTemporaryDir dir;
    const QString file = dir.filePath("My DB.epdb");
    CommandLineOptions o = parseCommandLine({"easepass", "--db", file});
    QVERIFY(o.error.isEmpty());
    QCOMPARE(o.databasePath, file);
    o = parseCommandLine({"easepass", "-d", file});
    QCOMPARE(o.databasePath, file);
    o = parseCommandLine({"easepass", QUrl::fromLocalFile(file).toString()}); // file association
    QCOMPARE(o.databasePath, file);
    o = parseCommandLine({"easepass", "relative.epdb"});
    QCOMPARE(o.databasePath, QDir::current().absoluteFilePath("relative.epdb"));
    o = parseCommandLine({"easepass"});
    QVERIFY(o.databasePath.isEmpty());
    QVERIFY(o.error.isEmpty());
    QVERIFY(!parseCommandLine({"easepass", "--bogus"}).error.isEmpty());
    QVERIFY(!parseCommandLine({"easepass", "a.epdb", "b.epdb"}).error.isEmpty());
    QVERIFY(!parseCommandLine({"easepass", "--db", "a.epdb", "b.epdb"}).error.isEmpty());
    QVERIFY(parseCommandLine({"easepass", "--help"}).helpRequested);
    QVERIFY(parseCommandLine({"easepass", "--version"}).versionRequested);
    QVERIFY(commandLineHelp().contains("--db"));
    QVERIFY2(commandLineHelp().startsWith("Usage: easepass [options] [database]"), qPrintable(commandLineHelp()));
}

void AppBehaviourTest::testStartupDatabase() {
    QTemporaryDir dir;
    auto write = [&](const QString &name) {
        const QString path = dir.filePath(name);
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly) || f.write(makeDbBytes({makeItem("X")})) < 0)
            qFatal("cannot write test database");
        return path;
    };
    const QString known = write("Known.epdb");
    const QString other = write("Other.epdb");
    auto settings = std::make_shared<AppSettings>(std::make_shared<InMemorySettingsStore>());
    settings->setDatabasePaths({known});
    DatabaseService svc(kFastKdf);
    DatabaseOperations ops(settings, &svc, kFastKdf);
    DatabaseController ctl(&svc, &ops, settings);

    QVERIFY(!ctl.setStartupDatabase(dir.filePath("missing.epdb")));
    QVERIFY(ctl.setStartupDatabase(known));
    QCOMPARE(ctl.lastDatabase(), known);
    QVERIFY(ctl.temporaryPath().isEmpty());

    QSignalSpy changed(&ctl, &DatabaseController::knownDatabasesChanged);
    QVERIFY(ctl.setStartupDatabase(QUrl::fromLocalFile(other).toString()));
    QVERIFY(changed.count() > 0);
    QCOMPARE(ctl.knownDatabases(), QStringList({known, other}));
    QCOMPARE(ctl.lastDatabase(), other);
    QVERIFY(ctl.isTemporaryPath(other));
    QVERIFY(!ctl.isTemporaryPath(known));
    QCOMPARE(settings->databasePaths(), QStringList{known}); // not remembered

    QSignalSpy opened(&svc, &DatabaseService::openFinished);
    QVERIFY(ctl.open(other, QString::fromLatin1(kPassword)));
    QVERIFY(opened.wait());
    QVERIFY(svc.isTemporary());
    QVERIFY(svc.isReadOnly());
    QCOMPARE(settings->lastDatabase(), known); // temporary databases do not become the last database
    QVERIFY(ctl.makePermanent());
    QVERIFY(ctl.temporaryPath().isEmpty());
    QVERIFY(settings->databasePaths().contains(other));
    svc.shutdown();
}
