#include "AppTestSupport.h"
#include "AppTests.h"

#include "AboutViewModel.h"
#include "AppContext.h"
#include "BuildInfo.h"
#include "AppController.h"
#include "AppSettings.h"
#include "ClipboardManager.h"
#include "ClipboardService.h"
#include "DatabaseController.h"
#include "DialogService.h"
#include "ISettingsStore.h"
#include "ItemViewModel.h"
#include "PasswordFilterModel.h"
#include "PrintService.h"
#include "PrintViewModel.h"
#include "QrCodeScanner.h"
#include "ScreenScanViewModel.h"
#include "SettingsViewModel.h"
#include "TotpTimerService.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QTest>

using namespace EasePass::Core;
using namespace EasePass::App;
using namespace AppTest;

void AppServicesTest::testTotpTimerRefCount() {
    FakeClock clock;
    TotpTimerService timer(&clock);
    QSignalSpy running(&timer, &TotpTimerService::runningChanged);
    QSignalSpy tick(&timer, &TotpTimerService::tick);
    timer.release(); // unbalanced release is ignored
    QCOMPARE(timer.subscribers(), 0);
    timer.acquire();
    timer.acquire();
    QVERIFY(timer.isRunning());
    QCOMPARE(running.count(), 1);
    timer.release();
    QVERIFY(timer.isRunning());
    timer.release();
    QVERIFY(!timer.isRunning());
    QCOMPARE(running.count(), 2);
    clock.now = 42;
    timer.tickNow();
    QCOMPARE(tick.count(), 1);
    QCOMPARE(tick.at(0).at(0).toLongLong(), 42);
    QCOMPARE(timer.now(), 42);
}

void AppServicesTest::testDialogServiceRequestResponse() {
    DialogService dialogs;
    QSignalSpy requested(&dialogs, &DialogService::dialogRequested);
    QSignalSpy closed(&dialogs, &DialogService::dialogClosed);
    QList<DialogService::Result> results;
    const int id = dialogs.ask("Delete?", "Really?", "Delete", "Keep", "Cancel",
                               [&](DialogService::Result r) { results << r; });
    QCOMPARE(requested.count(), 1);
    QCOMPARE(requested.at(0).at(0).toInt(), id);
    QCOMPARE(requested.at(0).at(1).toString(), QStringLiteral("Delete?"));
    QCOMPARE(requested.at(0).at(4).toString(), QStringLiteral("Keep"));
    QCOMPARE(dialogs.pendingCount(), 1);
    QVERIFY(dialogs.isPending(id));
    QVERIFY(dialogs.respond(id, DialogService::Result::Primary));
    QCOMPARE(results, QList<DialogService::Result>{DialogService::Result::Primary});
    QCOMPARE(closed.count(), 1);
    QCOMPARE(dialogs.pendingCount(), 0);
    QVERIFY(!dialogs.respond(id, DialogService::Result::Secondary)); // answered once only
    QVERIFY(!dialogs.respond(999, DialogService::Result::Primary));
    QCOMPARE(results.size(), 1);

    const int id2 = dialogs.confirm("Sure?", "msg", "Yes", [&](DialogService::Result r) { results << r; });
    QVERIFY(id2 != id);
    QCOMPARE(requested.last().at(5).toString(), QCoreApplication::translate("EasePass::App::DialogService", "Cancel"));
    QVERIFY(requested.last().at(4).toString().isEmpty());

    QSignalSpy info(&dialogs, &DialogService::infoRequested);
    dialogs.showInfo(DialogService::Severity::Error, "Oops", "failed");
    QCOMPARE(info.count(), 1);
    QCOMPARE(info.at(0).at(0).value<DialogService::Severity>(), DialogService::Severity::Error);
}

void AppServicesTest::testDialogServiceCancelAll() {
    DialogService dialogs;
    QList<DialogService::Result> results;
    dialogs.ask("a", "", "OK", "", "", [&](DialogService::Result r) { results << r; });
    dialogs.ask("b", "", "OK", "", "", [&](DialogService::Result r) { results << r; });
    dialogs.cancelAll();
    QCOMPARE(results, (QList<DialogService::Result>{DialogService::Result::None, DialogService::Result::None}));
    QCOMPARE(dialogs.pendingCount(), 0);
}

