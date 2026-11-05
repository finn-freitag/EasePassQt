#include "AppTestSupport.h"
#include "AppTests.h"

#include "AppSettings.h"
#include "ClipboardManager.h"
#include "ClipboardService.h"
#include "DatabaseOperations.h"
#include "ISettingsStore.h"
#include "PasswordActions.h"
#include "TotpHelper.h"
#include "TotpTimerService.h"
#include "WebsiteIconManager.h"
#include "WebsiteIconService.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>

using namespace EasePass::Core;
using namespace EasePass::App;
using namespace AppTest;

namespace {
const char *kSecret = "JBSWY3DPEHPK3PXP";

QList<PasswordItem> items() {
    PasswordItem a = makeItem("A", "pw-a");
    a.username = "alice";
    a.email = "a@example.com";
    a.secret = kSecret;
    PasswordItem b = makeItem("B", QString()); // no password, no 2FA
    PasswordItem c = makeItem("C", "pw-c");
    return {a, b, c};
}

struct Fixture {
    FakeClock clock;
    std::shared_ptr<AppSettings> settings = std::make_shared<AppSettings>(std::make_shared<InMemorySettingsStore>());
    std::shared_ptr<FakeClipboard> clip = std::make_shared<FakeClipboard>();
    DatabaseService svc{kFastKdf};
    DatabaseOperations ops{settings, &svc, kFastKdf};
    ClipboardManager manager{clip, settings.get(), &clock};
    ClipboardService clipboard{&manager};
    TotpTimerService timer{&clock};
    PasswordActions actions{&svc, &ops, &clipboard, &timer};
    std::shared_ptr<FakeDatabaseSource> source = makeSource(items());

    Fixture() {
        manager.setClearOnDestruction(false);
        QSignalSpy spy(&svc, &DatabaseService::openFinished);
        svc.open(source, SecretString(kPassword));
        if (!spy.wait(30000))
            qFatal("open failed");
    }
    ~Fixture() {
        ops.waitForBackgroundWork();
        svc.shutdown();
    }
};
} // namespace

void PasswordActionsTest::testCopyActions() {
    Fixture f;
    QVERIFY(f.actions.copyPassword(0));
    QCOMPARE(f.clip->content, QStringLiteral("pw-a"));
    QVERIFY(f.clip->lastSecret);
    QVERIFY(f.actions.copyUsername(0));
    QCOMPARE(f.clip->content, QStringLiteral("alice"));
    QVERIFY(!f.clip->lastSecret);
    QVERIFY(f.actions.copyEmail(0));
    QCOMPARE(f.clip->content, QStringLiteral("a@example.com"));
    // Empty fields and bad indices copy nothing.
    QVERIFY(!f.actions.copyPassword(1));
    QVERIFY(!f.actions.copyUsername(1));
    QVERIFY(!f.actions.copyEmail(2));
    QVERIFY(!f.actions.copyPassword(9));
    QVERIFY(!f.actions.copyPassword(-1));
    QCOMPARE(f.clip->content, QStringLiteral("a@example.com"));
}

void PasswordActionsTest::testTotpCode() {
    Fixture f;
    QCOMPARE(f.actions.totpCode(0), TotpHelper::generateToken(kSecret, f.clock.now));
    QVERIFY(f.actions.totpCode(1).isEmpty());
    QVERIFY(f.actions.copyTotp(0));
    QCOMPARE(f.clip->content, f.actions.totpCode(0));
    QVERIFY(f.clip->lastSecret);
    QVERIFY(!f.actions.copyTotp(1));
    f.clock.now += 30;
    QCOMPARE(f.actions.totpCode(0), TotpHelper::generateToken(kSecret, f.clock.now));
}

void PasswordActionsTest::testRegisterClickAndReadOnly() {
    Fixture f;
    QVERIFY(!f.actions.readOnly());
    QVERIFY(f.actions.registerClick(2));
    QCOMPARE(f.svc.items().at(2).clicks.size(), 1);
    QVERIFY(!f.actions.registerClick(7));
    QSignalSpy ro(&f.actions, &PasswordActions::readOnlyChanged);
    f.source->setReadOnly(true);
    QTRY_VERIFY(f.actions.readOnly());
    QVERIFY(ro.count() >= 1);
    QVERIFY(!f.actions.registerClick(2)); // read-only databases are not modified
    QVERIFY(!f.actions.deleteItem(0));
    QCOMPARE(f.svc.itemCount(), 3);
}

