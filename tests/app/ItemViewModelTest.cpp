#include "AppTestSupport.h"
#include "AppTests.h"

#include "AppSettings.h"
#include "ClipboardManager.h"
#include "ClipboardService.h"
#include "ISettingsStore.h"
#include "ItemViewModel.h"
#include "SortingHelper.h"
#include "TotpHelper.h"
#include "TotpTimerService.h"

#include <QTest>

using namespace EasePass::Core;
using namespace EasePass::App;
using namespace AppTest;

namespace {
const char *kSecret = "JBSWY3DPEHPK3PXP";
// SHA1("Leaked123") = 08699 3AE852FE8860F403C26869DE1E649DBE607
const QByteArray kLeakedBody = "3AE852FE8860F403C26869DE1E649DBE607:42\r\n0000000000000000000000000000000000A:1\r\n";

struct Fixture {
    FakeClock clock;
    DatabaseService svc{kFastKdf};
    TotpTimerService timer{&clock};
    std::shared_ptr<AppSettings> settings = std::make_shared<AppSettings>(std::make_shared<InMemorySettingsStore>());
    ItemViewModel vm{&svc, &timer};

    explicit Fixture(const QList<PasswordItem> &items = {makeItem("A", "pw1"), makeItem("B", "pw2")}) {
        vm.setSettings(settings.get());
        if (!openItems(svc, items))
            qFatal("open failed");
    }
    ~Fixture() { svc.shutdown(); }
};
} // namespace

void ItemViewModelTest::testLoadEditCancel() {
    Fixture f;
    QVERIFY(!f.vm.hasItem());
    QVERIFY(!f.vm.load(5));
    QSignalSpy loaded(&f.vm, &ItemViewModel::loadedChanged);
    QSignalSpy buffer(&f.vm, &ItemViewModel::bufferChanged);
    QVERIFY(f.vm.load(1));
    QCOMPARE(loaded.count(), 1);
    QCOMPARE(f.vm.index(), 1);
    QVERIFY(!f.vm.isNew());
    QCOMPARE(f.vm.displayName(), QStringLiteral("B"));
    QCOMPARE(f.vm.password(), QStringLiteral("pw2"));
    QVERIFY(!f.vm.dirty());

    f.vm.setUsername("bob");
    f.vm.setTags({"x", "y"});
    f.vm.setPassword("newpw");
    QVERIFY(f.vm.dirty());
    QVERIFY(buffer.count() >= 4);
    QCOMPARE(f.svc.items().at(1).username, QString()); // service untouched until commit
    f.vm.setUsername("bob"); // unchanged value: no signal
    const int before = buffer.count();
    f.vm.setUsername("bob");
    QCOMPARE(buffer.count(), before);

    f.vm.cancel();
    QVERIFY(!f.vm.dirty());
    QCOMPARE(f.vm.username(), QString());
    QCOMPARE(f.vm.password(), QStringLiteral("pw2"));
    QVERIFY(f.vm.tags().isEmpty());
}

void ItemViewModelTest::testCommitUpdatesService() {
    Fixture f;
    QVERIFY(f.vm.load(0));
    f.vm.setDisplayName("Alpha");
    f.vm.setNotes("note");
    QSignalSpy committed(&f.vm, &ItemViewModel::committed);
    QSignalSpy items(&f.svc, &DatabaseService::itemsChanged);
    QVERIFY(f.vm.commit());
    QCOMPARE(committed.count(), 1);
    QCOMPARE(committed.at(0).at(0).toInt(), 0);
    QVERIFY(items.count() >= 1);
    QCOMPARE(f.svc.items().at(0).displayName, QStringLiteral("Alpha"));
    QCOMPARE(f.svc.items().at(0).notes, QStringLiteral("note"));
    QVERIFY(!f.vm.dirty());
    QVERIFY(f.svc.isDirty());
    // cancel after commit restores the committed state
    f.vm.setNotes("other");
    f.vm.cancel();
    QCOMPARE(f.vm.notes(), QStringLiteral("note"));
}

