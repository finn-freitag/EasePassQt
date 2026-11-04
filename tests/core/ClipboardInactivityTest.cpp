#include "ClipboardInactivityTest.h"

#include "AppSettings.h"
#include "ClipboardManager.h"
#include "IClipboard.h"
#include "IClock.h"
#include "INetworkClient.h"
#include "InactivityManager.h"
#include "WebsiteIconManager.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace EasePass::Core;

namespace {
class FakeClock : public IClock {
public:
    qint64 now = 1'700'000'000;
    qint64 currentSecsSinceEpoch() const override { return now; }
};

class FakeClipboard : public IClipboard {
public:
    QString content;
    bool lastSecret = false;
    int setCount = 0;
    int clearCount = 0;

    QString text() const override { return content; }
    void setText(const QString &text, bool secret) override {
        content = text;
        lastSecret = secret;
        ++setCount;
    }
    void clear() override {
        content.clear();
        ++clearCount;
    }
};

class RecordingNetworkClient : public INetworkClient {
public:
    int requests = 0;
    void get(const QNetworkRequest &, ResponseCallback callback) override {
        ++requests;
        NetworkResponse resp;
        resp.statusCode = 404;
        if (callback)
            callback(resp);
    }
};
} // namespace

// --- Clipboard ---------------------------------------------------------------

void ClipboardInactivityTest::testClipboardClearedAfterTimeout() {
    auto settings = AppSettings::createInMemory();
    settings->setClipboardClearSeconds(10);
    auto cb = std::make_shared<FakeClipboard>();
    FakeClock clock;
    ClipboardManager mgr(cb, settings.get(), &clock);
    QSignalSpy cleared(&mgr, &ClipboardManager::cleared);

    mgr.copy("hunter2");
    QCOMPARE(cb->content, QString("hunter2"));
    QVERIFY(mgr.isClearPending());
    QCOMPARE(mgr.secondsUntilClear(), 10);

    clock.now += 9;
    mgr.checkExpiry();
    QCOMPARE(cb->content, QString("hunter2"));
    QCOMPARE(cleared.count(), 0);

    clock.now += 1;
    mgr.checkExpiry();
    QVERIFY(cb->content.isEmpty());
    QCOMPARE(cb->clearCount, 1);
    QCOMPARE(cleared.count(), 1);
    QVERIFY(!mgr.isClearPending());
    QCOMPARE(mgr.secondsUntilClear(), -1);

    mgr.checkExpiry(); // nothing pending any more
    QCOMPARE(cb->clearCount, 1);
}

void ClipboardInactivityTest::testClipboardNotClearedIfUserCopiedOther() {
    auto settings = AppSettings::createInMemory();
    settings->setClipboardClearSeconds(5);
    auto cb = std::make_shared<FakeClipboard>();
    FakeClock clock;
    ClipboardManager mgr(cb, settings.get(), &clock);
    QSignalSpy skipped(&mgr, &ClipboardManager::clearSkipped);
    QSignalSpy cleared(&mgr, &ClipboardManager::cleared);

    mgr.copy("secret");
    cb->content = "something the user copied"; // external copy

    clock.now += 5;
    mgr.checkExpiry();
    QCOMPARE(cb->content, QString("something the user copied"));
    QCOMPARE(cb->clearCount, 0);
    QCOMPARE(skipped.count(), 1);
    QCOMPARE(cleared.count(), 0);
    QVERIFY(!mgr.isClearPending());
}

void ClipboardInactivityTest::testClipboardRecopyRestartsTimer() {
    auto settings = AppSettings::createInMemory();
    settings->setClipboardClearSeconds(10);
    auto cb = std::make_shared<FakeClipboard>();
    FakeClock clock;
    ClipboardManager mgr(cb, settings.get(), &clock);

    mgr.copy("first");
    clock.now += 8;
    mgr.copy("second"); // restarts the period
    clock.now += 8;
    mgr.checkExpiry();
    QCOMPARE(cb->content, QString("second"));
    QCOMPARE(mgr.secondsUntilClear(), 2);
    clock.now += 2;
    mgr.checkExpiry();
    QVERIFY(cb->content.isEmpty());
}