void AppServicesTest::testClipboardService() {
    FakeClock clock;
    auto clip = std::make_shared<FakeClipboard>();
    AppSettings settings(std::make_shared<InMemorySettingsStore>());
    ClipboardManager manager(clip, &settings, &clock);
    manager.setClearOnDestruction(false);
    ClipboardService service(&manager);
    QSignalSpy state(&service, &ClipboardService::stateChanged);
    QSignalSpy copied(&service, &ClipboardService::copied);
    service.copy(QString()); // nothing to copy
    QCOMPARE(clip->setCount, 0);
    service.copy("hunter2");
    QCOMPARE(clip->content, QStringLiteral("hunter2"));
    QVERIFY(clip->lastSecret);
    QCOMPARE(copied.count(), 1);
    QVERIFY(service.clearPending());
    QCOMPARE(service.secondsUntilClear(), settings.clipboardClearSeconds());
    service.clearNow();
    QVERIFY(!service.clearPending());
    QVERIFY(clip->content.isEmpty());
    QVERIFY(state.count() >= 2);
    service.copy("user", false);
    QVERIFY(!clip->lastSecret);
}

void AppServicesTest::testSettingsViewModel() {
    AppSettings settings(std::make_shared<InMemorySettingsStore>());
    SettingsViewModel vm(&settings);
    QCOMPARE(vm.settings(), &settings);
    QCOMPARE(vm.minPasswordLength(), 8);
    QCOMPARE(vm.maxInactivityMinutes(), 1000);
    QVERIFY(vm.passwordCharsValid());
    QSignalSpy valid(&vm, &SettingsViewModel::passwordCharsValidChanged);
    settings.setPasswordChars("aaaa");
    QVERIFY(!vm.passwordCharsValid());
    QCOMPARE(valid.count(), 1);
    vm.resetPasswordChars();
    QVERIFY(vm.passwordCharsValid());
    settings.setPasswordLength(40);
    settings.setInactivityLogoutMinutes(99);
    vm.resetPasswordLength();
    QCOMPARE(settings.passwordLength(), AppSettings::Defaults::PasswordLength);
    vm.resetToDefaults();
    QCOMPARE(settings.inactivityLogoutMinutes(), AppSettings::Defaults::InactivityLogoutMinutes);
}

void AppServicesTest::testSettingsLanguages() {
    AppSettings settings(std::make_shared<InMemorySettingsStore>());
    settings.setLanguage("de-DE"); // language the app started with
    SettingsViewModel vm(&settings);
    const QVariantList langs = vm.languages();
    QCOMPARE(langs.size(), 3);
    QCOMPARE(langs.at(0).toMap().value("tag").toString(), QStringLiteral("en-US"));
    QCOMPARE(langs.at(2).toMap().value("name").toString(), QStringLiteral("Português (Brasil)"));
    QCOMPARE(vm.languageIndex(), 1);
    QVERIFY(!vm.restartRequired());
    QSignalSpy changed(&vm, &SettingsViewModel::languageChanged);
    vm.setLanguageIndex(2);
    QCOMPARE(settings.language(), QStringLiteral("pt-BR"));
    QVERIFY(vm.restartRequired());
    QCOMPARE(changed.count(), 1);
    vm.setLanguageIndex(7); // out of range: ignored
    vm.setLanguageIndex(-1);
    QCOMPARE(settings.language(), QStringLiteral("pt-BR"));
    vm.setLanguageIndex(1);
    QVERIFY(!vm.restartRequired());
    settings.setLanguage("fr-FR"); // unknown tag (e.g. edited config)
    QCOMPARE(vm.languageIndex(), -1);
}

void AppServicesTest::testSettingsPasswordLengthText() {
    AppSettings settings(std::make_shared<InMemorySettingsStore>());
    SettingsViewModel vm(&settings);
    QCOMPARE(vm.applyPasswordLengthText("2x4"), QStringLiteral("24"));
    QCOMPARE(settings.passwordLength(), 24);
    QCOMPARE(vm.applyPasswordLengthText("3"), QStringLiteral("3"));
    QCOMPARE(settings.passwordLength(), 8);
    QCOMPARE(vm.applyPasswordLengthText(""), QString());
    QCOMPARE(settings.passwordLength(), AppSettings::Defaults::PasswordLength);
    vm.applyPasswordLengthText("40");
    vm.applyPasswordLengthText("00");
    QCOMPARE(settings.passwordLength(), AppSettings::Defaults::PasswordLength);
    vm.applyPasswordLengthText("123456");
    QCOMPARE(settings.passwordLength(), 999);
    vm.applyPasswordLengthText("99999999999999999999"); // does not fit in an int
    QCOMPARE(settings.passwordLength(), 999);
    QCOMPARE(vm.applyPasswordLengthText(QString::fromUtf8("1\u0663")), QStringLiteral("1")); // ASCII digits only
    vm.applyPasswordCharsText(QString());
    QCOMPARE(settings.passwordChars(), AppSettings::Defaults::PasswordChars);
    vm.applyPasswordCharsText("xy");
    QCOMPARE(settings.passwordChars(), QStringLiteral("xy"));
    QCOMPARE(vm.minClipboardSeconds(), 1);
    QCOMPARE(vm.maxClipboardSeconds(), 100000);
}

