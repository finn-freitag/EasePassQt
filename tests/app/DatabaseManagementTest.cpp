// T16: DatabaseManagementViewModel + DatabaseController::switchTo/makePermanent on real files
// (temporary directory, cheap KDF), wired through AppContext with fakes.
#include "AppTestSupport.h"
#include "AppTests.h"

#include "AppContext.h"
#include "AppController.h"
#include "AppSettings.h"
#include "DatabaseController.h"
#include "DatabaseManagementViewModel.h"
#include "DatabaseOperations.h"
#include "ISettingsStore.h"
#include "LocalFileDatabaseSource.h"

#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace EasePass::Core;
using namespace EasePass::App;
using namespace AppTest;

namespace {

QList<PasswordItem> sample() { return {makeItem("Bank"), makeItem("Mail"), makeItem("Cloud")}; }

struct Fixture {
    FakeClock clock;
    std::shared_ptr<FakeClipboard> clip = std::make_shared<FakeClipboard>();
    QTemporaryDir dir;
    std::unique_ptr<AppContext> ctx;
    QString pathA;
    QString pathB;

    Fixture() {
        AppContext::Deps deps;
        deps.settingsStore = std::make_shared<InMemorySettingsStore>();
        deps.clipboard = clip;
        deps.networkClient = std::make_shared<FakeRangeClient>();
        deps.screenCapture = std::make_shared<FakeScreenCapture>();
        deps.clock = &clock;
        deps.kdf = kFastKdf;
        ctx = std::make_unique<AppContext>(deps);
        pathA = writeFile(QStringLiteral("Alpha"), sample());
        pathB = writeFile(QStringLiteral("Beta"), sample());
        ctx->settings()->setDatabasePaths({pathA, pathB});
    }

    QString writeFile(const QString &name, const QList<PasswordItem> &items, const SecretString &sf = {}) {
        const QString path = dir.filePath(name + QStringLiteral(".epdb"));
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly) || f.write(makeDbBytes(items, sf)) < 0)
            qFatal("cannot write test database");
        return path;
    }

    bool open(const QString &path, bool temporary = false) {
        DatabaseService *svc = ctx->databaseService();
        QSignalSpy spy(svc, &DatabaseService::openFinished);
        const bool started = temporary
            ? ctx->databaseOperations()->openTemporaryDatabase(path, SecretString(kPassword))
            : svc->open(std::make_shared<LocalFileDatabaseSource>(path), SecretString(kPassword));
        return started && spy.wait(30000) && svc->isOpen();
    }

    QStringList readNames(const QString &path, const QString &password, const QString &sf = {}) {
        QStringList names;
        ctx->databaseOperations()->readItemsFromFile(path, SecretString(password), SecretString(sf),
                                                     [&](const DatabaseOpResult &r, const QList<PasswordItem> &items) {
                                                         if (r.ok())
                                                             for (const auto &i : items)
                                                                 names << i.displayName;
                                                     });
        ctx->databaseOperations()->waitForBackgroundWork();
        return names;
    }

    DatabaseManagementViewModel *vm() const { return ctx->databaseManagement(); }
};

QVariantMap entry(const QVariantList &list, const QString &name) {
    for (const QVariant &v : list)
        if (v.toMap().value(QStringLiteral("name")).toString() == name)
            return v.toMap();
    return {};
}

} // namespace