void ItemViewModelTest::testNewItemCommitAndValidation() {
    Fixture f;
    f.vm.newItem();
    QVERIFY(f.vm.isNew());
    QVERIFY(f.vm.hasItem());
    QVERIFY(!f.vm.valid());
    QCOMPARE(f.vm.validationErrors().size(), 1); // name missing
    QSignalSpy failed(&f.vm, &ItemViewModel::commitFailed);
    QVERIFY(!f.vm.commit());
    QCOMPARE(failed.count(), 1);
    QCOMPARE(f.svc.itemCount(), 2);

    f.vm.setDisplayName("   ");
    QVERIFY(!f.vm.valid()); // whitespace only
    f.vm.setDisplayName("New");
    f.vm.setPassword("pw");
    QVERIFY(f.vm.valid());
    QVERIFY(f.vm.commit());
    QCOMPARE(f.svc.itemCount(), 3);
    QCOMPARE(f.svc.items().at(2).displayName, QStringLiteral("New"));
    QCOMPARE(f.vm.index(), 2);
    QVERIFY(!f.vm.isNew());

    // Read-only database: commit fails with an error message.
    static_cast<FakeDatabaseSource *>(f.svc.source())->setReadOnly(true);
    f.vm.setNotes("x");
    QVERIFY(!f.vm.commit());
    QCOMPARE(failed.count(), 2);
    QCOMPARE(failed.at(1).at(0).toStringList().size(), 1);
}

void ItemViewModelTest::testTotpValidationErrors() {
    Fixture f;
    QVERIFY(f.vm.load(0));
    f.vm.setSecret("1890"); // no Base32 characters at all
    f.vm.setDigits("x");
    f.vm.setInterval("0");
    f.vm.setAlgorithm("MD5");
    QCOMPARE(f.vm.validationErrors().size(), 4);
    QVERIFY(f.vm.totpCode().isEmpty());
    f.vm.setSecret(kSecret);
    f.vm.setDigits("8");
    f.vm.setInterval("60");
    f.vm.setAlgorithm("sha256");
    QVERIFY(f.vm.valid());
    QCOMPARE(f.vm.totpCode().size(), 8);
    f.vm.setSecret(QString()); // 2FA removed: fields no longer validated
    f.vm.setDigits("x");
    QVERIFY(f.vm.valid());
}

void ItemViewModelTest::testDeleteItem() {
    Fixture f;
    f.vm.newItem();
    QVERIFY(!f.vm.deleteItem()); // unsaved
    QVERIFY(f.vm.load(0));
    QSignalSpy deleted(&f.vm, &ItemViewModel::deleted);
    QVERIFY(f.vm.deleteItem());
    QCOMPARE(deleted.count(), 1);
    QCOMPARE(f.svc.itemCount(), 1);
    QCOMPARE(f.svc.items().at(0).displayName, QStringLiteral("B"));
    QVERIFY(!f.vm.hasItem());
}

void ItemViewModelTest::testSafetyChecksAndReuse() {
    Fixture f({makeItem("A", "Shared-Pass-1234!x"), makeItem("B", "Shared-Pass-1234!x"), makeItem("C", "abc")});
    QVERIFY(f.vm.load(2));
    QVariantList c = f.vm.safetyChecks();
    QCOMPARE(c.size(), 8);
    QCOMPARE(c[0].toBool(), true);  // lower
    QCOMPARE(c[1].toBool(), false); // upper
    QCOMPARE(c[2].toBool(), false); // length (min 15)
    QVERIFY(!c[3].isValid());       // leak unknown (no leak service)
    QCOMPARE(c[5].toBool(), false); // digits
    QCOMPARE(c[7].toBool(), true);  // only used by itself
    QCOMPARE(f.vm.leakState(), ItemViewModel::LeakState::Unknown);

    QSignalSpy safety(&f.vm, &ItemViewModel::safetyChanged);
    f.vm.setPassword("Shared-Pass-1234!x"); // used by A and B as well
    QCOMPARE(safety.count(), 1);
    c = f.vm.safetyChecks();
    QCOMPARE(c[1].toBool(), true);
    QCOMPARE(c[2].toBool(), true);
    QCOMPARE(c[4].toBool(), true);
    QCOMPARE(c[5].toBool(), true);
    QCOMPARE(c[7].toBool(), false); // seen before

    f.vm.setPassword(QString());
    for (const QVariant &v : f.vm.safetyChecks())
        QVERIFY(!v.isValid());
}

