#include "PluginTests.h"

#include "PluginAdapters.h"
#include "PluginCatalog.h"
#include "PluginInspector.h"
#include "PluginManagerFixture.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSslCertificate>
#include <QTest>

// The official plugins (steps.txt T29) as they are published in plugins/: checked against
// plugins/PluginList.json and run in the real plugin host. Their protocol code has its own
// tests in the plugins' repositories; here the question is whether the PACKAGES work in
// Ease Pass.

using namespace EasePass::Core;
using namespace EasePass::Plugin;
using namespace PluginTest;

namespace {

const QString UrlId = QStringLiteral("com.finnfreitag.easepass.url-database");
const QString CloudId = QStringLiteral("com.finnfreitag.easepass.cloud");
const QString FtpId = QStringLiteral("com.finnfreitag.easepass.ftp-database");

QByteArray officialFile(const QString &name) {
    QFile file(QStringLiteral(EASEPASS_OFFICIAL_PLUGINS_DIR "/") + name);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

struct Running {
    std::shared_ptr<PluginWorker> worker;
    std::unique_ptr<PluginDatabaseProvider> provider;

    PluginError setConfig(const QJsonObject &config) {
        bool done = false;
        PluginError result;
        QObject context;
        provider->setConfig(QString::fromUtf8(QJsonDocument(config).toJson(QJsonDocument::Compact)), &context,
                            [&](const PluginError &error) {
                                result = error;
                                done = true;
                            });
        if (!waitUntil([&] { return done; }, 60'000))
            result = {PluginError::Kind::Host, {}, QStringLiteral("test: no answer")};
        return result;
    }
    QList<std::shared_ptr<PluginDatabaseSource>> databases(PluginError *error = nullptr) {
        bool done = false;
        QList<std::shared_ptr<PluginDatabaseSource>> result;
        QObject context;
        provider->listDatabases(&context, [&](const QList<std::shared_ptr<PluginDatabaseSource>> &sources,
                                              const PluginError &listError) {
            result = sources;
            if (error)
                *error = listError;
            done = true;
        });
        waitUntil([&] { return done; }, 60'000);
        return result;
    }
    QString text(void (PluginDatabaseProvider::*getter)(QObject *, PluginCallback<QString>)) {
        bool done = false;
        QString result;
        QObject context;
        (provider.get()->*getter)(&context, [&](const QString &value, const PluginError &) {
            result = value;
            done = true;
        });
        waitUntil([&] { return done; }, 60'000);
        return result;
    }
};

/// Installs an official package the way the application does and allows it to reach the
/// local test servers (in the application that is the "local network" switch).
Running start(ManagerFixture &fixture, const QString &id) {
    Running running;
    const PluginResult installed = fixture.install(officialFile(id + QStringLiteral(".epplugin")));
    if (!installed.ok()) {
        qWarning("install failed: %s", qPrintable(installed.message));
        return running;
    }
    fixture.manager->setAllowPrivateNetwork(id, true);
    PluginResult result;
    running.worker = fixture.manager->load(id, &result);
    if (!running.worker) {
        qWarning("load failed: %s", qPrintable(result.message));
        return running;
    }
    running.provider = std::make_unique<PluginDatabaseProvider>(running.worker);
    return running;
}

} // namespace

void OfficialPluginsTest::testListMatchesPackages() {
    QString error;
    const auto list = PluginCatalogData::parse(officialFile(QStringLiteral("PluginList.json")), &error);
    QVERIFY2(list.has_value(), qPrintable(error));
    QCOMPARE(list->plugins.size(), 3);
    QStringList ids;
    for (const CatalogEntry &entry : list->plugins) {
        ids << entry.id;
        const QByteArray bytes = officialFile(entry.id + QStringLiteral(".epplugin"));
        QVERIFY2(!bytes.isEmpty(), qPrintable(entry.id));
        // What the application compares before it calls a package "verified".
        QCOMPARE(PluginPackage::hash(bytes), entry.sha256);
        const auto package = PluginPackage::parse(bytes, &error);
        QVERIFY2(package.has_value(), qPrintable(error));
        QCOMPARE(package->manifest.id, entry.id);
        QCOMPARE(package->manifest.version, entry.version);
        QCOMPARE(package->manifest.name, entry.name);
        QVERIFY(!package->icon.isEmpty());
        QVERIFY(!entry.revoked);
        QVERIFY(entry.supportsOs(QStringLiteral("linux")) && entry.supportsOs(QStringLiteral("windows")) &&
                entry.supportsOs(QStringLiteral("macos")));
        QVERIFY(entry.downloadUrl.toString().endsWith(QLatin1Char('/') + entry.id + QStringLiteral(".epplugin")));

        // Computed from the code, not from the manifest: what the install dialog shows.
        const PluginInspection inspection = PluginInspector::inspect(package->wasm);
        QVERIFY2(inspection.accepted(), qPrintable(inspection.rejected.join(QLatin1String("; "))));
        QCOMPARE(inspection.types, QStringList{QStringLiteral("database-source")});
        QCOMPARE(inspection.permissions, (QStringList{QStringLiteral("storage"), QStringLiteral("network")}));
        // The user chooses the server, so the plugins may connect anywhere (extra warning).
        QCOMPARE(package->manifest.networkHosts, QStringList{QStringLiteral("*:*")});

        // And the application installs it.
        ManagerFixture fixture;
        QVERIFY(fixture.install(bytes).ok());
    }
    ids.sort();
    QCOMPARE(ids, (QStringList{CloudId, FtpId, UrlId}));
}

void OfficialPluginsTest::testUrlPlugin() {
    TestServer server;
    ManagerFixture fixture;
    Running plugin = start(fixture, UrlId);
    QVERIFY(plugin.worker);
    QVERIFY(QJsonDocument::fromJson(plugin.text(&PluginDatabaseProvider::configSchema).toUtf8()).isObject());
    QVERIFY(plugin.databases().isEmpty());

    const QString url = server.url(QStringLiteral("/files/vault.epdb"));
    QVERIFY(plugin.setConfig({{"address", url + QStringLiteral("?token=abc")}, {"offline_copy", true}}).ok());
    const auto databases = plugin.databases();
    QCOMPARE(databases.size(), 1);
    PluginDatabaseSource *source = databases[0].get();
    QCOMPARE(source->databaseName(), QStringLiteral("vault"));
    QVERIFY(source->isReadOnly());
    QVERIFY2(!source->sourceDescription().contains(QLatin1String("token")), qPrintable(source->sourceDescription()));
    QVERIFY(server.requests().isEmpty()); // listing does not use the network

    // Nothing at the address yet.
    QVERIFY(!source->getBytes().has_value());
    QCOMPARE(source->lastError().code, QStringLiteral("not-found"));

    QByteArray database;
    for (int i = 0; i < 200'000; ++i)
        database.append(char(i * 31 + i / 251));
    server.setFile(database);
    QCOMPARE(source->getBytes().value_or(QByteArray()), database);
    QCOMPARE(server.requests().constLast(), QStringLiteral("GET /files/vault.epdb?token=abc"));

    // The largest database a plugin can carry arrives; a larger file is refused with a
    // clear message instead of Wasmtime's data limit ending the call.
    server.setFile(QByteArray(int(PluginLimits::MaxDatabaseBytes), 'x'));
    QCOMPARE(source->getBytes().value_or(QByteArray()).size(), PluginLimits::MaxDatabaseBytes);
    server.setFile(QByteArray(int(PluginLimits::MaxDatabaseBytes) + 1, 'x'));
    QVERIFY(!source->getBytes().has_value());
    QCOMPARE(source->lastError().code, QStringLiteral("too-large"));
    QVERIFY2(source->lastError().message.contains(QLatin1String("2 MiB")), qPrintable(source->lastError().message));
    server.setFile(database);
    QCOMPARE(source->getBytes().value_or(QByteArray()), database); // the plugin still runs

    // Read-only: nothing is ever sent to the server.
    const qsizetype before = server.requests().size();
    QVERIFY(!source->saveBytes("changed"));
    QCOMPARE(server.requests().size(), before);

    // The address stops answering: the kept copy opens instead.
    server.setFile(QByteArray());
    QCOMPARE(source->getBytes().value_or(QByteArray()), database);

    // What the plugin refuses.
    QCOMPARE(plugin.setConfig({{"address", QStringLiteral("ftp://example.org/x.epdb")}, {"offline_copy", false}}).code,
             QStringLiteral("invalid-argument"));
    QCOMPARE(plugin.databases().size(), 1);
}

void OfficialPluginsTest::testCloudPlugin() {
    TestServer server;
    ManagerFixture fixture;
    Running plugin = start(fixture, CloudId);
    QVERIFY(plugin.worker);
    QVERIFY(plugin.databases().isEmpty());

    // The "Ease Pass Config" of the web interface, pasted as it is.
    const QString host = QStringLiteral("http://allowed.conformance.test:%1").arg(server.httpPort());
    const QJsonObject pasted{{"Host", host}, {"SaveReadonlyOfflineCopies", false}, {"AccessTokens", QJsonArray{"test-token"}}};
    QVERIFY(plugin.setConfig({{"paste", QString::fromUtf8(QJsonDocument(pasted).toJson(QJsonDocument::Compact))},
                              {"host", QString()},
                              {"tokens", QString()},
                              {"offline_copies", false}})
                .ok());
    const QJsonObject stored = QJsonDocument::fromJson(plugin.text(&PluginDatabaseProvider::config).toUtf8()).object();
    QCOMPARE(stored.value("host").toString(), host);
    QCOMPARE(stored.value("tokens").toString(), QStringLiteral("test-token"));
    QCOMPARE(stored.value("paste").toString(), QString());

    auto databases = plugin.databases();
    QCOMPARE(databases.size(), 1);
    PluginDatabaseSource *source = databases[0].get();
    QCOMPARE(source->databaseName(), QStringLiteral("Team vault"));
    QCOMPARE(source->sourceDescription(), host);
    QVERIFY(!source->isReadOnly());
    QCOMPARE(source->availability(), IDatabaseSource::Availability::Available);
    QCOMPARE(source->lastModified().toSecsSinceEpoch(), qint64(1769722517));

    // Open, log in (locks the database on the server for others), save, log out.
    QCOMPARE(source->getBytes().value_or(QByteArray()), QByteArray("cloud database v1"));
    source->login();
    QVERIFY(server.cloudLocked());
    QVERIFY(!source->isReadOnly()); // our own lock
    QByteArray changed;
    for (int i = 0; i < 100'000; ++i)
        changed.append(char(i * 13));
    QVERIFY(source->saveBytes(changed));
    QCOMPARE(server.cloudDatabase(), changed);
    source->logout();
    QVERIFY(!server.cloudLocked());

    // Someone else holds the lock: read-only, and nothing is uploaded.
    server.setCloudLocked(true);
    databases = plugin.databases();
    QVERIFY(databases[0]->isReadOnly());
    QCOMPARE(databases[0]->getBytes().value_or(QByteArray()), changed);
    QVERIFY(!databases[0]->saveBytes("overwritten"));
    databases[0]->login();
    databases[0]->logout();
    QVERIFY(server.cloudLocked()); // their lock is not ours to remove
    QCOMPARE(server.cloudDatabase(), changed);
    server.setCloudLocked(false);

    // A token the server does not know.
    QVERIFY(plugin.setConfig({{"paste", QString()}, {"host", host}, {"tokens", QStringLiteral("wrong")}, {"offline_copies", false}}).ok());
    databases = plugin.databases();
    QCOMPARE(databases.size(), 1);
    QCOMPARE(databases[0]->availability(), IDatabaseSource::Availability::Unavailable);
    QVERIFY(!databases[0]->getBytes().has_value());
    QCOMPARE(databases[0]->lastError().code, QStringLiteral("not-allowed"));
}

void OfficialPluginsTest::testFtpPluginSettings() {
    ManagerFixture fixture;
    Running plugin = start(fixture, FtpId);
    QVERIFY(plugin.worker);
    const QJsonObject schema = QJsonDocument::fromJson(plugin.text(&PluginDatabaseProvider::configSchema).toUtf8()).object();
    QCOMPARE(schema.value("properties").toObject().size(), 7);
    QVERIFY(plugin.databases().isEmpty());
    // The untouched form can be saved and still means "no database".
    const QJsonObject untouched = QJsonDocument::fromJson(plugin.text(&PluginDatabaseProvider::config).toUtf8()).object();
    QVERIFY(plugin.setConfig(untouched).ok());
    QVERIFY(plugin.databases().isEmpty());

    QJsonObject config{{"mode", "SFTP"}, {"host", "sftp.example.com"}, {"port", 0},        {"username", "me"},
                       {"password", "secret"}, {"remote_path", "/home/me/Family.epdb"}, {"server_key", ""}};
    QVERIFY(plugin.setConfig(config).ok());
    const auto databases = plugin.databases();
    QCOMPARE(databases.size(), 1);
    QCOMPARE(databases[0]->databaseName(), QStringLiteral("Family (SFTP)"));
    QCOMPARE(databases[0]->sourceDescription(), QStringLiteral("sftp://sftp.example.com:22/home/me/Family.epdb"));
    QVERIFY(!databases[0]->isReadOnly());
    QCOMPARE(databases[0]->availability(), IDatabaseSource::Availability::UnknownState);

    config["username"] = QStringLiteral("me\r\nDELE x"); // would be a second FTP command
    QCOMPARE(plugin.setConfig(config).code, QStringLiteral("invalid-argument"));

    // Without the "local network" switch a plugin does not reach this computer or the LAN.
    config["username"] = QStringLiteral("me");
    config["host"] = QStringLiteral("allowed.conformance.test");
    QVERIFY(plugin.setConfig(config).ok());
    plugin.worker.reset();
    plugin.provider.reset();
    QVERIFY(fixture.manager->setAllowPrivateNetwork(FtpId, false).ok());
    fixture.manager->unload(FtpId);
    plugin.worker = fixture.manager->load(FtpId);
    QVERIFY(plugin.worker);
    plugin.provider = std::make_unique<PluginDatabaseProvider>(plugin.worker);
    const auto blocked = plugin.databases();
    QCOMPARE(blocked.size(), 1);
    QVERIFY(!blocked[0]->getBytes().has_value());
    QCOMPARE(blocked[0]->lastError().code, QStringLiteral("not-allowed"));
}

// The packaged plugin against real servers: FTP, FTPS (encrypted control and data
// connection) and SFTP. Needs the servers of the plugin's repository:
//
//   cd EasePassQTDatabaseSourcePlugin && nix develop
//   mkdir -p /tmp/ep/root && python3 core/tests/servers.py /tmp/ep/root        (prints JSON)
//   EASEPASS_TEST_FTP_SERVERS='<that JSON>' EASEPASS_TEST_FTP_ROOT=/tmp/ep/root ctest -R testFtpPluginAgainstServers
void OfficialPluginsTest::testFtpPluginAgainstServers() {
    const QJsonObject servers = QJsonDocument::fromJson(qgetenv("EASEPASS_TEST_FTP_SERVERS")).object();
    const QString root = qEnvironmentVariable("EASEPASS_TEST_FTP_ROOT");
    if (servers.isEmpty() || root.isEmpty())
        QSKIP("set EASEPASS_TEST_FTP_SERVERS and EASEPASS_TEST_FTP_ROOT (see the comment above this test)");

    ManagerFixture fixture;
    // The FTPS test server has a self-signed certificate for ftps.conformance.test.
    fixture.environment.extraCaCertificates = QSslCertificate::fromPath(QDir(root).filePath(QStringLiteral("../ftps-cert.pem")));
    fixture.reopen();
    Running plugin = start(fixture, FtpId);
    QVERIFY(plugin.worker);

    const QString file = QDir(root).filePath(QStringLiteral("vault.epdb"));
    const auto write = [&](const QByteArray &content) {
        QFile out(file);
        return out.open(QIODevice::WriteOnly | QIODevice::Truncate) && out.write(content) == content.size();
    };
    const auto read = [&] {
        QFile in(file);
        return in.open(QIODevice::ReadOnly) ? in.readAll() : QByteArray();
    };
    QByteArray large;
    for (int i = 0; i < int(PluginLimits::MaxDatabaseBytes) - 8; ++i) // the largest database a plugin can carry
        large.append(char(i * 7 + i / 255));

    struct Case {
        QString mode;
        int port;
        QString host;
    };
    QList<Case> cases{{QStringLiteral("FTP"), servers.value("ftp").toInt(), QStringLiteral("allowed.conformance.test")},
                      {QStringLiteral("SFTP"), servers.value("sftp").toObject().value("default").toObject().value("port").toInt(),
                       QStringLiteral("allowed.conformance.test")}};
    if (servers.contains("ftps") && !fixture.environment.extraCaCertificates.isEmpty())
        cases << Case{QStringLiteral("FTPS"), servers.value("ftps").toInt(), QStringLiteral("ftps.conformance.test")};
    else
        qWarning("no FTPS server or certificate: FTPS is not tested");

    for (const Case &c : cases) {
        qInfo("-- %s", qPrintable(c.mode));
        QVERIFY(plugin.setConfig({{"mode", c.mode}, {"host", c.host}, {"port", c.port}, {"username", servers.value("user").toString()},
                                  {"password", servers.value("password").toString()}, {"remote_path", "/vault.epdb"},
                                  {"server_key", ""}})
                    .ok());
        const auto databases = plugin.databases();
        QCOMPARE(databases.size(), 1);
        PluginDatabaseSource *source = databases[0].get();
        for (const QByteArray &content : {QByteArray("small database"), large}) {
            QVERIFY(write(content));
            const auto loaded = source->getBytes();
            QVERIFY2(loaded.has_value(), qPrintable(c.mode + QStringLiteral(": ") + source->lastError().message));
            QCOMPARE(*loaded, content);
            const QByteArray replacement = content + "+changed";
            QVERIFY2(source->saveBytes(replacement), qPrintable(c.mode + QStringLiteral(": ") + source->lastError().message));
            QCOMPARE(read(), replacement);
            QCOMPARE(QDir(root).entryList(QDir::Files), QStringList{QStringLiteral("vault.epdb")}); // no temporary file left
        }
        if (c.mode == QLatin1String("SFTP")) {
            // The server's key was remembered at the first login.
            const QJsonObject stored = QJsonDocument::fromJson(plugin.text(&PluginDatabaseProvider::config).toUtf8()).object();
            QCOMPARE(stored.value("server_key").toString(),
                     servers.value("sftp").toObject().value("default").toObject().value("fingerprint").toString());
        }
        // A wrong password is refused by the server and reported.
        QVERIFY(plugin.setConfig({{"mode", c.mode}, {"host", c.host}, {"port", c.port}, {"username", servers.value("user").toString()},
                                  {"password", "wrong"}, {"remote_path", "/vault.epdb"}, {"server_key", ""}})
                    .ok());
        const auto wrong = plugin.databases();
        QVERIFY(!wrong[0]->getBytes().has_value());
        QCOMPARE(wrong[0]->lastError().code, QStringLiteral("not-allowed"));
    }
}