void DatabaseManagementTest::testDatabaseListStates() {
    Fixture f;
    const QString missing = f.dir.filePath(QStringLiteral("Gone.epdb"));
    f.ctx->settings()->setDatabasePaths({f.pathA, f.pathB, missing, f.pathA});
    QCOMPARE(f.vm()->databases().size(), 3); // duplicates dropped
    QVERIFY(!entry(f.vm()->databases(), QStringLiteral("Alpha")).value(QStringLiteral("loaded")).toBool());

    QSignalSpy changed(f.vm(), &DatabaseManagementViewModel::databasesChanged);
    QVERIFY(f.open(f.pathA));
    QVERIFY(changed.count() > 0);
    const QVariantList list = f.vm()->databases();
    QCOMPARE(entry(list, QStringLiteral("Alpha")).value(QStringLiteral("loaded")).toBool(), true);
    QCOMPARE(entry(list, QStringLiteral("Alpha")).value(QStringLiteral("state")).toString(), QStringLiteral("Loaded"));
    QCOMPARE(entry(list, QStringLiteral("Beta")).value(QStringLiteral("state")).toString(), QString());
    QCOMPARE(entry(list, QStringLiteral("Gone")).value(QStringLiteral("exists")).toBool(), false);
    QCOMPARE(entry(list, QStringLiteral("Gone")).value(QStringLiteral("state")).toString(), QStringLiteral("File not found"));
    QCOMPARE(f.vm()->loadedPath(), f.pathA);
    QCOMPARE(f.vm()->itemNames(), QStringList({"Bank", "Mail", "Cloud"}));

    // A temporary database is listed although it is not a known database.
    const QString pathT = f.writeFile(QStringLiteral("Temp"), sample());
    QVERIFY(f.open(pathT, true));
    const QVariantMap t = entry(f.vm()->databases(), QStringLiteral("Temp"));
    QVERIFY(t.value(QStringLiteral("temporary")).toBool());
    QVERIFY(t.value(QStringLiteral("loaded")).toBool());
    QVERIFY(f.vm()->readOnly());
}

void DatabaseManagementTest::testDeleteNeedsTypedName() {
    Fixture f;
    QVERIFY(f.open(f.pathA));
    QVERIFY(!f.vm()->deleteDatabase(f.pathB, QStringLiteral("beta")).isEmpty());
    QVERIFY(!f.vm()->deleteDatabase(f.pathB, QString()).isEmpty());
    QVERIFY(QFileInfo::exists(f.pathB));
    // The loaded database cannot be deleted, even with the right name.
    QVERIFY(!f.vm()->deleteDatabase(f.pathA, QStringLiteral("Alpha")).isEmpty());
    QVERIFY(QFileInfo::exists(f.pathA));

    QCOMPARE(f.vm()->deleteDatabase(f.pathB, QStringLiteral("Beta")), QString());
    QVERIFY(!QFileInfo::exists(f.pathB));
    QCOMPARE(f.ctx->settings()->databasePaths(), QStringList{f.pathA});
    // The last known database cannot be deleted (C#: CantDeleteDatabase).
    f.ctx->databaseService()->close();
    QVERIFY(!f.vm()->deleteDatabase(f.pathA, QStringLiteral("Alpha")).isEmpty());
}

void DatabaseManagementTest::testRenameAndCopyPath() {
    Fixture f;
    QVERIFY(!f.vm()->rename(f.pathB, QStringLiteral("Alpha")).isEmpty()); // exists
    QCOMPARE(f.vm()->rename(f.pathB, QStringLiteral("Gamma")), QString());
    const QString gamma = f.dir.filePath(QStringLiteral("Gamma.epdb"));
    QVERIFY(QFileInfo::exists(gamma));
    QVERIFY(f.ctx->settings()->databasePaths().contains(gamma));
    QCOMPARE(f.vm()->nameForPath(QUrl::fromLocalFile(gamma).toString()), QStringLiteral("Gamma"));

    f.vm()->copyPath(gamma);
    QCOMPARE(f.clip->content, gamma);
    QVERIFY(!f.clip->lastSecret);
}

