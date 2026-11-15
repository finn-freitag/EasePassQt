#include "PluginTests.h"

#include "AppSettings.h"
#include "ISettingsStore.h"
#include "LeakCheckService.h"
#include "PluginManagerFixture.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>
#include <thread>

using namespace EasePass::Plugin;
using namespace PluginTest;
using Code = PluginResult::Code;

namespace {

const QString ImporterId = QStringLiteral("org.easepass.conformance.importer");
const QString DatabaseId = QStringLiteral("org.easepass.conformance.database-source");
const QString NetworkId = QStringLiteral("org.easepass.conformance.network");
const QString DatabaseInterface = QStringLiteral("easepass:plugin/database-source");
const QString ImporterInterface = QStringLiteral("easepass:plugin/importer");

QByteArray readFile(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

bool writeFile(const QString &path, const QByteArray &data) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(data) == data.size();
}

/// write-database("main", data) through the plugin; returns true on success.
bool writeDatabase(PluginWorker &worker, const QByteArray &data) {
    PluginError error;
    const Value raw = callSync(worker, DatabaseInterface, QStringLiteral("write-database"),
                               {Value::string(QStringLiteral("main")), Value::bytes(data)}, &error);
    return error.ok() && raw.isOk();
}

QByteArray readDatabase(PluginWorker &worker, PluginError *error) {
    const Value raw = callSync(worker, DatabaseInterface, QStringLiteral("read-database"),
                               {Value::string(QStringLiteral("main"))}, error);
    return error->ok() ? PluginError::unwrap(raw, error).toBytes() : QByteArray();
}

/// import-passwords(source) of an importer plugin, first item.
Value importFirst(PluginWorker &worker, const QString &source, PluginError *error) {
    const Value raw = callSync(worker, ImporterInterface, QStringLiteral("import-passwords"), {Value::string(source)}, error);
    return error->ok() ? PluginError::unwrap(raw, error).items().value(0) : Value();
}

QString installedJson(const ManagerFixture &fixture) {
    return QDir(fixture.environment.pluginsDirectory).filePath(QStringLiteral("installed.json"));
}

} // namespace

// A worker can outlive the manager (a queued callback or an open database holds it). Its
// thread must not: a plugin still being compiled when the process ends crashed it.
void PluginManagerTest::testManagerStopsThePluginsItStarted() {
    ManagerFixture fixture;
    std::shared_ptr<PluginWorker> worker = fixture.installAndLoad(QStringLiteral("importer"));
    QVERIFY(worker);
    bool answered = false;
    QObject context;
    // Like the adapters: the callback keeps the worker alive until it has run.
    worker->call(ImporterInterface, QStringLiteral("sources"), {}, 0, &context,
                 [&, keep = worker](const Value &, const PluginError &) { answered = true; });
    // The manager goes away at once: the plugin has not even been compiled yet.
    fixture.manager.reset();
    PluginError error;
    worker->callBlocking(ImporterInterface, QStringLiteral("sources"), {}, 0, &error);
    QCOMPARE(error.kind, PluginError::Kind::Unusable); // stopped, and this did not wait forever
    bool late = false;
    worker->call(ImporterInterface, QStringLiteral("sources"), {}, 0, &context,
                 [&](const Value &, const PluginError &lateError) { late = lateError.kind == PluginError::Kind::Unusable; });
    worker->start(&context, [](const PluginError &) {});
    // The call that was already queued got its answer (finished or interrupted, depending
    // on how far it was), and the worker's thread has ended.
    QVERIFY(waitUntil([&] { return answered && late; }));

    // unload() stops a plugin the same way; loading again gives a fresh, working one.
    fixture.reopen();
    const auto first = fixture.manager->load(ImporterId);
    QVERIFY(first);
    callSync(*first, ImporterInterface, QStringLiteral("sources"), {}, &error);
    QVERIFY2(error.ok(), qPrintable(error.message));
    fixture.manager->unload(ImporterId);
    first->callBlocking(ImporterInterface, QStringLiteral("sources"), {}, 0, &error);
    QCOMPARE(error.kind, PluginError::Kind::Unusable);
    const auto second = fixture.manager->load(ImporterId);
    QVERIFY(second && second != first);
    callSync(*second, ImporterInterface, QStringLiteral("sources"), {}, &error);
    QVERIFY2(error.ok(), qPrintable(error.message));
}