void AppServicesTest::testSettingsResetPopularity() {
    AppSettings settings(std::make_shared<InMemorySettingsStore>());
    SettingsViewModel vm(&settings);
    QSignalSpy msg(&vm, &SettingsViewModel::message);
    QVERIFY(!vm.canResetPopularity()); // no service
    QVERIFY(!vm.resetPopularity());
    QCOMPARE(msg.count(), 1);

    DatabaseService svc(kFastKdf);
    vm.setDatabaseService(&svc);
    QVERIFY(!vm.canResetPopularity()); // closed
    QSignalSpy can(&vm, &SettingsViewModel::canResetPopularityChanged);
    auto source = makeSource({makeItem("A"), makeItem("B")});
    QSignalSpy opened(&svc, &DatabaseService::openFinished);
    QVERIFY(svc.open(source, SecretString(kPassword)));
    QVERIFY(opened.wait());
    QVERIFY(can.count() > 0);
    QVERIFY(vm.canResetPopularity());
    FakeClock clock;
    QVERIFY(svc.registerClick(0, &clock));
    QVERIFY(svc.registerClick(1, &clock));
    QVERIFY(vm.resetPopularity());
    QVERIFY(svc.items().at(0).clicks.isEmpty());
    QVERIFY(svc.items().at(1).clicks.isEmpty());
    QCOMPARE(msg.last().at(0).toInt(), 1);

    source->setReadOnly(true);
    QTRY_VERIFY(!vm.canResetPopularity());
    QVERIFY(!vm.resetPopularity());
    svc.shutdown();
}

void AppServicesTest::testAboutChangelogParser() {
    const QString text = QString::fromUtf8("\xEF\xBB\xBF#Version 2.0 [01.01.2030]\n"
                                           "\t\u2022 First\r\n"
                                           "\n"
                                           "\t\u2022 Second\n"
                                           "---More\n"
                                           "#  Version 1.0\n"
                                           "plain\n\n\n");
    const QVariantList blocks = AboutViewModel::parseChangelog(text);
    QCOMPARE(blocks.size(), 2);
    const QVariantMap first = blocks.at(0).toMap();
    QCOMPARE(first.value("title").toString(), QStringLiteral("Version 2.0 [01.01.2030]"));
    QCOMPARE(first.value("lines").toStringList(),
             QStringList({QString::fromUtf8("\u2022 First"), QString::fromUtf8("\u2022 Second"), QString(), "More"}));
    QCOMPARE(blocks.at(1).toMap().value("title").toString(), QStringLiteral("Version 1.0"));
    QCOMPARE(blocks.at(1).toMap().value("lines").toStringList(), QStringList{"plain"});
    QVERIFY(AboutViewModel::parseChangelog(QString()).isEmpty());

    // The bundled changelog (C#: Assets/changelog.txt) starts with the current version.
    AboutViewModel vm;
    QVERIFY(vm.changelog().size() >= 5);
    QVERIFY(vm.changelog().first().toMap().value("title").toString().startsWith("Version " + vm.version()));
}

void AppServicesTest::testAboutOpenUrl() {
    AboutViewModel vm;
    QStringList opened;
    bool succeed = true;
    vm.setUrlOpener([&](const QUrl &url) {
        opened << url.toString();
        return succeed;
    });
    QSignalSpy spy(&vm, &AboutViewModel::urlOpened);
    QVERIFY(vm.openUrl(vm.repositoryUrl()));
    QVERIFY(vm.openUrl("mailto:" + vm.contactEmail()));
    QVERIFY(vm.openUrl(vm.websiteUrl())); // http, as in C#
    QVERIFY(!vm.openUrl("file:///etc/passwd"));
    QVERIFY(!vm.openUrl("javascript:alert(1)"));
    QVERIFY(!vm.openUrl("ftp://example.org"));
    QVERIFY(!vm.openUrl("not a url with spaces"));
    QCOMPARE(opened.size(), 3);
    QCOMPARE(spy.count(), 3);
    succeed = false; // opener failure is reported
    QVERIFY(!vm.openUrl("https://example.org"));
    QCOMPARE(spy.count(), 3);
    QVERIFY(vm.issuesUrl().startsWith(vm.repositoryUrl()));
}