void DatabaseManagementTest::testChangePasswordAndSecondFactor() {
    Fixture f;
    QVERIFY(f.open(f.pathA));
    QSignalSpy pw(f.vm(), &DatabaseManagementViewModel::passwordChangeFinished);
    f.vm()->changePassword(QStringLiteral("wrong"), QStringLiteral("abcd1234"), QStringLiteral("abcd1234"));
    QVERIFY(pw.wait());
    QVERIFY(!pw.at(0).at(0).toBool());
    f.vm()->changePassword(QString::fromLatin1(kPassword), QStringLiteral("abcd1234"), QStringLiteral("abcd"));
    QVERIFY(pw.wait());
    QVERIFY(!pw.at(1).at(0).toBool());
    f.vm()->changePassword(QString::fromLatin1(kPassword), QStringLiteral("abcd1234"), QStringLiteral("abcd1234"));
    QVERIFY(pw.wait());
    QVERIFY(pw.at(2).at(0).toBool());
    f.ctx->databaseService()->waitForBackgroundWork();
    QCOMPARE(f.readNames(f.pathA, QStringLiteral("abcd1234")).size(), 3);

    QCOMPARE(f.vm()->currentSecondFactorToken(), QString());
    const QString token = DatabaseManagementViewModel::generateSecondFactorToken();
    QCOMPARE(token.size(), 12);
    QSignalSpy sf(f.vm(), &DatabaseManagementViewModel::secondFactorFinished);
    f.vm()->applySecondFactor(true, 3, token); // Authenticator: not supported
    QVERIFY(sf.wait());
    QVERIFY(!sf.at(0).at(0).toBool());
    QVERIFY(!f.vm()->secondFactorEnabled());
    f.vm()->applySecondFactor(true, 2, token);
    QVERIFY(sf.wait());
    QVERIFY(sf.at(1).at(0).toBool());
    QVERIFY(f.vm()->secondFactorEnabled());
    QCOMPARE(f.vm()->secondFactorType(), 2);
    QCOMPARE(f.vm()->currentSecondFactorToken(), token);
    f.vm()->applySecondFactor(false, 1, QStringLiteral("ignored"));
    QVERIFY(sf.wait());
    QVERIFY(!f.vm()->secondFactorEnabled());
    QCOMPARE(f.vm()->currentSecondFactorToken(), QString());
}

void DatabaseManagementTest::testExportSelectionAndNewPassword() {
    Fixture f;
    QVERIFY(f.open(f.pathA));
    QSignalSpy done(f.vm(), &DatabaseManagementViewModel::exportFinished);
    const QString out = f.dir.filePath(QStringLiteral("out.epdb"));
    // Indexes out of range / duplicates are ignored, the database order is kept.
    f.vm()->exportDatabase(out, {2, 0, 2, 7, -1});
    QVERIFY(done.wait());
    QVERIFY(done.at(0).at(0).toBool());
    QCOMPARE(f.readNames(out, QString::fromLatin1(kPassword)), QStringList({"Bank", "Cloud"}));

    QCOMPARE(DatabaseManagementViewModel::validateNewPassword(QStringLiteral("ab"), QStringLiteral("ab")).isEmpty(), false);
    QCOMPARE(DatabaseManagementViewModel::validateNewPassword(QStringLiteral("abcd"), QStringLiteral("abce")).isEmpty(), false);
    QCOMPARE(DatabaseManagementViewModel::validateNewPassword(QStringLiteral("abcd"), QStringLiteral("abcd")), QString());

    const QString out2 = f.dir.filePath(QStringLiteral("out2.epdb"));
    f.vm()->exportWithNewPassword(out2, {0, 1, 2}, QStringLiteral("newpass"), QStringLiteral("other"));
    QVERIFY(done.wait());
    QVERIFY(!done.at(1).at(0).toBool());
    QVERIFY(!QFileInfo::exists(out2));
    f.vm()->exportWithNewPassword(out2, {0, 1, 2}, QStringLiteral("newpass"), QStringLiteral("newpass"));
    QVERIFY(done.wait());
    QVERIFY(done.at(2).at(0).toBool());
    QCOMPARE(f.readNames(out2, QStringLiteral("newpass")).size(), 3);
    QVERIFY(f.readNames(out2, QString::fromLatin1(kPassword)).isEmpty());

    // Backup: everything; the file of the open database is refused.
    const QString backup = f.dir.filePath(f.vm()->defaultBackupFileName());
    QVERIFY(f.vm()->defaultBackupFileName().startsWith(QStringLiteral("Alpha")));
    f.vm()->backup(backup);
    QVERIFY(done.wait());
    QVERIFY(done.at(3).at(0).toBool());
    QCOMPARE(f.readNames(backup, QString::fromLatin1(kPassword)).size(), 3);
    f.vm()->backup(f.pathA);
    QVERIFY(done.wait());
    QVERIFY(!done.at(4).at(0).toBool());
}

