#include "LeakCheckServiceTest.h"

#include "AppSettings.h"
#include "IClock.h"
#include "INetworkClient.h"
#include "LeakCheckService.h"

#include <QSignalSpy>
#include <QTest>
#include <optional>

using namespace EasePass::Core;
using Status = LeakCheckService::Status;

namespace {
// SHA-1("password") = 5BAA61E4C9B93F3F0682250B6CF8331B7EE68FD8
const QByteArray kPasswordPrefix = "5BAA6";
const QByteArray kPasswordSuffix = "1E4C9B93F3F0682250B6CF8331B7EE68FD8";

class FakeClock : public IClock {
public:
    qint64 now = 1'700'000'000;
    qint64 currentSecsSinceEpoch() const override { return now; }
};

class FakeRangeClient : public INetworkClient {
public:
    NetworkResponse response;
    bool deferred = false;
    QList<QNetworkRequest> requests;
    QList<ResponseCallback> pendingCallbacks;

    void get(const QNetworkRequest &request, ResponseCallback callback) override {
        requests.append(request);
        if (deferred)
            pendingCallbacks.append(std::move(callback));
        else
            callback(response);
    }
    void release() {
        auto cbs = std::move(pendingCallbacks);
        pendingCallbacks.clear();
        for (auto &cb : cbs)
            cb(response);
    }
};

NetworkResponse ok(const QByteArray &body) {
    NetworkResponse r;
    r.statusCode = 200;
    r.data = body;
    return r;
}

QByteArray rangeBody(bool includePassword, int count = 3861493) {
    QByteArray body = "0018A45C4D1DEF81644B54AB7F969B88D65:1\r\n"
                      "00D4F6E8FA6EECAD2A3AA415EEC418D38EC:0\r\n"; // padding entry
    if (includePassword)
        body += kPasswordSuffix + ":" + QByteArray::number(count) + "\r\n";
    body += "011053FD0102E94D6AE2F8B83D76FAF94F6:13\r\n";
    return body;
}

struct Fixture {
    std::shared_ptr<AppSettings> settings = AppSettings::createInMemory();
    std::shared_ptr<FakeRangeClient> net = std::make_shared<FakeRangeClient>();
    FakeClock clock;
    LeakCheckService service{net, settings.get(), &clock};

    Fixture() { settings->setVerifyNotLeaked(true); }

    LeakCheckService::Result run(const SecretString &pw) {
        std::optional<LeakCheckService::Result> result;
        service.check(pw, [&](const LeakCheckService::Result &r) { result = r; });
        // callbacks are always queued
        for (int i = 0; i < 200 && !result; ++i)
            QTest::qWait(5);
        return result.value_or(LeakCheckService::Result{});
    }
};
} // namespace

void LeakCheckServiceTest::testSha1HexAndSuffixParsing() {
    QCOMPARE(LeakCheckService::sha1Hex(SecretString("password")), kPasswordPrefix + kPasswordSuffix);
    bool okFlag = false;
    QCOMPARE(LeakCheckService::countForSuffix(rangeBody(true, 42), kPasswordSuffix, &okFlag), 42);
    QVERIFY(okFlag);
    QCOMPARE(LeakCheckService::countForSuffix(rangeBody(false), kPasswordSuffix, &okFlag), 0);
    QVERIFY(okFlag);
    QCOMPARE(LeakCheckService::countForSuffix(rangeBody(true, 7), kPasswordSuffix.toLower()), 7);
    QCOMPARE(LeakCheckService::countForSuffix("<html>oops</html>", kPasswordSuffix, &okFlag), 0);
    QVERIFY(!okFlag);
}

void LeakCheckServiceTest::testLeakedPassword() {
    Fixture f;
    f.net->response = ok(rangeBody(true));
    QSignalSpy finished(&f.service, &LeakCheckService::checkFinished);
    auto r = f.run("password");
    QCOMPARE(r.status, Status::Leaked);
    QCOMPARE(r.count, 3861493);
    QCOMPARE(finished.count(), 1);
}

void LeakCheckServiceTest::testNotLeakedPassword() {
    Fixture f;
    f.net->response = ok(rangeBody(false));
    auto r = f.run("password");
    QCOMPARE(r.status, Status::NotLeaked);
    QCOMPARE(r.count, 0);
}