void PluginManagerTest::testInstallFromStore() {
    ManagerFixture fixture;
    FakeCatalog catalog;
    const QByteArray bytes = conformancePackage(QStringLiteral("importer"));
    const QByteArray hash = PluginPackage::hash(bytes);
    catalog.known.insert(hash, PluginVerification::Verified);
    fixture.manager->setCatalog(&catalog);
    QSignalSpy changed(fixture.manager.get(), &PluginManager::pluginsChanged);

    // Step 1: nothing is written, the dialog data is computed from the component.
    InstallCandidate candidate;
    PluginResult result = fixture.manager->prepareInstall(bytes, &candidate, hash);
    QVERIFY2(result.ok(), qPrintable(result.message));
    QCOMPARE(candidate.verification, PluginVerification::Verified);
    QCOMPARE(candidate.inspection.types, QStringList{QStringLiteral("importer")});
    QCOMPARE(candidate.inspection.permissions, QStringList{QStringLiteral("pick-file")});
    QCOMPARE(candidate.package.manifest.id, ImporterId);
    QVERIFY(!candidate.isUpdate);
    QVERIFY(fixture.manager->installedPlugins().isEmpty());
    QVERIFY(!QFileInfo::exists(fixture.environment.pluginsDirectory));
    QCOMPARE(changed.count(), 0);

    // Step 2: after the user confirmed.
    result = fixture.manager->install(candidate, {QStringLiteral("pick-file")});
    QVERIFY2(result.ok(), qPrintable(result.message));
    QCOMPARE(changed.count(), 1);
    const std::optional<InstalledPlugin> record = fixture.manager->plugin(ImporterId);
    QVERIFY(record.has_value());
    QCOMPARE(record->name, QStringLiteral("Conformance: importer"));
    QCOMPARE(record->version, QStringLiteral("1.0.0"));
    QCOMPARE(record->sha256, hash);
    QCOMPARE(record->types, QStringList{QStringLiteral("importer")});
    QCOMPARE(record->permissions, QStringList{QStringLiteral("pick-file")});
    QCOMPARE(record->approvedPermissions, QStringList{QStringLiteral("pick-file")});
    QCOMPARE(record->verification, PluginVerification::Verified);
    QVERIFY(record->enabled);
    QVERIFY(!record->allowPrivateNetwork);
    QVERIFY(record->installedAt.isValid());
    QCOMPARE(readFile(fixture.manager->packagePath(ImporterId)), bytes); // stored unchanged

    // The installed plugin runs.
    PluginResult loadResult;
    const auto worker = fixture.manager->load(ImporterId, &loadResult);
    QVERIFY2(worker, qPrintable(loadResult.message));
    QCOMPARE(fixture.manager->load(ImporterId), worker); // loaded once
    PluginError error;
    QCOMPARE(importFirst(*worker, QStringLiteral("fixed"), &error).field("display-name").toString(),
             QStringLiteral("Example"));
    QVERIFY2(error.ok(), qPrintable(error.message));
    QVERIFY(fixture.log().contains(ImporterId + QStringLiteral("|info|conformance-importer: importing fixed items")));
}

void PluginManagerTest::testHashMismatchAborts() {
    ManagerFixture fixture;
    const QByteArray bytes = conformancePackage(QStringLiteral("importer"));
    const QByteArray hash = PluginPackage::hash(bytes);
    InstallCandidate candidate;

    // The catalog entry names another file.
    PluginResult result = fixture.manager->prepareInstall(
        bytes, &candidate, PluginPackage::hash(conformancePackage(QStringLiteral("about"))));
    QCOMPARE(result.code, Code::HashMismatch);

    // The download was modified on the way (one byte differs).
    QByteArray modified = bytes;
    modified[modified.size() / 2] = char(modified[modified.size() / 2] ^ 0x01);
    QCOMPARE(fixture.manager->prepareInstall(modified, &candidate, hash).code, Code::HashMismatch);
    // The mismatch is found before the file is parsed: garbage gives the same answer.
    QCOMPARE(fixture.manager->prepareInstall("garbage", &candidate, hash).code, Code::HashMismatch);

    QVERIFY(candidate.packageBytes.isEmpty());
    QVERIFY(fixture.manager->installedPlugins().isEmpty());
    QVERIFY(!QFileInfo::exists(fixture.manager->packagePath(ImporterId)));

    // The hash comparison ignores the letter case of the catalog entry.
    QVERIFY(fixture.manager->prepareInstall(bytes, &candidate, hash.toUpper()).ok());

    // A candidate whose bytes were swapped after the check is caught by install().
    candidate.packageBytes = conformancePackage(QStringLiteral("about"));
    QCOMPARE(fixture.manager->install(candidate, {QStringLiteral("pick-file")}).code, Code::HashMismatch);
    QVERIFY(fixture.manager->installedPlugins().isEmpty());
}