void AppServicesTest::testAboutBuildInfo() {
    QCOMPARE(BuildInfo::appVersion(), QStringLiteral(EASEPASS_VERSION));
    QVERIFY(!AboutViewModel::version().isEmpty());
    const auto deps = BuildInfo::dependencies();
    QCOMPARE(deps.size(), 4);
    QCOMPARE(deps.at(0).name, QStringLiteral("Qt"));
    QCOMPARE(deps.at(0).version, QString::fromLatin1(qVersion()));
    QVERIFY(deps.at(1).version.at(0).isDigit()); // OpenSSL "3.x.y"
    QVERIFY(deps.at(3).version.count('.') == 2);  // zxing-cpp
    for (const auto &d : deps) {
        QVERIFY(!d.license.isEmpty());
        QVERIFY(d.url.startsWith("https://"));
    }
    QCOMPARE(AboutViewModel::developers().size(), 2);
    QCOMPARE(AboutViewModel::dependencies().size(), 4);
}

void AppServicesTest::testPrintViewModelExportPdf() {
    DatabaseService svc(kFastKdf);
    PrintService print;
    PrintViewModel vm(&svc, &print);
    QVERIFY(!vm.canPrint());
    QVERIFY(!vm.exportPdf("/tmp/never.pdf"));
    QVERIFY(!vm.lastError().isEmpty());
    QVERIFY(!vm.plaintextWarning().isEmpty());

    QSignalSpy canPrint(&vm, &PrintViewModel::canPrintChanged);
    QVERIFY(openItems(svc, {makeItem("A"), makeItem("B")}));
    QVERIFY(vm.canPrint());
    QVERIFY(canPrint.count() >= 1);
    QTemporaryDir dir;
    const QString path = QDir(dir.path()).filePath("out.pdf");
    QVERIFY(vm.exportPdf(QUrl::fromLocalFile(path).toString())); // file:// URL from a FileDialog
    QVERIFY(vm.lastError().isEmpty());
    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QVERIFY(f.read(5).startsWith("%PDF"));
    QVERIFY(!vm.exportPdf(QDir(dir.path()).filePath("missing/dir/out.pdf")));
    QVERIFY(!vm.lastError().isEmpty());
    svc.shutdown();
}

namespace {
QImage fixture(const char *name) {
    return QImage(QString::fromUtf8(EASEPASS_TEST_FIXTURES_DIR) + "/" + name);
}
} // namespace

void AppServicesTest::testScreenScanViewModelFound() {
    auto capture = std::make_shared<FakeScreenCapture>();
    const QImage qr = fixture("qr_valid.png");
    QVERIFY(!qr.isNull());
    QImage screen(qr.width() + 200, qr.height() + 200, QImage::Format_RGB32);
    screen.fill(Qt::white);
    QPainter(&screen).drawImage(100, 100, qr);
    capture->result.image = screen;
    capture->result.logicalGeometry = QRect(QPoint(0, 0), screen.size());
    const QString expected = QrCodeScanner::scanImage(qr);
    QVERIFY(!expected.isEmpty());

    ScreenScanViewModel vm(capture);
    QVERIFY(vm.available());
    QSignalSpy finished(&vm, &ScreenScanViewModel::scanFinished);
    QSignalSpy otp(&vm, &ScreenScanViewModel::otpauthFound);
    QVERIFY(vm.start());
    QVERIFY(vm.busy());
    QVERIFY(!vm.start()); // already busy
    QVERIFY(finished.wait(10000));
    QCOMPARE(vm.status(), ScreenScanViewModel::Status::Found);
    QCOMPARE(vm.resultText(), expected);
    QVERIFY(!vm.busy());
    QCOMPARE(otp.count(), expected.startsWith("otpauth://", Qt::CaseInsensitive) ? 1 : 0);

    // Re-scan a region of the last capture.
    QVERIFY(vm.scanRegion(QRect(90, 90, qr.width() + 20, qr.height() + 20)));
    QVERIFY(finished.wait(10000));
    QCOMPARE(vm.status(), ScreenScanViewModel::Status::Found);
    QVERIFY(!vm.chooseCandidate(0)); // only valid in Multiple state
}