void PasswordActionsTest::testDeleteItems() {
    Fixture f;
    QCOMPARE(f.actions.deleteItems({0, 2, 2, 9}), 2); // duplicates/invalid ignored, high index first
    QCOMPARE(f.svc.itemCount(), 1);
    QCOMPARE(f.svc.items().at(0).displayName, QStringLiteral("B"));
    QVERIFY(f.actions.deleteItem(0));
    QCOMPARE(f.svc.itemCount(), 0);
}

void PasswordActionsTest::testExportItems() {
    Fixture f;
    QTemporaryDir dir;
    const QString path = QDir(dir.path()).filePath("one.epdb");
    QVERIFY(!f.actions.exportItems({}, path));
    QSignalSpy done(&f.actions, &PasswordActions::exportFinished);
    QVERIFY(f.actions.exportItems({2}, QUrl::fromLocalFile(path).toString()));
    QVERIFY(f.actions.busy());
    QVERIFY(!f.actions.exportItems({0}, path)); // one export at a time
    QVERIFY(done.wait(30000));
    QVERIFY(!f.actions.busy());
    QVERIFY(done.at(0).at(0).toBool());
    QCOMPARE(done.at(0).at(2).toString(), path);

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    DatabaseFile db;
    QCOMPARE(DatabaseFile::loadFromBytes(file.readAll(), SecretString(kPassword), db, SecretString(), kFastKdf),
             LoadResult::Success);
    QCOMPARE(db.items.size(), 1);
    QCOMPARE(db.items.at(0).displayName, QStringLiteral("C"));

    // Bad target -> error message.
    QVERIFY(f.actions.exportItems({0}, QDir(dir.path()).filePath("no/such/dir/x.epdb")));
    QVERIFY(done.wait(30000));
    QVERIFY(!done.at(1).at(0).toBool());
    QVERIFY(!done.at(1).at(1).toString().isEmpty());
}

void PasswordActionsTest::testExportWithNewPassword() {
    Fixture f;
    QTemporaryDir dir;
    const QString path = QDir(dir.path()).filePath("newpw.epdb");
    QSignalSpy done(&f.actions, &PasswordActions::exportFinished);
    QVERIFY(f.actions.exportItems({0, 2}, path, "other-pass"));
    QVERIFY(done.wait(30000));
    QVERIFY(done.at(0).at(0).toBool());
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray bytes = file.readAll();
    DatabaseFile db;
    QCOMPARE(DatabaseFile::loadFromBytes(bytes, SecretString(kPassword), db, SecretString(), kFastKdf),
             LoadResult::WrongPassword);
    QCOMPARE(DatabaseFile::loadFromBytes(bytes, SecretString("other-pass"), db, SecretString(), kFastKdf),
             LoadResult::Success);
    QCOMPARE(db.items.size(), 2);
}

void PasswordActionsTest::testWebsiteIconService() {
    QTemporaryDir cache;
    auto net = std::make_shared<FakeRangeClient>();
    AppSettings settings(std::make_shared<InMemorySettingsStore>());
    WebsiteIconManager manager(net);
    manager.setCacheDir(cache.path());
    manager.setSettings(&settings);
    WebsiteIconService icons(&manager, &settings);

    QVERIFY(icons.enabled());
    QVERIFY(icons.iconUrl(QString()).isEmpty());
    // Not cached and fetching off (default, privacy): no URL and no network request.
    QVERIFY(icons.iconUrl("https://example.com/login").isEmpty());
    QCOMPARE(net->requests, 0);

    // A cached icon gives an image:// URL; showIcons=false hides it again.
    QImage img(16, 16, QImage::Format_ARGB32);
    img.fill(Qt::red);
    // Cache layout of WebsiteIconManager: <cache>/<md5(domain)>.png
    const QString file = QString::fromLatin1(QCryptographicHash::hash("example.com", QCryptographicHash::Md5).toHex()) + ".png";
    QVERIFY(img.save(QDir(cache.path()).filePath(file)));
    const QString url = icons.iconUrl("https://example.com/login");
    QVERIFY(url.startsWith("image://favicon/example.com"));
    std::unique_ptr<QQuickImageProvider> provider(icons.createImageProvider());
    QSize size;
    QVERIFY(!provider->requestPixmap(url.mid(QStringLiteral("image://favicon/").size()), &size, QSize(16, 16)).isNull());
    QSignalSpy rev(&icons, &WebsiteIconService::revisionChanged);
    settings.setShowIcons(false);
    QVERIFY(!icons.enabled());
    QVERIFY(icons.iconUrl("https://example.com").isEmpty());
    QCOMPARE(rev.count(), 1);
}