void PluginManagerTest::testLocalInstallVerificationStates() {
    ManagerFixture fixture;
    const QByteArray bytes = conformancePackage(QStringLiteral("store-feed"));
    const QByteArray hash = PluginPackage::hash(bytes);
    const QString id = QStringLiteral("org.easepass.conformance.store-feed");
    InstallCandidate candidate;

    // No catalog at all: could not verify.
    QVERIFY(fixture.manager->prepareInstall(bytes, &candidate).ok());
    QCOMPARE(candidate.verification, PluginVerification::Offline);

    FakeCatalog catalog;
    fixture.manager->setCatalog(&catalog);
    // Catalog reachable, hash unknown: "not officially verified" (still installable).
    QVERIFY(fixture.manager->prepareInstall(bytes, &candidate).ok());
    QCOMPARE(candidate.verification, PluginVerification::Unverified);
    QVERIFY(fixture.manager->install(candidate, {}).ok());
    QCOMPARE(fixture.manager->plugin(id)->verification, PluginVerification::Unverified);

    catalog.offline = true;
    QVERIFY(fixture.manager->prepareInstall(bytes, &candidate).ok());
    QCOMPARE(candidate.verification, PluginVerification::Offline);

    catalog.offline = false;
    catalog.known.insert(hash, PluginVerification::Verified);
    QVERIFY(fixture.manager->prepareInstall(bytes, &candidate).ok());
    QCOMPARE(candidate.verification, PluginVerification::Verified);
    QVERIFY(candidate.isUpdate);

    // Revoked: blocked, also for a candidate that was prepared before the revocation.
    catalog.known.insert(hash, PluginVerification::Revoked);
    InstallCandidate blocked;
    QCOMPARE(fixture.manager->prepareInstall(bytes, &blocked).code, Code::Revoked);
    QCOMPARE(fixture.manager->install(candidate, {}).code, Code::Revoked);
    QCOMPARE(fixture.manager->plugin(id)->verification, PluginVerification::Unverified); // unchanged record
}

void PluginManagerTest::testInstallNeedsApproval() {
    ManagerFixture fixture;
    InstallCandidate candidate;
    QVERIFY(fixture.manager->prepareInstall(conformancePackage(QStringLiteral("importer")), &candidate).ok());

    QCOMPARE(fixture.manager->install(candidate, {}).code, Code::NotApproved);
    QCOMPARE(fixture.manager->install(candidate, {QStringLiteral("storage"), QStringLiteral("network")}).code,
             Code::NotApproved);
    QVERIFY(fixture.manager->installedPlugins().isEmpty());
    QVERIFY(!QFileInfo::exists(fixture.manager->packagePath(ImporterId)));

    // Approving more than needed never widens what the plugin gets.
    QVERIFY(fixture.manager->install(candidate, {QStringLiteral("pick-file"), QStringLiteral("network"),
                                                 QStringLiteral("storage")}).ok());
    QCOMPARE(fixture.manager->plugin(ImporterId)->approvedPermissions, QStringList{QStringLiteral("pick-file")});
}

void PluginManagerTest::testBadPackagesRejected_data() {
    QTest::addColumn<QByteArray>("bytes");
    QTest::addColumn<int>("code");

    const QString id = QStringLiteral("com.example.bad");
    const QByteArray importerWasm = component(QStringLiteral("importer"));
    const QString otherOs = PluginManifest::currentOs() == QLatin1String("windows") ? QStringLiteral("linux")
                                                                                    : QStringLiteral("windows");

    QTest::newRow("forbidden import") << makePackage(manifestJson(id), fixture(QStringLiteral("negative/forbidden-import.wasm")))
                                      << int(Code::Rejected);
    QTest::newRow("unknown namespace") << makePackage(manifestJson(id), fixture(QStringLiteral("negative/unknown-namespace.wasm")))
                                       << int(Code::Rejected);
    QTest::newRow("not a component") << makePackage(manifestJson(id), "garbage") << int(Code::Rejected);
    QTest::newRow("manifest hides a permission")
        << makePackage(manifestJson(id, QStringLiteral(R"("permissions": [])")), importerWasm) << int(Code::Rejected);
    QTest::newRow("manifest claims another permission")
        << makePackage(manifestJson(id, QStringLiteral(R"("permissions": ["storage"])")), importerWasm)
        << int(Code::Rejected);
    QTest::newRow("manifest claims more")
        << makePackage(manifestJson(id, QStringLiteral(R"("permissions": ["pick-file", "network"])")), importerWasm)
        << int(Code::Rejected);
    QTest::newRow("api version 2") << makePackage(QString::fromUtf8(manifestJson(id)).replace("\"api_version\": 1", "\"api_version\": 2").toUtf8(), importerWasm)
                                   << int(Code::Unsupported);
    QTest::newRow("other operating system")
        << makePackage(manifestJson(id, QStringLiteral(R"("os": ["%1"])").arg(otherOs)), importerWasm)
        << int(Code::Unsupported);
    QTest::newRow("application too old")
        << makePackage(manifestJson(id, QStringLiteral(R"("min_app_version": "99.0.0")")), importerWasm)
        << int(Code::Unsupported);
    QTest::newRow("not a zip") << QByteArray("not a package") << int(Code::InvalidPackage);
    QTest::newRow("invalid manifest") << makePackage("{}", importerWasm) << int(Code::InvalidPackage);
    QTest::newRow("too large") << QByteArray(PluginPackage::MaxPackageBytes + 1, 'x') << int(Code::TooLarge);
}