void AppServicesTest::testScreenScanViewModelFailureAndClear() {
    auto capture = std::make_shared<FakeScreenCapture>();
    capture->result.errorString = "portal denied";
    ScreenScanViewModel vm(capture);
    QSignalSpy finished(&vm, &ScreenScanViewModel::scanFinished);
    QVERIFY(!vm.scanRegion(QRect(0, 0, 10, 10))); // nothing captured yet
    QVERIFY(vm.start());
    QVERIFY(finished.wait(10000));
    QVERIFY(vm.status() == ScreenScanViewModel::Status::Failed || vm.status() == ScreenScanViewModel::Status::Cancelled);
    capture->result = {};
    capture->result.image = QImage(200, 200, QImage::Format_RGB32);
    capture->result.image.fill(Qt::white);
    capture->result.logicalGeometry = QRect(0, 0, 200, 200);
    QVERIFY(vm.start());
    QVERIFY(finished.wait(10000));
    QCOMPARE(vm.status(), ScreenScanViewModel::Status::NoneFound);
    vm.clear();
    QCOMPARE(vm.status(), ScreenScanViewModel::Status::Idle);
    QVERIFY(vm.resultText().isEmpty());
    capture->available = false;
    QVERIFY(!vm.available());
}

void AppServicesTest::testAppContextQmlRegistration() {
    FakeClock clock;
    AppContext::Deps deps;
    deps.settingsStore = std::make_shared<InMemorySettingsStore>();
    deps.clipboard = std::make_shared<FakeClipboard>();
    deps.networkClient = std::make_shared<FakeRangeClient>();
    deps.screenCapture = std::make_shared<FakeScreenCapture>();
    deps.clock = &clock;
    deps.kdf = kFastKdf;
    AppContext ctx(deps);
    ctx.registerQml();

    // Wiring: opening via the service drives navigation, list and item view model.
    QVERIFY(openItems(*ctx.databaseService(), {makeItem("Zeta"), makeItem("Alpha")}));
    QCOMPARE(ctx.appController()->page(), AppController::Page::Passwords);
    QCOMPARE(ctx.passwordFilterModel()->count(), 2);

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(R"(
        import QtQml
        import EasePass.App
        QtObject {
            property int page: AppController.page
            property bool open: DatabaseController.isOpen
            property int count: PasswordList.count
            property bool settingsOk: SettingsViewModel.settings.passwordLength >= 8
            property bool loaded: ItemViewModel.load(1)
            property string name: ItemViewModel.displayName
            property bool timerIdle: !TotpTimer.running
            property int pending: DialogService.pendingCount
            property bool canPrint: PrintViewModel.canPrint
            property bool scanIdle: !ScreenScanViewModel.busy
            property bool clip: !ClipboardService.clearPending
            function sortByName() { PasswordList.sortMode = 0 }
        }
    )", QUrl("qrc:/test.qml"));
    std::unique_ptr<QObject> obj(component.create());
    QVERIFY2(obj, qPrintable(component.errorString()));
    QCOMPARE(obj->property("page").toInt(), int(AppController::Page::Passwords));
    QVERIFY(obj->property("open").toBool());
    QCOMPARE(obj->property("count").toInt(), 2);
    QVERIFY(obj->property("settingsOk").toBool());
    QVERIFY(obj->property("loaded").toBool());
    QCOMPARE(obj->property("name").toString(), QStringLiteral("Alpha"));
    QVERIFY(obj->property("timerIdle").toBool());
    QCOMPARE(obj->property("pending").toInt(), 0);
    QVERIFY(obj->property("canPrint").toBool());
    QVERIFY(obj->property("scanIdle").toBool());
    QVERIFY(obj->property("clip").toBool());
    QMetaObject::invokeMethod(obj.get(), "sortByName");
    QCOMPARE(ctx.passwordFilterModel()->sortMode(), PasswordFilterModel::SortMode::Name);

    // Locking cancels open dialogs and unloads the item.
    bool cancelled = false;
    ctx.dialogService()->ask("q", "", "OK", "", "", [&](DialogService::Result r) {
        cancelled = r == DialogService::Result::None;
    });
    ctx.databaseController()->lock();
    QVERIFY(cancelled);
    QVERIFY(!ctx.itemViewModel()->hasItem());
    QCOMPARE(obj->property("page").toInt(), int(AppController::Page::Login));
    QCOMPARE(obj->property("count").toInt(), 0);
}
