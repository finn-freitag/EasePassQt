#include "PluginTests.h"

#include "PluginCatalog.h"
#include "PluginTestSupport.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

// Catalog client (steps.txt T28, 4.3): PluginList.json validation, fetch with timeout and
// cache, and the verification states verified / unverified / offline / revoked.

using namespace EasePass::Plugin;
using namespace PluginTest;

namespace {

const QString CatalogUrl = QStringLiteral("https://catalog.example.test/PluginList.json");
const QByteArray HashA(64, 'a');
const QByteArray HashB(64, 'b');
const QByteArray HashC(64, 'c');

QByteArray readFile(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

struct CatalogFixture {
    QTemporaryDir directory;
    std::shared_ptr<FakeHttp> http = std::make_shared<FakeHttp>();
    std::unique_ptr<CatalogClient> client;

    explicit CatalogFixture(int timeoutMs = 5000, const QString &url = CatalogUrl) { reopen(timeoutMs, url); }

    /// New client on the same cache directory (like an application restart).
    void reopen(int timeoutMs = 5000, const QString &url = CatalogUrl) {
        CatalogClient::Options options;
        options.url = QUrl(url);
        options.cacheDirectory = directory.path();
        options.timeoutMs = timeoutMs;
        client = std::make_unique<CatalogClient>(http, options);
    }

    void serve(const QByteArray &catalog) { http->replies[CatalogUrl] = {200, catalog, {}}; }

    /// refresh() and wait; returns its result, `error` gets the message.
    bool refresh(QString *error = nullptr) {
        bool done = false, result = false;
        client->refresh([&](bool ok, const QString &message) {
            done = true;
            result = ok;
            if (error)
                *error = message;
        });
        if (!waitUntil([&] { return done; }, 10'000))
            qWarning("refresh did not finish");
        return result;
    }
};

} // namespace

void PluginCatalogTest::testParseSpecSamples() {
    const QString base = QStringLiteral(EASEPASS_TEST_FIXTURES_DIR "/plugins/catalog/");
    const QStringList valid = QDir(base + QStringLiteral("valid")).entryList({QStringLiteral("*.json")});
    const QStringList invalid = QDir(base + QStringLiteral("invalid")).entryList({QStringLiteral("*.json")});
    QVERIFY(valid.size() >= 2);
    QVERIFY(invalid.size() >= 9);
    for (const QString &name : valid) {
        QString error;
        QVERIFY2(PluginCatalogData::parse(readFile(base + QStringLiteral("valid/") + name), &error).has_value(),
                 qPrintable(name + QStringLiteral(": ") + error));
    }
    for (const QString &name : invalid) {
        QString error;
        QVERIFY2(!PluginCatalogData::parse(readFile(base + QStringLiteral("invalid/") + name), &error).has_value(),
                 qPrintable(name));
        QVERIFY2(error.startsWith(QLatin1String("plugin list: ")), qPrintable(name + QStringLiteral(": ") + error));
    }
}

void PluginCatalogTest::testParseFields() {
    const QByteArray json = catalogJson({
        catalogEntryJson(QStringLiteral("org.example.one"), HashA,
                         QStringLiteral(R"("os": ["windows", "macos"], "icon_url": "https://example.test/i.png", )"
                                        R"("sensitive": true, "min_app_version": "2.0.0", "revoked": true, )"
                                        R"("some_future_property": {"x": 1})"),
                         QStringLiteral("1.2.3-beta.1")),
        catalogEntryJson(QStringLiteral("org.example.two"), HashB),
    });
    QString error;
    const auto data = PluginCatalogData::parse(json, &error);
    QVERIFY2(data.has_value(), qPrintable(error));
    QCOMPARE(data->updated, QDateTime(QDate(2026, 11, 1), QTime(0, 0), QTimeZone::UTC));
    QCOMPARE(data->plugins.size(), 2);

    const CatalogEntry &one = data->plugins.at(0);
    QCOMPARE(one.id, QStringLiteral("org.example.one"));
    QCOMPARE(one.name, QStringLiteral("Catalog org.example.one"));
    QCOMPARE(one.author, QStringLiteral("Catalog author"));
    QCOMPARE(one.version, QStringLiteral("1.2.3-beta.1"));
    QCOMPARE(one.apiVersion, 1);
    QCOMPARE(one.os, QStringList({QStringLiteral("windows"), QStringLiteral("macos")}));
    QCOMPARE(one.downloadUrl, QUrl(downloadUrl(QStringLiteral("org.example.one"))));
    QCOMPARE(one.iconUrl, QUrl(QStringLiteral("https://example.test/i.png")));
    QCOMPARE(one.sha256, HashA);
    QVERIFY(one.sensitive);
    QVERIFY(one.revoked);
    QCOMPARE(one.minAppVersion, QStringLiteral("2.0.0"));
    QVERIFY(one.supportsOs(QStringLiteral("windows")));
    QVERIFY(!one.supportsOs(QStringLiteral("linux")));

    // Optional members missing: all operating systems, not sensitive, not revoked.
    const CatalogEntry &two = data->plugins.at(1);
    QVERIFY(two.os.isEmpty());
    QVERIFY(two.supportsOs(QStringLiteral("linux")));
    QVERIFY(!two.sensitive);
    QVERIFY(!two.revoked);
    QVERIFY(two.iconUrl.isEmpty());
    QVERIFY(two.feed.isEmpty());
}

void PluginCatalogTest::testParseRejects_data() {
    QTest::addColumn<QByteArray>("json");
    const QString id = QStringLiteral("org.example.plugin");
    const auto entry = [&](const QString &extra) { return catalogJson({catalogEntryJson(id, HashA, extra)}); };
    // A member given twice: Qt keeps the LAST one, so `extra` overrides the valid default.
    QTest::newRow("not json") << QByteArray("PluginList");
    QTest::newRow("array") << QByteArray("[]");
    QTest::newRow("schema 2") << QByteArray(R"({"schema": 2, "updated": "2026-11-01T00:00:00Z", "plugins": []})");
    QTest::newRow("schema missing") << QByteArray(R"({"updated": "2026-11-01T00:00:00Z", "plugins": []})");
    QTest::newRow("schema as text") << QByteArray(R"({"schema": "1", "updated": "2026-11-01T00:00:00Z", "plugins": []})");
    QTest::newRow("updated missing") << QByteArray(R"({"schema": 1, "plugins": []})");
    QTest::newRow("updated not UTC") << QByteArray(R"({"schema": 1, "updated": "2026-11-01T00:00:00+01:00", "plugins": []})");
    QTest::newRow("updated impossible") << QByteArray(R"({"schema": 1, "updated": "2026-13-41T00:00:00Z", "plugins": []})");
    QTest::newRow("plugins missing") << QByteArray(R"({"schema": 1, "updated": "2026-11-01T00:00:00Z"})");
    QTest::newRow("plugins not a list") << QByteArray(R"({"schema": 1, "updated": "2026-11-01T00:00:00Z", "plugins": {}})");
    QTest::newRow("entry not an object") << catalogJson({"\"text\""});
    QTest::newRow("hash too short") << catalogJson({catalogEntryJson(id, QByteArray(63, 'a'))});
    QTest::newRow("hash too long") << catalogJson({catalogEntryJson(id, QByteArray(65, 'a'))});
    QTest::newRow("hash upper case") << catalogJson({catalogEntryJson(id, QByteArray(64, 'A'))});
    QTest::newRow("hash not hex") << catalogJson({catalogEntryJson(id, QByteArray(64, 'g'))});
    QTest::newRow("hash missing") << QByteArray(
        R"({"schema": 1, "updated": "2026-11-01T00:00:00Z", "plugins": [{"id": "org.example.plugin", "name": "n", )"
        R"("description": "", "author": "", "version": "1.0.0", "api_version": 1, )"
        R"("download_url": "https://example.test/p.epplugin"}]})");
    QTest::newRow("url missing") << QByteArray(
        R"({"schema": 1, "updated": "2026-11-01T00:00:00Z", "plugins": [{"id": "org.example.plugin", "name": "n", )"
        R"("description": "", "author": "", "version": "1.0.0", "api_version": 1, "sha256": ")" + HashA + R"("}]})");
    QTest::newRow("url http") << entry(QStringLiteral(R"("download_url": "http://example.test/p.epplugin")"));
    QTest::newRow("url file") << entry(QStringLiteral(R"("download_url": "file:///etc/passwd")"));
    QTest::newRow("url without host") << entry(QStringLiteral(R"("download_url": "https://")"));
    QTest::newRow("url with user") << entry(QStringLiteral(R"("download_url": "https://user:pw@example.test/p")"));
    QTest::newRow("url not a string") << entry(QStringLiteral(R"("download_url": 5)"));
    QTest::newRow("icon url http") << entry(QStringLiteral(R"("icon_url": "http://example.test/i.png")"));
    QTest::newRow("id invalid") << catalogJson({catalogEntryJson(QStringLiteral("../evil"), HashA)});
    QTest::newRow("name empty") << entry(QStringLiteral(R"("name": "")"));
    QTest::newRow("name too long") << entry(QStringLiteral(R"("name": "%1")").arg(QString(81, QLatin1Char('n'))));
    QTest::newRow("version invalid") << catalogJson({catalogEntryJson(id, HashA, {}, QStringLiteral("1.0"))});
    QTest::newRow("api version 0") << entry(QStringLiteral(R"("api_version": 0)"));
    QTest::newRow("api version text") << entry(QStringLiteral(R"("api_version": "1")"));
    QTest::newRow("os unknown") << entry(QStringLiteral(R"("os": ["linux", "beos"])"));
    QTest::newRow("os twice") << entry(QStringLiteral(R"("os": ["linux", "linux"])"));
    QTest::newRow("os not a list") << entry(QStringLiteral(R"("os": "linux")"));
    QTest::newRow("revoked not boolean") << entry(QStringLiteral(R"("revoked": "no")"));
    QTest::newRow("sensitive not boolean") << entry(QStringLiteral(R"("sensitive": 1)"));
    QTest::newRow("min app version invalid") << entry(QStringLiteral(R"("min_app_version": "2")"));
    // One bad entry spoils the whole list (schema validation), even after good ones.
    QTest::newRow("second entry bad") << catalogJson({catalogEntryJson(id, HashA),
                                                      catalogEntryJson(QStringLiteral("org.example.b"), "abc")});
    QTest::newRow("too large") << QByteArray(PluginCatalogData::MaxBytes + 1, ' ');
}

void PluginCatalogTest::testParseRejects() {
    QFETCH(QByteArray, json);
    QString error;
    QVERIFY(!PluginCatalogData::parse(json, &error).has_value());
    QVERIFY2(error.startsWith(QLatin1String("plugin list: ")), qPrintable(error));
}

void PluginCatalogTest::testRefreshGoesLiveAndWritesCache() {
    CatalogFixture f;
    QCOMPARE(f.client->state(), CatalogClient::State::Empty);
    QVERIFY(f.client->entries().isEmpty());

    const QByteArray catalog = catalogJson({catalogEntryJson(QStringLiteral("org.example.one"), HashA)});
    f.serve(catalog);
    int changes = 0;
    QObject::connect(f.client.get(), &CatalogClient::changed, f.client.get(), [&] { ++changes; });
    QString error;
    QVERIFY2(f.refresh(&error), qPrintable(error));
    QCOMPARE(f.client->state(), CatalogClient::State::Live);
    QVERIFY(!f.client->isRefreshing());
    QVERIFY(changes >= 2); // loading, done
    QCOMPARE(f.client->entries().size(), 1);
    QCOMPARE(f.client->entries().constFirst().id, QStringLiteral("org.example.one"));
    QCOMPARE(f.http->requested, QStringList{CatalogUrl});
    // The exact answer is cached.
    QCOMPARE(readFile(f.directory.filePath(QStringLiteral("PluginList.json"))), catalog);

    // Two refreshes at the same time share one request.
    f.http->requested.clear();
    int done = 0;
    f.client->refresh([&](bool, const QString &) { ++done; });
    f.client->refresh([&](bool, const QString &) { ++done; });
    QVERIFY(waitUntil([&] { return done == 2; }));
    QCOMPARE(f.http->requested.size(), 1);
}

void PluginCatalogTest::testVerificationStates() {
    CatalogFixture f;
    // Nothing fetched yet: offline, whatever the hash.
    QCOMPARE(f.client->verify(HashA), PluginVerification::Offline);

    f.serve(catalogJson({catalogEntryJson(QStringLiteral("org.example.good"), HashA),
                         catalogEntryJson(QStringLiteral("org.example.bad"), HashB,
                                          QStringLiteral(R"("revoked": true)"))}));
    QVERIFY(f.refresh());
    QCOMPARE(f.client->verify(HashA), PluginVerification::Verified);
    QCOMPARE(f.client->verify(HashB), PluginVerification::Revoked);
    QCOMPARE(f.client->verify(HashC), PluginVerification::Unverified);
    QVERIFY(f.client->officialEntryByHash(HashA).has_value());
    QVERIFY(!f.client->officialEntryByHash(HashC).has_value());

    // The catalog becomes unreachable: a known hash is no longer "verified", an unknown one
    // is not "unverified" - both are "offline". Only the revocation stays.
    f.http->replies[CatalogUrl] = {0, {}, QStringLiteral("host not found")};
    QString error;
    QVERIFY(!f.refresh(&error));
    QCOMPARE(error, QStringLiteral("host not found"));
    QCOMPARE(f.client->state(), CatalogClient::State::Cached);
    QCOMPARE(f.client->verify(HashA), PluginVerification::Offline);
    QCOMPARE(f.client->verify(HashC), PluginVerification::Offline);
    QCOMPARE(f.client->verify(HashB), PluginVerification::Revoked);

    // HTTP errors count as unreachable too.
    f.http->replies[CatalogUrl] = {503, "busy", {}};
    QVERIFY(!f.refresh());
    QCOMPARE(f.client->verify(HashA), PluginVerification::Offline);

    // Reachable again.
    f.serve(catalogJson({catalogEntryJson(QStringLiteral("org.example.good"), HashA)}));
    QVERIFY(f.refresh());
    QCOMPARE(f.client->verify(HashA), PluginVerification::Verified);
    QCOMPARE(f.client->verify(HashB), PluginVerification::Unverified); // not listed any more
}

void PluginCatalogTest::testCacheIsForDisplayOnly() {
    CatalogFixture f;
    f.serve(catalogJson({catalogEntryJson(QStringLiteral("org.example.good"), HashA),
                         catalogEntryJson(QStringLiteral("org.example.bad"), HashB,
                                          QStringLiteral(R"("revoked": true)"))}));
    QVERIFY(f.refresh());

    // "Restart" without network: the store can show the last list, but nothing is verified.
    f.http->replies.clear();
    f.http->requested.clear();
    f.reopen();
    QCOMPARE(f.client->state(), CatalogClient::State::Cached);
    QCOMPARE(f.client->entries().size(), 2);
    QVERIFY(f.http->requested.isEmpty()); // reading the cache needs no network
    QCOMPARE(f.client->verify(HashA), PluginVerification::Offline);
    QCOMPARE(f.client->verify(HashB), PluginVerification::Revoked);
    QVERIFY(!f.refresh());
    QCOMPARE(f.client->state(), CatalogClient::State::Cached);
    QCOMPARE(f.client->entries().size(), 2);

    // A damaged cache file is ignored.
    QFile cache(f.directory.filePath(QStringLiteral("PluginList.json")));
    QVERIFY(cache.open(QIODevice::WriteOnly));
    cache.write("{ damaged");
    cache.close();
    f.reopen();
    QCOMPARE(f.client->state(), CatalogClient::State::Empty);
    QVERIFY(f.client->entries().isEmpty());
}

void PluginCatalogTest::testTimeout() {
    CatalogFixture f(150);
    f.http->silent = true; // the server never answers
    QElapsedTimer timer;
    timer.start();
    QString error;
    QVERIFY(!f.refresh(&error));
    QCOMPARE(error, QStringLiteral("timeout"));
    QVERIFY(timer.elapsed() < 5000);
    QVERIFY(!f.client->isRefreshing());
    QCOMPARE(f.client->state(), CatalogClient::State::Empty);
    QCOMPARE(f.client->verify(HashA), PluginVerification::Offline);

    // The client still works afterwards.
    f.http->silent = false;
    f.serve(catalogJson({}));
    QVERIFY(f.refresh());
    QCOMPARE(f.client->state(), CatalogClient::State::Live);
}

void PluginCatalogTest::testInvalidAnswerKeepsLastCatalog() {
    CatalogFixture f;
    const QByteArray good = catalogJson({catalogEntryJson(QStringLiteral("org.example.good"), HashA)});
    f.serve(good);
    QVERIFY(f.refresh());

    // A list with a bad hash length is rejected as a whole; the old one stays for display.
    f.serve(catalogJson({catalogEntryJson(QStringLiteral("org.example.good"), HashA),
                         catalogEntryJson(QStringLiteral("org.example.evil"), QByteArray(40, 'a'))}));
    QString error;
    QVERIFY(!f.refresh(&error));
    QVERIFY2(error.contains(QLatin1String("sha256")), qPrintable(error));
    QCOMPARE(f.client->state(), CatalogClient::State::Cached);
    QCOMPARE(f.client->entries().size(), 1);
    QCOMPARE(f.client->verify(HashA), PluginVerification::Offline);
    QCOMPARE(readFile(f.directory.filePath(QStringLiteral("PluginList.json"))), good);

    // Too large an answer is not even parsed.
    f.serve(QByteArray(PluginCatalogData::MaxBytes + 1, ' '));
    QVERIFY(!f.refresh(&error));
    QCOMPARE(error, QStringLiteral("too large"));
}

void PluginCatalogTest::testHttpsOnly() {
    const QString plain = QStringLiteral("http://catalog.example.test/PluginList.json");
    CatalogFixture f(5000, plain);
    f.http->replies[plain] = {200, catalogJson({}), {}};
    QString error;
    QVERIFY(!f.refresh(&error));
    QCOMPARE(error, QStringLiteral("not an https URL"));
    QVERIFY(f.http->requested.isEmpty()); // no request was sent at all

    QVERIFY(CatalogClient::isHttps(QUrl(QStringLiteral("https://example.test/a"))));
    QVERIFY(!CatalogClient::isHttps(QUrl(QStringLiteral("http://example.test/a"))));
    QVERIFY(!CatalogClient::isHttps(QUrl(QStringLiteral("https://user@example.test/a"))));
    QVERIFY(!CatalogClient::isHttps(QUrl(QStringLiteral("file:///tmp/a"))));
    QVERIFY(!CatalogClient::isHttps(QUrl()));
}

void PluginCatalogTest::testFeedsNeverVerify() {
    CatalogFixture f;
    const QString feedUrl = QStringLiteral("https://feed.example.test/list.json");
    const QString brokenUrl = QStringLiteral("https://broken.example.test/list.json");
    f.serve(catalogJson({catalogEntryJson(QStringLiteral("org.example.official"), HashA)}));
    f.http->replies[feedUrl] = {200, catalogJson({catalogEntryJson(QStringLiteral("org.example.third"), HashB)}), {}};
    f.http->replies[brokenUrl] = {200, "not a catalog", {}};

    f.client->setFeeds({{QStringLiteral("Community"), QUrl(feedUrl)},
                        {QStringLiteral("Broken"), QUrl(brokenUrl)},
                        {QStringLiteral("Duplicate"), QUrl(feedUrl)},                // same URL twice
                        {QStringLiteral("Official again"), QUrl(CatalogUrl)},       // the official one
                        {QStringLiteral("Plain"), QUrl(QStringLiteral("http://feed.example.test/list.json"))}});
    QVERIFY(f.refresh()); // a broken feed does not fail the refresh
    QCOMPARE(f.http->requested, QStringList({CatalogUrl, feedUrl, brokenUrl}));

    const QList<CatalogEntry> entries = f.client->entries();
    QCOMPARE(entries.size(), 2);
    QCOMPARE(entries.at(0).id, QStringLiteral("org.example.official"));
    QVERIFY(entries.at(0).feed.isEmpty());
    QCOMPARE(entries.at(1).id, QStringLiteral("org.example.third"));
    QCOMPARE(entries.at(1).feed, QStringLiteral("Community"));

    // Only the official catalog makes a package "verified".
    QCOMPARE(f.client->verify(HashA), PluginVerification::Verified);
    QCOMPARE(f.client->verify(HashB), PluginVerification::Unverified);
    QVERIFY(!f.client->officialEntryByHash(HashB).has_value());

    // A feed cannot revoke either... nor survive a failed refresh.
    f.http->replies[CatalogUrl] = {500, {}, {}};
    QVERIFY(!f.refresh());
    QCOMPARE(f.client->entries().size(), 1);
}

void PluginCatalogTest::testDownload() {
    CatalogFixture f;
    const QString id = QStringLiteral("org.example.one");
    f.serve(catalogJson({catalogEntryJson(id, HashA)}));
    QVERIFY(f.refresh());
    const CatalogEntry entry = f.client->entries().constFirst();

    const auto download = [&](QString *error) {
        bool done = false;
        QByteArray result;
        f.client->download(entry, [&](const QByteArray &bytes, const QString &message) {
            done = true;
            result = bytes;
            *error = message;
        });
        waitUntil([&] { return done; });
        return result;
    };

    QString error;
    f.http->replies[downloadUrl(id)] = {200, "package bytes", {}};
    QCOMPARE(download(&error), QByteArray("package bytes"));
    QVERIFY(error.isEmpty());

    f.http->replies[downloadUrl(id)] = {404, "missing", {}};
    QVERIFY(download(&error).isEmpty());
    QCOMPARE(error, QStringLiteral("HTTP status 404"));

    f.http->replies[downloadUrl(id)] = {200, QByteArray(PluginPackage::MaxPackageBytes + 1, 'x'), {}};
    QVERIFY(download(&error).isEmpty());
    QCOMPARE(error, QStringLiteral("too large"));
}

void PluginCatalogTest::testNetworkClientSizeCap() {
    // The real network client stops a transfer that grows beyond the requested cap
    // (the catalog client sets it for the list and for packages).
    TestServer server;
    EasePass::Core::QtNetworkClient client;
    const QUrl big(QStringLiteral("http://127.0.0.1:%1/big").arg(server.httpPort())); // 5000 bytes

    const auto get = [&](qint64 cap) {
        QNetworkRequest request(big);
        if (cap > 0)
            request.setAttribute(EasePass::Core::INetworkClient::MaxResponseBytesAttribute, cap);
        bool done = false;
        EasePass::Core::NetworkResponse result;
        client.get(request, [&](const EasePass::Core::NetworkResponse &response) {
            done = true;
            result = response;
        });
        waitUntil([&] { return done; });
        return result;
    };

    const EasePass::Core::NetworkResponse capped = get(1000);
    QVERIFY(!capped.isSuccess());
    QCOMPARE(capped.errorString, QStringLiteral("response too large"));
    QVERIFY(capped.data.isEmpty());

    const EasePass::Core::NetworkResponse fits = get(5000);
    QVERIFY2(fits.isSuccess(), qPrintable(fits.errorString));
    QCOMPARE(fits.data.size(), 5000);

    QCOMPARE(get(0).data.size(), 5000); // no cap
}