void DatabaseManagementTest::testImportIntoSelectedItems() {
    Fixture f;
    const QString other = f.writeFile(QStringLiteral("Other"), {makeItem("X"), makeItem("Y")},
                                      SecretString(QStringLiteral("123456789012")));
    QVERIFY(f.open(f.pathA));
    QSignalSpy read(f.vm(), &DatabaseManagementViewModel::importFileRead);
    f.vm()->readImportFile(other, QString::fromLatin1(kPassword), QString());
    QVERIFY(read.wait());
    QVERIFY(!read.at(0).at(0).toBool());
    QVERIFY(read.at(0).at(2).toBool()); // needs second factor
    QVERIFY(f.vm()->importCandidates().isEmpty());
    f.vm()->readImportFile(other, QString::fromLatin1(kPassword), QStringLiteral("123456789012"));
    QVERIFY(read.wait());
    QVERIFY(read.at(1).at(0).toBool());
    QCOMPARE(f.vm()->importCandidates(), QStringList({"X", "Y"}));

    QVERIFY(!f.vm()->importSelected({}, false)); // nothing selected
    QVERIFY(f.vm()->importSelected({1}, false));
    QCOMPARE(f.vm()->itemNames(), QStringList({"Bank", "Mail", "Cloud", "Y"}));
    QVERIFY(f.vm()->importCandidates().isEmpty());

    f.vm()->readImportFile(other, QString::fromLatin1(kPassword), QStringLiteral("123456789012"));
    QVERIFY(read.wait());
    QVERIFY(f.vm()->importSelected({0, 1}, true));
    QCOMPARE(f.vm()->itemNames(), QStringList({"X", "Y"}));

    // Locking drops pending candidates.
    f.vm()->readImportFile(other, QString::fromLatin1(kPassword), QStringLiteral("123456789012"));
    QVERIFY(read.wait());
    QVERIFY(!f.vm()->importCandidates().isEmpty());
    f.ctx->databaseService()->close();
    QVERIFY(f.vm()->importCandidates().isEmpty());
}

void DatabaseManagementTest::testSwitchToAndMakePermanent() {
    Fixture f;
    DatabaseController *ctl = f.ctx->databaseController();
    QVERIFY(f.open(f.pathA));
    QVERIFY(!ctl->switchTo(f.pathA));                                   // already loaded
    QVERIFY(!ctl->switchTo(f.dir.filePath(QStringLiteral("x.epdb"))));  // unknown
    QVERIFY(ctl->switchTo(QUrl::fromLocalFile(f.pathB).toString()));
    QVERIFY(!f.ctx->databaseService()->isOpen());
    QCOMPARE(ctl->lastDatabase(), f.pathB);
    QCOMPARE(f.ctx->appController()->page(), AppController::Page::Login);

    const QString pathT = f.writeFile(QStringLiteral("Temp"), sample());
    QVERIFY(!ctl->makePermanent()); // nothing open
    QVERIFY(f.open(pathT, true));
    QVERIFY(ctl->temporary());
    QVERIFY(ctl->readOnly());
    QVERIFY(ctl->databaseName().contains(QStringLiteral("(Temp)")));
    QVERIFY(ctl->makePermanent());
    QVERIFY(!ctl->temporary());
    QVERIFY(!ctl->readOnly());
    QVERIFY(!ctl->databaseName().contains(QStringLiteral("(Temp)")));
    QVERIFY(f.ctx->settings()->databasePaths().contains(QFileInfo(pathT).absoluteFilePath()));
    QVERIFY(f.ctx->databaseService()->addItem(makeItem("New")));        // writable now
}