void ItemViewModelTest::testLeakCheckStates() {
    Fixture f;
    auto net = std::make_shared<FakeRangeClient>();
    net->body = kLeakedBody;
    net->deferred = true;
    LeakCheckService leak(net, f.settings.get(), &f.clock);
    f.vm.setLeakCheckService(&leak);
    QVERIFY(f.vm.load(0));
    f.vm.setPassword("Leaked123");
    QCOMPARE(f.vm.leakState(), ItemViewModel::LeakState::Checking);
    QSignalSpy safety(&f.vm, &ItemViewModel::safetyChanged);
    net->release();
    QTRY_COMPARE(f.vm.leakState(), ItemViewModel::LeakState::Leaked); // delivered queued
    QCOMPARE(safety.count(), 1);
    QCOMPARE(f.vm.safetyChecks()[3].toBool(), false);

    net->deferred = false;
    net->body = "0000000000000000000000000000000000A:1\r\n";
    leak.clearCache();
    f.vm.setPassword("Unleaked-Very-Long-1!");
    QTRY_COMPARE(f.vm.leakState(), ItemViewModel::LeakState::NotLeaked);
    QCOMPARE(f.vm.safetyChecks()[3].toBool(), true);

    net->statusCode = 500; // network error -> unknown
    leak.clearCache();
    f.vm.setPassword("another-pass");
    QTRY_COMPARE(f.vm.leakState(), ItemViewModel::LeakState::Unknown);
    QVERIFY(!f.vm.safetyChecks()[3].isValid());

    f.settings->setVerifyNotLeaked(false); // disabled -> no request
    const int requests = net->requests;
    f.vm.setPassword("yet-another");
    QCOMPARE(net->requests, requests);
    QCOMPARE(f.vm.leakState(), ItemViewModel::LeakState::Unknown);
}

void ItemViewModelTest::testLeakResultForStalePasswordIgnored() {
    Fixture f;
    auto net = std::make_shared<FakeRangeClient>();
    net->body = kLeakedBody;
    net->deferred = true;
    LeakCheckService leak(net, f.settings.get(), &f.clock);
    f.vm.setLeakCheckService(&leak);
    QVERIFY(f.vm.load(0));
    f.vm.setPassword("Leaked123");
    f.vm.setPassword("Different-Pass-99!"); // second request, first result is stale
    net->release();
    QTRY_COMPARE(f.vm.leakState(), ItemViewModel::LeakState::NotLeaked);
    QTest::qWait(20); // the stale "Leaked" result must not overwrite it
    QCOMPARE(f.vm.leakState(), ItemViewModel::LeakState::NotLeaked);
    QCOMPARE(f.vm.safetyChecks()[3].toBool(), true);
}

void ItemViewModelTest::testTotpSharedTimerTicks() {
    PasswordItem withTotp = makeItem("T", "pw");
    withTotp.secret = kSecret;
    Fixture f({withTotp, makeItem("Plain")});
    ItemViewModel second(&f.svc, &f.timer);

    QVERIFY(!f.timer.isRunning());
    QVERIFY(f.vm.load(0));
    QVERIFY(f.timer.isRunning());
    QCOMPARE(f.timer.subscribers(), 1);
    QCOMPARE(f.vm.totpCode(), TotpHelper::generateToken(kSecret, f.clock.now));
    QCOMPARE(f.vm.totpRemaining(), 10); // 1'700'000'000 % 30 == 20
    QVERIFY(f.vm.totpProgress() > 0.0);

    QVERIFY(second.load(0)); // two views share ONE timer
    QCOMPARE(f.timer.subscribers(), 2);

    QSignalSpy totp(&f.vm, &ItemViewModel::totpChanged);
    QSignalSpy totp2(&second, &ItemViewModel::totpChanged);
    f.clock.now += 1;
    f.timer.tickNow();
    QCOMPARE(totp.count(), 1);
    QCOMPARE(totp2.count(), 1);
    QCOMPARE(f.vm.totpRemaining(), 9);
    const QString oldCode = f.vm.totpCode();
    f.clock.now += 9; // next window
    f.timer.tickNow();
    QCOMPARE(f.vm.totpRemaining(), 30);
    QVERIFY(f.vm.totpCode() != oldCode);
    QCOMPARE(f.vm.totpCode(), TotpHelper::generateToken(kSecret, f.clock.now));
    QCOMPARE(second.totpCode(), f.vm.totpCode());

    QVERIFY(f.vm.load(1)); // item without 2FA releases its subscription
    QVERIFY(f.vm.totpCode().isEmpty());
    QCOMPARE(f.timer.subscribers(), 1);
    second.unload();
    QCOMPARE(f.timer.subscribers(), 0);
    QVERIFY(!f.timer.isRunning());
}

