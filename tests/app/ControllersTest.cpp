#include "AppTestSupport.h"
#include "AppTests.h"

#include "AppController.h"
#include "AppSettings.h"
#include "DatabaseController.h"
#include "DatabaseOperations.h"
#include "ISettingsStore.h"

#include <QDir>
#include <QTemporaryDir>
#include <QTest>

using namespace EasePass::Core;
using namespace EasePass::App;
using namespace AppTest;
using Page = AppController::Page;

// --- AppController --------------------------------------------------------------------

void AppControllerTest::testNavigationNeedsOpenDatabase() {
    DatabaseService svc(kFastKdf);
    AppController app(&svc);
    QCOMPARE(app.page(), Page::Login);
    QVERIFY(!app.canGoBack());
    QSignalSpy rejected(&app, &AppController::navigationRejected);
    QVERIFY(!app.navigate(Page::Passwords));
    QVERIFY(!app.navigate(Page::Database));
    QCOMPARE(rejected.count(), 2);
    QCOMPARE(app.page(), Page::Login);

    QSignalSpy changed(&app, &AppController::pageChanged);
    QVERIFY(app.navigate(Page::Settings));
    QCOMPARE(app.page(), Page::Settings);
    QVERIFY(app.canGoBack());
    QVERIFY(app.navigate(Page::Settings)); // same page: no-op
    QCOMPARE(changed.count(), 1);
    QVERIFY(app.navigate(Page::About));
    QVERIFY(app.goBack());
    QCOMPARE(app.page(), Page::Settings);
    QVERIFY(app.goBack());
    QCOMPARE(app.page(), Page::Login);
    QVERIFY(!app.goBack());
}

void AppControllerTest::testOpenAndLockResetNavigation() {
    DatabaseService svc(kFastKdf);
    AppController app(&svc);
    app.navigate(Page::About);
    QSignalSpy openChanged(&app, &AppController::databaseOpenChanged);
    QVERIFY(openItems(svc, {makeItem("A")}));
    QCOMPARE(app.page(), Page::Passwords);
    QVERIFY(!app.canGoBack());
    QVERIFY(app.databaseOpen());
    QVERIFY(app.navigate(Page::Database));
    QVERIFY(app.navigate(Page::Settings));
    svc.lock();
    QCOMPARE(app.page(), Page::Login);
    QVERIFY(!app.canGoBack());
    QVERIFY(!app.databaseOpen());
    QCOMPARE(openChanged.count(), 2);
    svc.shutdown();
}

void AppControllerTest::testGoBackSkipsUnavailablePages() {
    DatabaseService svc(kFastKdf);
    AppController app(&svc);
    QVERIFY(openItems(svc, {makeItem("A")}));
    QVERIFY(app.navigate(Page::Settings));
    // History: Passwords -> Settings. Lock clears it; Settings again from Login.
    svc.lock();
    QVERIFY(app.navigate(Page::Settings));
    QVERIFY(app.goBack());
    QCOMPARE(app.page(), Page::Login);
    svc.shutdown();
}

// --- DatabaseController ----------------------------------------------------------------

namespace {
struct ControllerFixture {
    DatabaseService svc{kFastKdf};
    std::shared_ptr<AppSettings> settings = std::make_shared<AppSettings>(std::make_shared<InMemorySettingsStore>());
    DatabaseOperations ops{settings, &svc, kFastKdf};
    DatabaseController ctl{&svc, &ops, settings};
    QHash<QString, std::shared_ptr<FakeDatabaseSource>> sources;

    ControllerFixture() {
        ctl.setSourceFactory([this](const QString &path) -> std::shared_ptr<IDatabaseSource> {
            return sources.value(path);
        });
    }
    ~ControllerFixture() {
        ops.waitForBackgroundWork();
        svc.shutdown();
    }
};
} // namespace

void DatabaseControllerTest::testOpenSuccessBusyFlags() {
    ControllerFixture f;
    f.sources.insert("db1", makeSource({makeItem("A"), makeItem("B")}));
    QSignalSpy busy(&f.ctl, &DatabaseController::busyChanged);
    QSignalSpy state(&f.ctl, &DatabaseController::stateChanged);
    QVERIFY(!f.ctl.busy());
    QVERIFY(f.ctl.open("db1", kPassword));
    QVERIFY(f.ctl.busy());
    QVERIFY(!f.ctl.open("db1", kPassword)); // second open while busy is refused
    QTRY_VERIFY_WITH_TIMEOUT(f.ctl.isOpen(), 30000);
    QVERIFY(!f.ctl.busy());
    QVERIFY(busy.count() >= 2);
    QVERIFY(state.count() >= 1);
    QVERIFY(f.ctl.errorMessage().isEmpty());
    QCOMPARE(f.ctl.databaseName(), QStringLiteral("fake"));
    QCOMPARE(f.ctl.lastDatabase(), QStringLiteral("db1"));
    QVERIFY(!f.ctl.dirty());
    QVERIFY(!f.ctl.readOnly());
}