void ClipboardInactivityTest::testClipboardDisabledWhenZero() {
    auto settings = AppSettings::createInMemory();
    settings->setClipboardClearSeconds(0);
    auto cb = std::make_shared<FakeClipboard>();
    FakeClock clock;
    {
        ClipboardManager mgr(cb, settings.get(), &clock);
        mgr.copy("keep me");
        QVERIFY(!mgr.isClearPending());
        clock.now += 100000;
        mgr.checkExpiry();
        mgr.clearNow();
        QCOMPARE(cb->content, QString("keep me"));
    }
    QCOMPARE(cb->content, QString("keep me")); // not cleared on destruction either
    QCOMPARE(cb->clearCount, 0);
}

void ClipboardInactivityTest::testClipboardSecretFlagAndEmptyText() {
    auto settings = AppSettings::createInMemory();
    auto cb = std::make_shared<FakeClipboard>();
    FakeClock clock;
    ClipboardManager mgr(cb, settings.get(), &clock);
    mgr.setClearOnDestruction(false);

    mgr.copy("pw");
    QVERIFY(cb->lastSecret);
    QCOMPARE(mgr.secondsUntilClear(), AppSettings::Defaults::ClipboardClearSeconds);
    mgr.copy("user", false);
    QVERIFY(!cb->lastSecret);

    const int before = cb->setCount;
    mgr.copy(QString());
    QCOMPARE(cb->setCount, before);
    QCOMPARE(cb->content, QString("user"));
}

void ClipboardInactivityTest::testClipboardClearOnDestruction() {
    auto settings = AppSettings::createInMemory();
    auto cb = std::make_shared<FakeClipboard>();
    FakeClock clock;
    {
        ClipboardManager mgr(cb, settings.get(), &clock);
        mgr.copy("pw");
    }
    QVERIFY(cb->content.isEmpty());
    {
        ClipboardManager mgr(cb, settings.get(), &clock);
        mgr.copy("pw");
        cb->content = "other";
    }
    QCOMPARE(cb->content, QString("other"));
}

// --- Inactivity --------------------------------------------------------------

void ClipboardInactivityTest::testInactivityLogoutAfterTimeout() {
    auto settings = AppSettings::createInMemory();
    settings->setInactivityLogoutMinutes(2);
    FakeClock clock;
    InactivityManager mgr(settings.get(), &clock);
    mgr.setWarningSeconds(0);
    QSignalSpy logout(&mgr, &InactivityManager::logoutRequested);

    mgr.setEnabled(true);
    QCOMPARE(mgr.secondsUntilLogout(), 120);
    clock.now += 119;
    mgr.tick();
    QCOMPARE(logout.count(), 0);
    clock.now += 1;
    mgr.tick();
    QCOMPARE(logout.count(), 1);
    QVERIFY(!mgr.isEnabled());
    clock.now += 500;
    mgr.tick();
    QCOMPARE(logout.count(), 1); // only once
}

void ClipboardInactivityTest::testInactivityActivityResetsTimer() {
    auto settings = AppSettings::createInMemory();
    settings->setInactivityLogoutMinutes(1);
    FakeClock clock;
    InactivityManager mgr(settings.get(), &clock);
    QSignalSpy logout(&mgr, &InactivityManager::logoutRequested);

    mgr.setEnabled(true);
    for (int i = 0; i < 5; ++i) {
        clock.now += 50;
        mgr.tick();
        mgr.reportActivity();
    }
    QCOMPARE(logout.count(), 0);
    QCOMPARE(mgr.secondsUntilLogout(), 60);
    clock.now += 60;
    mgr.tick();
    QCOMPARE(logout.count(), 1);
}

void ClipboardInactivityTest::testInactivityWarningThenLogout() {
    auto settings = AppSettings::createInMemory();
    settings->setInactivityLogoutMinutes(1);
    FakeClock clock;
    InactivityManager mgr(settings.get(), &clock);
    mgr.setWarningSeconds(10);
    QSignalSpy started(&mgr, &InactivityManager::warningStarted);
    QSignalSpy countdown(&mgr, &InactivityManager::warningCountdown);
    QSignalSpy logout(&mgr, &InactivityManager::logoutRequested);

    mgr.setEnabled(true);
    clock.now += 49;
    mgr.tick();
    QCOMPARE(started.count(), 0);
    QVERIFY(!mgr.isWarningActive());

    clock.now += 1; // 10 s left
    mgr.tick();
    QCOMPARE(started.count(), 1);
    QCOMPARE(started.first().first().toInt(), 10);
    QVERIFY(mgr.isWarningActive());
    mgr.tick(); // same second: no duplicate countdown
    QCOMPARE(countdown.count(), 1);

    for (int i = 0; i < 9; ++i) {
        clock.now += 1;
        mgr.tick();
    }
    QCOMPARE(countdown.count(), 10);
    QCOMPARE(countdown.last().first().toInt(), 1);
    QCOMPARE(logout.count(), 0);

    clock.now += 1;
    mgr.tick();
    QCOMPARE(logout.count(), 1);
    QVERIFY(!mgr.isWarningActive());
    QCOMPARE(started.count(), 1);
}