void ItemViewModelTest::testApplyOtpauthUri() {
    Fixture f;
    QVERIFY(f.vm.load(0));
    QVERIFY(!f.vm.applyOtpauthUri("https://example.com"));
    QVERIFY(f.vm.applyOtpauthUri(
        "otpauth://totp/Example:alice@example.com?secret=JBSWY3DPEHPK3PXP&issuer=Example&digits=8&period=60&algorithm=SHA256"));
    QCOMPARE(f.vm.secret(), QString::fromLatin1(kSecret));
    QCOMPARE(f.vm.digits(), QStringLiteral("8"));
    QCOMPARE(f.vm.interval(), QStringLiteral("60"));
    QCOMPARE(f.vm.algorithm(), QStringLiteral("SHA256"));
    QVERIFY(f.vm.has2FA());
    QVERIFY(f.vm.dirty());
    QCOMPARE(f.vm.totpCode().size(), 8);
}

void ItemViewModelTest::testCopyActionsUseClipboardAndRegisterClick() {
    PasswordItem item = makeItem("A", "secret-pw");
    item.username = "alice";
    item.email = "a@example.com";
    item.secret = kSecret;
    Fixture f({item});
    auto clip = std::make_shared<FakeClipboard>();
    ClipboardManager manager(clip, f.settings.get(), &f.clock);
    manager.setClearOnDestruction(false);
    ClipboardService service(&manager);
    f.vm.setClipboardService(&service);
    QVERIFY(f.vm.load(0));

    f.vm.copyUsername();
    QCOMPARE(clip->content, QStringLiteral("alice"));
    QVERIFY(!clip->lastSecret);
    f.vm.copyEmail();
    QCOMPARE(clip->content, QStringLiteral("a@example.com"));
    f.vm.copyTotp();
    QCOMPARE(clip->content, f.vm.totpCode());
    QVERIFY(clip->lastSecret);

    QCOMPARE(f.svc.items().at(0).clicks.size(), 0);
    f.vm.copyPassword();
    QCOMPARE(clip->content, QStringLiteral("secret-pw"));
    QVERIFY(clip->lastSecret);
    QCOMPARE(f.svc.items().at(0).clicks.size(), 1); // counts for popularity sort

    // A later commit keeps the registered click.
    f.vm.setNotes("n");
    QVERIFY(f.vm.commit());
    QCOMPARE(f.svc.items().at(0).clicks.size(), 1);
}

void ItemViewModelTest::testUnloadOnLock() {
    PasswordItem withTotp = makeItem("T", "pw");
    withTotp.secret = kSecret;
    Fixture f({withTotp});
    QVERIFY(f.vm.load(0));
    QVERIFY(f.timer.isRunning());
    QSignalSpy loaded(&f.vm, &ItemViewModel::loadedChanged);
    f.svc.lock();
    QCOMPARE(loaded.count(), 1);
    QVERIFY(!f.vm.hasItem());
    QVERIFY(f.vm.password().isEmpty()); // buffer wiped of secrets
    QVERIFY(!f.timer.isRunning());
    QVERIFY(!f.vm.load(0));
}