void PluginManagerTest::testBadPackagesRejected() {
    QFETCH(QByteArray, bytes);
    QFETCH(int, code);

    ManagerFixture fixture;
    InstallCandidate candidate;
    const PluginResult result = fixture.manager->prepareInstall(bytes, &candidate);
    QCOMPARE(int(result.code), code);
    QVERIFY(!result.message.isEmpty());
    QVERIFY(candidate.packageBytes.isEmpty());
    // install() repeats every check, so a hand-made candidate does not get through either.
    candidate.packageBytes = bytes;
    candidate.package.sha256 = PluginPackage::hash(bytes);
    QCOMPARE(int(fixture.manager->install(candidate, {QStringLiteral("storage"), QStringLiteral("pick-file"),
                                                      QStringLiteral("network"), QStringLiteral("leak-check")}).code),
             code);
    QVERIFY(fixture.manager->installedPlugins().isEmpty());

    // The valid counterpart of these packages installs fine (the test data is not broken otherwise).
    QVERIFY(fixture.install(makePackage(manifestJson(QStringLiteral("com.example.good"),
                                                     QStringLiteral(R"("permissions": ["pick-file"], "os": ["%1"], "min_app_version": "1.4.0")")
                                                         .arg(PluginManifest::currentOs())),
                                        component(QStringLiteral("importer"))))
                .ok());
}