void ClipboardInactivityTest::testInactivityWarningCancelledByActivity() {
    auto settings = AppSettings::createInMemory();
    settings->setInactivityLogoutMinutes(1);
    FakeClock clock;
    InactivityManager mgr(settings.get(), &clock);
    QCOMPARE(mgr.warningSeconds(), InactivityManager::DefaultWarningSeconds);
    QSignalSpy cancelled(&mgr, &InactivityManager::warningCancelled);
    QSignalSpy logout(&mgr, &InactivityManager::logoutRequested);

    mgr.setEnabled(true);
    clock.now += 45;
    mgr.tick();
    QVERIFY(mgr.isWarningActive());
    mgr.reportActivity();
    QVERIFY(!mgr.isWarningActive());
    QCOMPARE(cancelled.count(), 1);
    clock.now += 59;
    mgr.tick();
    QCOMPARE(logout.count(), 0);

    // Warning is capped to half the timeout.
    mgr.setWarningSeconds(1000);
    QCOMPARE(mgr.warningSeconds(), 30);
}

void ClipboardInactivityTest::testInactivityDisabled() {
    auto settings = AppSettings::createInMemory();
    FakeClock clock;
    InactivityManager mgr(settings.get(), &clock);
    QSignalSpy logout(&mgr, &InactivityManager::logoutRequested);
    QSignalSpy warning(&mgr, &InactivityManager::warningStarted);

    QVERIFY(!mgr.isEnabled()); // off until the UI enables it (database unlocked)
    QCOMPARE(mgr.secondsUntilLogout(), -1);
    clock.now += 100000;
    mgr.tick();
    mgr.reportActivity();
    QCOMPARE(logout.count(), 0);

    mgr.setEnabled(true);
    clock.now += mgr.timeoutSeconds() - 5;
    mgr.tick();
    QCOMPARE(warning.count(), 1);
    mgr.setEnabled(false); // e.g. user logged out manually
    QVERIFY(!mgr.isWarningActive());
    clock.now += 100000;
    mgr.tick();
    QCOMPARE(logout.count(), 0);

    // Re-enabling starts a fresh period.
    mgr.setEnabled(true);
    QCOMPARE(mgr.secondsUntilLogout(), mgr.timeoutSeconds());
}

void ClipboardInactivityTest::testInactivityFollowsSettingChange() {
    auto settings = AppSettings::createInMemory();
    settings->setInactivityLogoutMinutes(10);
    FakeClock clock;
    InactivityManager mgr(settings.get(), &clock);
    mgr.setWarningSeconds(0);
    QSignalSpy logout(&mgr, &InactivityManager::logoutRequested);

    mgr.setEnabled(true);
    clock.now += 90;
    mgr.tick();
    QCOMPARE(logout.count(), 0);
    settings->setInactivityLogoutMinutes(1);
    mgr.tick();
    QCOMPARE(logout.count(), 1);

    settings->setInactivityLogoutMinutes(0); // clamped to 1 minute by AppSettings
    QCOMPARE(mgr.timeoutSeconds(), 60);
}

// --- Favicons ----------------------------------------------------------------

void ClipboardInactivityTest::testFaviconFetchDisabledBySetting() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto net = std::make_shared<RecordingNetworkClient>();
    auto settings = AppSettings::createInMemory();
    QVERIFY(!settings->fetchFavicons());

    WebsiteIconManager mgr(net);
    mgr.setCacheDir(dir.path());
    mgr.setSettings(settings.get());

    QVERIFY(mgr.getIcon("https://example.com").isNull());
    mgr.requestIcon("example.com");
    QCOMPARE(net->requests, 0);

    settings->setFetchFavicons(true);
    mgr.requestIcon("example.com");
    QCOMPARE(net->requests, 2); // direct + fallback (both 404)
}

void ClipboardInactivityTest::testFaviconFetchDefaultOffWithoutSettings() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto net = std::make_shared<RecordingNetworkClient>();
    WebsiteIconManager mgr(net);
    mgr.setCacheDir(dir.path());
    QVERIFY(!mgr.fetchingAllowed());
    mgr.getIcon("https://example.com");
    QCOMPARE(net->requests, 0);
}