void DatabaseControllerTest::testOpenWrongPasswordError() {
    ControllerFixture f;
    f.sources.insert("db1", makeSource({makeItem("A")}));
    QSignalSpy failed(&f.ctl, &DatabaseController::openFailed);
    QVERIFY(f.ctl.open("db1", "wrong"));
    QVERIFY(failed.wait(30000));
    QVERIFY(!f.ctl.isOpen());
    QVERIFY(!f.ctl.busy());
    QCOMPARE(f.ctl.errorMessage(), DatabaseOpResult::describe(DatabaseOpStatus::WrongPassword));
    QCOMPARE(failed.at(0).at(0).toString(), f.ctl.errorMessage());
    QVERIFY(!f.ctl.needsSecondFactor());
    f.ctl.clearError();
    QVERIFY(f.ctl.errorMessage().isEmpty());
}

void DatabaseControllerTest::testOpenNeedsSecondFactor() {
    ControllerFixture f;
    f.sources.insert("db2", std::make_shared<FakeDatabaseSource>(makeDbBytes({makeItem("A")}, SecretString("123456789012"))));
    QSignalSpy failed(&f.ctl, &DatabaseController::openFailed);
    QVERIFY(f.ctl.open("db2", kPassword));
    QVERIFY(failed.wait(30000));
    QVERIFY(f.ctl.needsSecondFactor());
    QVERIFY(!f.ctl.errorMessage().isEmpty());
    QVERIFY(f.ctl.open("db2", kPassword, "123456789012"));
    QTRY_VERIFY_WITH_TIMEOUT(f.ctl.isOpen(), 30000);
    QVERIFY(!f.ctl.needsSecondFactor());
    QVERIFY(f.ctl.errorMessage().isEmpty());
}

void DatabaseControllerTest::testOpenEmptyPathError() {
    ControllerFixture f;
    QVERIFY(!f.ctl.open(QString(), kPassword));
    QCOMPARE(f.ctl.errorMessage(), DatabaseOpResult::describe(DatabaseOpStatus::InvalidPath));
    QVERIFY(!f.ctl.open("missing", kPassword)); // factory returns no source
    QCOMPARE(f.ctl.errorMessage(), DatabaseOpResult::describe(DatabaseOpStatus::NotFound));
}

void DatabaseControllerTest::testLockAndSaveErrors() {
    ControllerFixture f;
    f.ctl.saveNow();
    QCOMPARE(f.ctl.errorMessage(), DatabaseOpResult::describe(DatabaseOpStatus::NoDatabaseOpen));
    auto src = makeSource({makeItem("A")});
    f.sources.insert("db1", src);
    QVERIFY(f.ctl.open("db1", kPassword));
    QTRY_VERIFY_WITH_TIMEOUT(f.ctl.isOpen(), 30000);

    // Save failure propagates as error message.
    src->setFailSaves(true);
    QSignalSpy dirty(&f.ctl, &DatabaseController::dirtyChanged);
    QVERIFY(f.svc.addItem(makeItem("B")));
    QVERIFY(f.ctl.dirty());
    QVERIFY(dirty.count() >= 1);
    f.ctl.saveNow();
    QTRY_VERIFY_WITH_TIMEOUT(!f.ctl.errorMessage().isEmpty(), 30000);
    QVERIFY(f.ctl.dirty());

    src->setFailSaves(false);
    f.ctl.clearError();
    f.ctl.saveNow();
    QTRY_VERIFY_WITH_TIMEOUT(!f.ctl.dirty(), 30000);
    QVERIFY(!f.ctl.saving());

    f.ctl.lock();
    QVERIFY(!f.ctl.isOpen());
    QVERIFY(f.ctl.databaseName().isEmpty());
}

void DatabaseControllerTest::testCreateDatabaseValidationAndSuccess() {
    ControllerFixture f;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = QDir(dir.path()).filePath("new.epdb");

    f.ctl.createDatabase(path, "abcd1", "abcd2");
    QCOMPARE(f.ctl.errorMessage(), DatabaseOpResult::describe(DatabaseOpStatus::PasswordsDoNotMatch));
    f.ctl.createDatabase(path, "ab", "ab");
    QCOMPARE(f.ctl.errorMessage(), DatabaseOpResult::describe(DatabaseOpStatus::PasswordTooShort));
    QVERIFY(!f.ctl.busy());

    QSignalSpy created(&f.ctl, &DatabaseController::created);
    f.ctl.createDatabase(path, "abcd1234", "abcd1234");
    QVERIFY(f.ctl.busy());
    QVERIFY(created.wait(30000));
    QVERIFY(!f.ctl.busy());
    QVERIFY(f.ctl.errorMessage().isEmpty());
    QVERIFY(QFile::exists(path));
    QVERIFY(f.ctl.knownDatabases().contains(created.at(0).at(0).toString()));
}