void PluginManagerTest::testRecordsPersist() {
    ManagerFixture fixture;
    const QByteArray icon("\x89PNG\r\n\x1a\nfake icon");
    const QString iconId = QStringLiteral("com.example.with-icon");
    QVERIFY(fixture.install(conformancePackage(QStringLiteral("network"))).ok());
    QVERIFY(fixture.install(makePackage(manifestJson(iconId, QStringLiteral(R"("author": "Jane", "homepage": "https://example.com")")),
                                        component(QStringLiteral("about")), icon))
                .ok());
    const QList<InstalledPlugin> before = fixture.manager->installedPlugins();
    QCOMPARE(before.size(), 2);

    fixture.reopen(); // "restart"
    const QList<InstalledPlugin> after = fixture.manager->installedPlugins();
    QCOMPARE(after.size(), 2);
    for (int i = 0; i < 2; ++i) {
        QCOMPARE(after[i].id, before[i].id);
        QCOMPARE(after[i].name, before[i].name);
        QCOMPARE(after[i].sha256, before[i].sha256);
        QCOMPARE(after[i].types, before[i].types);
        QCOMPARE(after[i].permissions, before[i].permissions);
        QCOMPARE(after[i].approvedPermissions, before[i].approvedPermissions);
        QCOMPARE(after[i].networkHosts, before[i].networkHosts);
        QCOMPARE(after[i].enabled, before[i].enabled);
        QCOMPARE(after[i].verification, before[i].verification);
        QCOMPARE(after[i].installedAt.toSecsSinceEpoch(), before[i].installedAt.toSecsSinceEpoch());
    }
    const InstalledPlugin network = *fixture.manager->plugin(NetworkId);
    QCOMPARE(network.permissions, QStringList{QStringLiteral("network")});
    QCOMPARE(network.networkHosts.size(), 4);
    QVERIFY(network.networkHosts.contains(QStringLiteral("port.conformance.test:8080")));
    const InstalledPlugin withIcon = *fixture.manager->plugin(iconId);
    QCOMPARE(withIcon.author, QStringLiteral("Jane"));
    QCOMPARE(withIcon.homepage, QStringLiteral("https://example.com"));
    QCOMPARE(withIcon.types, (QStringList{QStringLiteral("about"), QStringLiteral("initializer")}));
    QCOMPARE(fixture.manager->icon(iconId), icon);
    QVERIFY(fixture.manager->icon(NetworkId).isEmpty());
    QVERIFY(fixture.manager->icon(QStringLiteral("com.example.unknown")).isEmpty());

    // Records with an id that is not a plain plugin id are ignored (the id becomes a path).
    QJsonObject root = QJsonDocument::fromJson(readFile(installedJson(fixture))).object();
    QJsonArray records = root.value(QStringLiteral("plugins")).toArray();
    QJsonObject evil = records.at(0).toObject();
    evil.insert(QStringLiteral("id"), QStringLiteral("../../../outside"));
    records.append(evil);
    root.insert(QStringLiteral("plugins"), records);
    QVERIFY(writeFile(installedJson(fixture), QJsonDocument(root).toJson()));
    fixture.reopen();
    QCOMPARE(fixture.manager->installedPlugins().size(), 2);

    // A damaged list means "nothing installed", not a crash.
    QVERIFY(writeFile(installedJson(fixture), "{ broken"));
    fixture.reopen();
    QVERIFY(fixture.manager->installedPlugins().isEmpty());
}

void PluginManagerTest::testTamperedFileRefusedAtLoad() {
    ManagerFixture fixture;
    const QByteArray original = conformancePackage(QStringLiteral("importer"));
    QVERIFY(fixture.install(original).ok());
    const QString path = fixture.manager->packagePath(ImporterId);
    QVERIFY(fixture.manager->load(ImporterId));
    fixture.manager->unload(ImporterId);
    PluginResult result;

    // One byte appended.
    QVERIFY(writeFile(path, original + "x"));
    QVERIFY(!fixture.manager->load(ImporterId, &result));
    QCOMPARE(result.code, Code::Tampered);

    // Replaced by another valid, harmless-looking package.
    QVERIFY(writeFile(path, conformancePackage(QStringLiteral("about"))));
    QVERIFY(!fixture.manager->load(ImporterId, &result));
    QCOMPARE(result.code, Code::Tampered);

    // Replaced by a package with the same manifest but another component.
    QVERIFY(writeFile(path, makePackage(manifestJson(ImporterId), component(QStringLiteral("network")))));
    QVERIFY(!fixture.manager->load(ImporterId, &result));
    QCOMPARE(result.code, Code::Tampered);

    // Deleted.
    QVERIFY(QFile::remove(path));
    QVERIFY(!fixture.manager->load(ImporterId, &result));
    QCOMPARE(result.code, Code::Tampered);

    // The check also holds after a restart, and the original file loads again.
    QVERIFY(writeFile(path, original + "x"));
    fixture.reopen();
    QVERIFY(!fixture.manager->load(ImporterId, &result));
    QCOMPARE(result.code, Code::Tampered);
    QVERIFY(writeFile(path, original));
    QVERIFY2(fixture.manager->load(ImporterId, &result), qPrintable(result.message));
}

void PluginManagerTest::testLoadChecksApprovedPermissions() {
    ManagerFixture fixture;
    QVERIFY(fixture.install(conformancePackage(QStringLiteral("importer"))).ok());

    // installed.json says the user approved nothing: the plugin's real import decides.
    QJsonObject root = QJsonDocument::fromJson(readFile(installedJson(fixture))).object();
    QJsonArray records = root.value(QStringLiteral("plugins")).toArray();
    QJsonObject record = records.at(0).toObject();
    record.insert(QStringLiteral("approved_permissions"), QJsonArray());
    record.insert(QStringLiteral("permissions"), QJsonArray()); // a lie about what the plugin uses
    records.replace(0, record);
    root.insert(QStringLiteral("plugins"), records);
    QVERIFY(writeFile(installedJson(fixture), QJsonDocument(root).toJson()));
    fixture.reopen();

    PluginResult result;
    QVERIFY(!fixture.manager->load(ImporterId, &result));
    QCOMPARE(result.code, Code::NotApproved);
    QVERIFY2(result.message.contains(QStringLiteral("pick-file")), qPrintable(result.message));

    // The other direction: the list claims MORE than the component imports. The plugin
    // still only gets what it imports (network stays unlinked and unused).
    record.insert(QStringLiteral("approved_permissions"),
                  QJsonArray{QStringLiteral("pick-file"), QStringLiteral("network"), QStringLiteral("storage")});
    records.replace(0, record);
    root.insert(QStringLiteral("plugins"), records);
    QVERIFY(writeFile(installedJson(fixture), QJsonDocument(root).toJson()));
    fixture.reopen();
    const auto worker = fixture.manager->load(ImporterId, &result);
    QVERIFY2(worker, qPrintable(result.message));
    QVERIFY(!QFileInfo::exists(fixture.manager->storageDirectory(ImporterId)));
}

void PluginManagerTest::testRevokedAfterInstallRefusedAtLoad() {
    ManagerFixture fixture;
    FakeCatalog catalog;
    const QByteArray bytes = conformancePackage(QStringLiteral("importer"));
    catalog.known.insert(PluginPackage::hash(bytes), PluginVerification::Verified);
    fixture.manager->setCatalog(&catalog);
    QVERIFY(fixture.install(bytes).ok());
    QVERIFY(fixture.manager->load(ImporterId));
    fixture.manager->unload(ImporterId);

    catalog.known.insert(PluginPackage::hash(bytes), PluginVerification::Revoked);
    PluginResult result;
    QVERIFY(!fixture.manager->load(ImporterId, &result));
    QCOMPARE(result.code, Code::Revoked);

    // Offline does not block an installed plugin.
    catalog.offline = true;
    QVERIFY(fixture.manager->load(ImporterId, &result));
}

void PluginManagerTest::testEnableDisable() {
    ManagerFixture fixture;
    QVERIFY(fixture.install(conformancePackage(QStringLiteral("importer"))).ok());
    std::weak_ptr<PluginWorker> running = fixture.manager->load(ImporterId);
    QVERIFY(!running.expired());

    QSignalSpy changed(fixture.manager.get(), &PluginManager::pluginsChanged);
    QVERIFY(fixture.manager->setEnabled(ImporterId, false).ok());
    QCOMPARE(changed.count(), 1);
    QVERIFY(running.expired()); // the running instance was stopped
    PluginResult result;
    QVERIFY(!fixture.manager->load(ImporterId, &result));
    QCOMPARE(result.code, Code::Disabled);
    QVERIFY(fixture.manager->setEnabled(ImporterId, false).ok());
    QCOMPARE(changed.count(), 1); // no change, no signal

    fixture.reopen();
    QVERIFY(!fixture.manager->plugin(ImporterId)->enabled);
    QVERIFY(!fixture.manager->load(ImporterId, &result));
    QCOMPARE(result.code, Code::Disabled);

    QVERIFY(fixture.manager->setEnabled(ImporterId, true).ok());
    QVERIFY(fixture.manager->load(ImporterId, &result));

    QCOMPARE(fixture.manager->setEnabled(QStringLiteral("com.example.unknown"), true).code, Code::NotInstalled);
    QVERIFY(!fixture.manager->load(QStringLiteral("com.example.unknown"), &result));
    QCOMPARE(result.code, Code::NotInstalled);
}

void PluginManagerTest::testUninstallCleanup() {
    ManagerFixture fixture;
    const QByteArray data("encrypted database bytes");

    // Setting on (default): package, record and storage disappear.
    auto worker = fixture.installAndLoad(QStringLiteral("database-source"));
    QVERIFY(worker);
    QVERIFY(writeDatabase(*worker, data));
    const QString storage = fixture.manager->storageDirectory(DatabaseId);
    QVERIFY(QFileInfo::exists(QDir(storage).filePath(QStringLiteral("files/main.epdb"))));
    worker.reset();

    QSignalSpy changed(fixture.manager.get(), &PluginManager::pluginsChanged);
    QVERIFY(fixture.manager->uninstall(DatabaseId).ok());
    QCOMPARE(changed.count(), 1);
    QVERIFY(!fixture.manager->plugin(DatabaseId).has_value());
    QVERIFY(!QFileInfo::exists(fixture.manager->packagePath(DatabaseId)));
    QVERIFY(!QFileInfo::exists(storage));
    PluginResult result;
    QVERIFY(!fixture.manager->load(DatabaseId, &result));
    QCOMPARE(result.code, Code::NotInstalled);
    QCOMPARE(fixture.manager->uninstall(DatabaseId).code, Code::NotInstalled);
    fixture.reopen();
    QVERIFY(fixture.manager->installedPlugins().isEmpty());

    // A new install starts with empty storage.
    worker = fixture.installAndLoad(QStringLiteral("database-source"));
    QVERIFY(worker);
    PluginError error;
    readDatabase(*worker, &error);
    QCOMPARE(error.code, QStringLiteral("not-found"));

    // Setting off: the storage is kept and a later install finds it again.
    QVERIFY(writeDatabase(*worker, data));
    worker.reset();
    fixture.removeStorage = false;
    QVERIFY(fixture.manager->uninstall(DatabaseId).ok());
    QVERIFY(!QFileInfo::exists(fixture.manager->packagePath(DatabaseId)));
    QVERIFY(QFileInfo::exists(QDir(storage).filePath(QStringLiteral("files/main.epdb"))));
    worker = fixture.installAndLoad(QStringLiteral("database-source"));
    QVERIFY(worker);
    QCOMPARE(readDatabase(*worker, &error), data);
}

void PluginManagerTest::testUpdateKeepsStorage() {
    ManagerFixture fixture;
    const QByteArray data("database before the update");
    auto worker = fixture.installAndLoad(QStringLiteral("database-source"));
    QVERIFY(worker);
    QVERIFY(writeDatabase(*worker, data));
    std::weak_ptr<PluginWorker> old = worker;
    worker.reset();
    QVERIFY(fixture.manager->setEnabled(DatabaseId, false).ok());
    const QByteArray oldHash = fixture.manager->plugin(DatabaseId)->sha256;

    // Version 1.1.0 of the same plugin id.
    const QByteArray manifest = QString::fromUtf8(manifestJson(DatabaseId))
                                    .replace(QStringLiteral("\"version\": \"1.0.0\""), QStringLiteral("\"version\": \"1.1.0\""))
                                    .toUtf8();
    const QByteArray update = makePackage(manifest, component(QStringLiteral("database-source")));
    InstallCandidate candidate;
    QVERIFY(fixture.manager->prepareInstall(update, &candidate).ok());
    QVERIFY(candidate.isUpdate);
    QCOMPARE(candidate.installedVersion, QStringLiteral("1.0.0"));
    QVERIFY(fixture.manager->install(candidate, candidate.inspection.permissions).ok());

    QCOMPARE(fixture.manager->installedPlugins().size(), 1);
    const InstalledPlugin record = *fixture.manager->plugin(DatabaseId);
    QCOMPARE(record.version, QStringLiteral("1.1.0"));
    QVERIFY(record.sha256 != oldHash);
    QVERIFY(!record.enabled); // the user's switch survives the update
    QVERIFY(old.expired());
    QVERIFY(fixture.manager->setEnabled(DatabaseId, true).ok());
    worker = fixture.manager->load(DatabaseId);
    QVERIFY(worker);
    PluginError error;
    QCOMPARE(readDatabase(*worker, &error), data); // the plugin's data survived
}

void PluginManagerTest::testStorageIsolationBetweenPlugins() {
    // Two installed plugins with the same code but different ids.
    ManagerFixture fixture;
    const QString firstId = QStringLiteral("com.example.first"), secondId = QStringLiteral("com.example.second");
    const QByteArray wasm = component(QStringLiteral("database-source"));
    QVERIFY(fixture.install(makePackage(manifestJson(firstId), wasm)).ok());
    QVERIFY(fixture.install(makePackage(manifestJson(secondId), wasm)).ok());
    auto first = fixture.manager->load(firstId);
    auto second = fixture.manager->load(secondId);
    QVERIFY(first && second && first != second);

    PluginError error;
    QVERIFY(writeDatabase(*first, "data of the first plugin"));
    readDatabase(*second, &error);
    QCOMPARE(error.code, QStringLiteral("not-found")); // the second one sees nothing
    QVERIFY(writeDatabase(*second, "data of the second plugin"));
    QCOMPARE(readDatabase(*first, &error), QByteArray("data of the first plugin"));
    QCOMPARE(readDatabase(*second, &error), QByteArray("data of the second plugin"));

    // Configuration (key/value storage) is separate as well.
    const Value raw = callSync(*first, DatabaseInterface, QStringLiteral("set-config"),
                               {Value::string(QStringLiteral(R"({"database_name":"First","read_only":true})"))}, &error);
    QVERIFY(error.ok() && raw.isOk());
    QVERIFY(writeDatabase(*second, "second is still writable"));
    QVERIFY(!writeDatabase(*first, "first is read-only now"));

    const QString firstStorage = fixture.manager->storageDirectory(firstId);
    const QString secondStorage = fixture.manager->storageDirectory(secondId);
    QVERIFY(firstStorage != secondStorage);
    QCOMPARE(readFile(QDir(firstStorage).filePath(QStringLiteral("files/main.epdb"))), QByteArray("data of the first plugin"));
    QCOMPARE(readFile(QDir(secondStorage).filePath(QStringLiteral("files/main.epdb"))), QByteArray("second is still writable"));
    // Everything the plugins wrote is inside their two directories.
    QCOMPARE(QDir(QDir(fixture.environment.pluginsDirectory).filePath(QStringLiteral("storage")))
                 .entryList(QDir::AllEntries | QDir::NoDotAndDotDot),
             (QStringList{firstId, secondId}));

    first.reset();
    QVERIFY(fixture.manager->uninstall(firstId).ok());
    QVERIFY(!QFileInfo::exists(firstStorage));
    QCOMPARE(readDatabase(*second, &error), QByteArray("second is still writable"));
}

void PluginManagerTest::testNetworkPolicyFromManifest() {
    TestServer server;
    ManagerFixture fixture;
    auto worker = fixture.installAndLoad(QStringLiteral("network"));
    QVERIFY(worker);
    PluginError error;
    const QString hello = QStringLiteral("http GET ") + server.url(QStringLiteral("/hello"));

    // Default: the allowed name resolves to 127.0.0.1, which a plugin must not reach.
    importFirst(*worker, hello, &error);
    QCOMPARE(error.code, QStringLiteral("not-allowed"));
    QVERIFY(server.requests().isEmpty());

    // The user allows local addresses for this plugin.
    worker.reset();
    QVERIFY(fixture.manager->setAllowPrivateNetwork(NetworkId, true).ok());
    worker = fixture.manager->load(NetworkId);
    QVERIFY(worker);
    Value item = importFirst(*worker, hello, &error);
    QVERIFY2(error.ok(), qPrintable(error.message));
    QCOMPARE(item.field("display-name").toString(), QStringLiteral("200"));
    QCOMPARE(item.field("notes").toString(), QStringLiteral("hello"));

    // The manifest allow-list still applies: other hosts and ports stay closed.
    importFirst(*worker, QStringLiteral("http GET ") + server.url(QStringLiteral("/hello"), QStringLiteral("other.conformance.test")),
                &error);
    QCOMPARE(error.code, QStringLiteral("not-allowed"));
    importFirst(*worker, QStringLiteral("http GET ") + server.url(QStringLiteral("/redirect-other")), &error);
    QCOMPARE(error.code, QStringLiteral("not-allowed"));
    importFirst(*worker, QStringLiteral("tcp port.conformance.test %1 plain x").arg(server.tcpPort()), &error);
    QCOMPARE(error.code, QStringLiteral("not-allowed"));
    item = importFirst(*worker, QStringLiteral("tcp allowed.conformance.test %1 plain hello").arg(server.tcpPort()), &error);
    QVERIFY2(error.ok(), qPrintable(error.message));
    QCOMPARE(item.field("notes").toString(), QStringLiteral("echo:hello"));
    QCOMPARE(server.requests(), (QStringList{"GET /hello", "GET /redirect-other"}));

    // The same component with an empty network_hosts list reaches nothing at all.
    const QString emptyId = QStringLiteral("com.example.no-hosts");
    QVERIFY(fixture.install(makePackage(manifestJson(emptyId, QStringLiteral(R"("network_hosts": [])")),
                                        component(QStringLiteral("network"))))
                .ok());
    QVERIFY(fixture.manager->setAllowPrivateNetwork(emptyId, true).ok());
    const auto closed = fixture.manager->load(emptyId);
    QVERIFY(closed);
    importFirst(*closed, hello, &error);
    QCOMPARE(error.code, QStringLiteral("not-allowed"));

    // An update with a different host list drops the local-network override.
    const QByteArray update = makePackage(
        manifestJson(NetworkId, QStringLiteral(R"("network_hosts": ["*:*"])")), component(QStringLiteral("network")));
    worker.reset();
    QVERIFY(fixture.install(update).ok());
    QVERIFY(!fixture.manager->plugin(NetworkId)->allowPrivateNetwork);
    QCOMPARE(fixture.manager->plugin(NetworkId)->networkHosts, QStringList{QStringLiteral("*:*")});
}

void PluginManagerTest::testInitializerRuns() {
    ManagerFixture fixture;
    const QString aboutId = QStringLiteral("org.easepass.conformance.about");
    const QString disabledId = QStringLiteral("com.example.disabled");
    QVERIFY(fixture.install(conformancePackage(QStringLiteral("about"))).ok());
    QVERIFY(fixture.install(conformancePackage(QStringLiteral("importer"))).ok());
    QVERIFY(fixture.install(makePackage(manifestJson(disabledId), component(QStringLiteral("about")))).ok());
    QVERIFY(fixture.manager->setEnabled(disabledId, false).ok());

    fixture.manager->startInitializers();
    const QString initLine = aboutId + QStringLiteral("|info|conformance-about: init");
    QVERIFY(waitUntil([&] { return fixture.log().contains(initLine); }));

    // init ran exactly once, also after further calls; the disabled plugin did not run.
    PluginError error;
    const auto worker = fixture.manager->load(aboutId);
    const Value info = callSync(*worker, QStringLiteral("easepass:plugin/about"), QStringLiteral("info"), {}, &error);
    QVERIFY2(error.ok(), qPrintable(error.message));
    QCOMPARE(info.field("name").toString(), QStringLiteral("Conformance: about"));
    QCOMPARE(fixture.log().count(initLine), 1);
    for (const QString &line : fixture.log())
        QVERIFY2(!line.startsWith(disabledId), qPrintable(line));
}

void PluginManagerTest::testLeakCheckWithService() {
    using namespace EasePass::Core;

    /// Answers every range request with a fixed body.
    class RangeClient : public INetworkClient {
    public:
        QByteArray body;
        int requests = 0;
        void get(const QNetworkRequest &, ResponseCallback callback) override {
            ++requests;
            NetworkResponse response;
            response.statusCode = 200;
            response.data = body;
            callback(response);
        }
    };
    auto client = std::make_shared<RangeClient>();
    client->body = LeakCheckService::sha1Hex(SecretString(QStringLiteral("hunter2"))).mid(5) + ":42\r\n";
    AppSettings settings(std::make_shared<InMemorySettingsStore>());
    settings.setVerifyNotLeaked(true);
    auto service = std::make_unique<LeakCheckService>(client, &settings);
    const auto check = PluginManager::leakCheckWith(service.get());

    // The plugin thread blocks in check() while the service works on this thread.
    const auto ask = [&](const QString &password) {
        LeakCheckOutcome outcome;
        bool done = false;
        std::thread pluginThread([&] {
            outcome = check(password);
            done = true;
        });
        const bool finished = waitUntil([&] { return done; });
        pluginThread.join();
        if (!finished)
            outcome.status = QStringLiteral("test timed out");
        return outcome;
    };

    LeakCheckOutcome outcome = ask(QStringLiteral("hunter2"));
    QCOMPARE(outcome.status, QStringLiteral("leaked"));
    QCOMPARE(outcome.count, 42u);
    outcome = ask(QStringLiteral("a much better password"));
    QCOMPARE(outcome.status, QStringLiteral("not-leaked"));
    QCOMPARE(outcome.count, 0u);

    // The user switched leak checks off: plugins get "unknown" and nothing is sent.
    settings.setVerifyNotLeaked(false);
    const int requestsBefore = client->requests;
    QCOMPARE(ask(QStringLiteral("hunter2")).status, QStringLiteral("unknown"));
    QCOMPARE(client->requests, requestsBefore);

    // The service is gone (shutdown): no crash, "unknown".
    service.reset();
    QCOMPARE(ask(QStringLiteral("hunter2")).status, QStringLiteral("unknown"));
}