void LeakCheckServiceTest::testOnlyPrefixIsSent() {
    Fixture f;
    f.net->response = ok(rangeBody(true));
    f.run("password");
    QCOMPARE(f.net->requests.size(), 1);
    const QNetworkRequest &req = f.net->requests.first();
    QCOMPARE(req.url(), QUrl("https://api.pwnedpasswords.com/range/5BAA6"));
    QVERIFY(!req.url().toString().contains(kPasswordSuffix.left(6)));
    QCOMPARE(req.url().path().section(QLatin1Char('/'), -1), QString("5BAA6")); // 5 chars only
    QCOMPARE(req.rawHeader("Add-Padding"), QByteArray("true"));
    for (const QByteArray &h : req.rawHeaderList()) {
        QVERIFY(!req.rawHeader(h).contains(kPasswordSuffix.left(6)));
    }
}

void LeakCheckServiceTest::testNetworkErrorGivesUnknown() {
    Fixture f;
    f.net->response.statusCode = 0;
    f.net->response.errorString = "Host not found";
    auto r = f.run("password");
    QCOMPARE(r.status, Status::Unknown);
    QCOMPARE(r.errorString, QString("Host not found"));

    f.net->response = NetworkResponse{};
    f.net->response.statusCode = 503;
    r = f.run("password");
    QCOMPARE(r.status, Status::Unknown);
    QVERIFY(r.errorString.contains("503"));
    QCOMPARE(f.net->requests.size(), 2); // errors are not cached
}

void LeakCheckServiceTest::testMalformedResponseGivesUnknown() {
    Fixture f;
    f.net->response = ok("<html>captive portal</html>");
    QCOMPARE(f.run("password").status, Status::Unknown);
    // A malformed body that still contains the suffix counts as leaked (positive evidence).
    f.service.clearCache();
    f.net->response = ok("garbage\n" + kPasswordSuffix + ":5\n");
    QCOMPARE(f.run("password").status, Status::Leaked);
}

void LeakCheckServiceTest::testCacheHitAvoidsRequest() {
    Fixture f;
    f.net->response = ok(rangeBody(true));
    QCOMPARE(f.run("password").status, Status::Leaked);
    QCOMPARE(f.run("password").status, Status::Leaked);
    QCOMPARE(f.net->requests.size(), 1);
}

void LeakCheckServiceTest::testCacheExpires() {
    Fixture f;
    f.service.setCacheTtlSeconds(100);
    f.net->response = ok(rangeBody(false));
    f.run("password");
    f.clock.now += 99;
    f.run("password");
    QCOMPARE(f.net->requests.size(), 1);
    f.clock.now += 1;
    f.net->response = ok(rangeBody(true));
    QCOMPARE(f.run("password").status, Status::Leaked);
    QCOMPARE(f.net->requests.size(), 2);
}

void LeakCheckServiceTest::testConcurrentChecksShareRequest() {
    Fixture f;
    f.net->deferred = true;
    f.net->response = ok(rangeBody(true));
    QList<Status> results;
    f.service.check("password", [&](const auto &r) { results.append(r.status); });
    f.service.check("password", [&](const auto &r) { results.append(r.status); });
    QCOMPARE(f.net->requests.size(), 1);
    f.net->release();
    QTRY_COMPARE(results.size(), 2);
    QCOMPARE(results, (QList<Status>{Status::Leaked, Status::Leaked}));
}

void LeakCheckServiceTest::testDisabledBySettingNoRequest() {
    Fixture f;
    f.settings->setVerifyNotLeaked(false);
    QVERIFY(!f.service.isEnabled());
    auto r = f.run("password");
    QCOMPARE(r.status, Status::Unknown);
    QVERIFY(!r.errorString.isEmpty());
    QCOMPARE(f.net->requests.size(), 0);
}

void LeakCheckServiceTest::testEmptyPasswordUnknown() {
    Fixture f;
    QCOMPARE(f.run(SecretString()).status, Status::Unknown);
    QCOMPARE(f.net->requests.size(), 0);
}
