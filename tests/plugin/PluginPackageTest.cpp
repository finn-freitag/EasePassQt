#include "PluginTests.h"

#include "PluginPackage.h"
#include "PluginTestSupport.h"

#include <QCryptographicHash>
#include <QTest>

using namespace EasePass::Plugin;
using namespace PluginTest;

namespace {
const QByteArray Wasm = QByteArray("\0asm-not-really", 15);
QByteArray validManifest() { return manifestJson(QStringLiteral("com.example.plugin")); }
} // namespace

void PluginPackageTest::testParseConformancePackage() {
    const QByteArray bytes = conformancePackage(QStringLiteral("importer"));
    QString error;
    const std::optional<PluginPackage> package = PluginPackage::parse(bytes, &error);
    QVERIFY2(package, qPrintable(error));

    QCOMPARE(package->sha256, QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
    QCOMPARE(PluginPackage::hash(bytes), package->sha256);
    QCOMPARE(package->sha256.size(), 64);
    QCOMPARE(package->wasm, component(QStringLiteral("importer")));
    QVERIFY(package->icon.isEmpty());
    QCOMPARE(package->manifest.id, QStringLiteral("org.easepass.conformance.importer"));
    QCOMPARE(package->manifest.name, QStringLiteral("Conformance: importer"));
    QCOMPARE(package->manifest.version, QStringLiteral("1.0.0"));
    QCOMPARE(package->manifest.apiVersion, 1);
    QCOMPARE(*package->manifest.permissions, QStringList{QStringLiteral("pick-file")});
}

void PluginPackageTest::testStoredDeflateAndIcon() {
    const QByteArray big = QByteArray(200'000, 'w') + Wasm;
    for (bool deflate : {false, true}) {
        const QByteArray zip = makeZip({{"manifest.json", validManifest(), deflate},
                                        {"plugin.wasm", big, deflate},
                                        {"icon.png", QByteArray("\x89PNG-icon"), deflate}});
        QString error;
        const std::optional<PluginPackage> package = PluginPackage::parse(zip, &error);
        QVERIFY2(package, qPrintable(error));
        QCOMPARE(package->wasm, big);
        QCOMPARE(package->icon, QByteArray("\x89PNG-icon"));
        QCOMPARE(package->manifest.id, QStringLiteral("com.example.plugin"));
        if (deflate)
            QVERIFY(zip.size() < 10'000);
    }
}

void PluginPackageTest::testOtherEntriesIgnored() {
    // Unknown entries (even with hostile names or flags) are never read or extracted.
    const QByteArray zip = makeZip({{"../../evil.sh", "rm -rf", false, 0x0001},
                                    {"manifest.json", validManifest()},
                                    {"/etc/passwd", "x", false, 0, 99},
                                    {"plugin.wasm", Wasm},
                                    {"MANIFEST.JSON", "{}"}});
    QString error;
    const std::optional<PluginPackage> package = PluginPackage::parse(zip, &error);
    QVERIFY2(package, qPrintable(error));
    QCOMPARE(package->wasm, Wasm);
}

void PluginPackageTest::testBrokenArchivesRejected_data() {
    QTest::addColumn<QByteArray>("bytes");
    QTest::addColumn<QString>("message");

    const QByteArray good = makePackage(validManifest(), Wasm);
    QTest::newRow("empty") << QByteArray() << "not a valid ZIP";
    QTest::newRow("text") << QByteArray("this is not a zip archive at all, sorry") << "not a valid ZIP";
    QTest::newRow("truncated") << good.left(good.size() - 5) << "not a valid ZIP";
    QTest::newRow("trailing garbage") << good + "x" << "not a valid ZIP";
    QTest::newRow("no manifest") << makeZip({{"plugin.wasm", Wasm}}) << "no manifest.json";
    QTest::newRow("no wasm") << makeZip({{"manifest.json", validManifest()}}) << "no plugin.wasm";
    QTest::newRow("duplicate manifest") << makeZip({{"manifest.json", validManifest()},
                                                    {"plugin.wasm", Wasm},
                                                    {"manifest.json", validManifest()}})
                                        << "twice";
    QTest::newRow("encrypted") << makeZip({{"manifest.json", validManifest()}, {"plugin.wasm", Wasm, false, 0x0001}})
                               << "encrypted";
    QTest::newRow("unknown method") << makeZip({{"manifest.json", validManifest()}, {"plugin.wasm", Wasm, false, 0, 14}})
                                    << "compression method";
    ZipFile wrongCrc{"plugin.wasm", Wasm};
    wrongCrc.corruptCrc = true;
    QTest::newRow("wrong crc") << makeZip({{"manifest.json", validManifest()}, wrongCrc}) << "damaged";
    ZipFile sizeLie{"plugin.wasm", QByteArray(100'000, 'z'), true};
    sizeLie.declaredSize = 10; // decompresses to far more than it claims
    QTest::newRow("size lie") << makeZip({{"manifest.json", validManifest()}, sizeLie}) << "damaged";
    ZipFile zip64{"plugin.wasm", Wasm};
    zip64.declaredSize = 0xFFFFFFFFll;
    QTest::newRow("zip64 marker") << makeZip({{"manifest.json", validManifest()}, zip64}) << "ZIP64";
    QTest::newRow("manifest too large") << makeZip({{"manifest.json", QByteArray(70'000, ' ') + validManifest(), true},
                                                    {"plugin.wasm", Wasm}})
                                        << "too large";
    QList<ZipFile> many = {{"manifest.json", validManifest()}, {"plugin.wasm", Wasm}};
    for (int i = 0; i < 70; ++i)
        many << ZipFile{"extra" + QByteArray::number(i), "x"};
    QTest::newRow("too many entries") << makeZip(many) << "too many files";
    QTest::newRow("invalid manifest") << makePackage("{ not json", Wasm) << "manifest";
    QTest::newRow("package too large") << QByteArray(PluginPackage::MaxPackageBytes + 1, 'p') << "16 MiB";
}

void PluginPackageTest::testBrokenArchivesRejected() {
    QFETCH(QByteArray, bytes);
    QFETCH(QString, message);

    QString error;
    QVERIFY(!PluginPackage::parse(bytes, &error));
    QVERIFY2(error.contains(message), qPrintable(error));
}

void PluginPackageTest::testManifestValidation_data() {
    QTest::addColumn<QByteArray>("json");
    QTest::addColumn<bool>("valid");

    const auto with = [](const QString &extra) { return manifestJson(QStringLiteral("com.example.plugin"), extra); };
    const auto base = [](const QString &member, const QString &value) {
        // Replaces one member of a valid manifest.
        QString json = QString::fromUtf8(manifestJson(QStringLiteral("com.example.plugin")));
        const qsizetype start = json.indexOf(QStringLiteral("\"%1\"").arg(member));
        const qsizetype end = member == QLatin1String("description") ? json.size() - 1 : json.indexOf(QLatin1Char(','), start);
        return json.replace(start, end - start, value).toUtf8();
    };

    // Mirrors samples/manifest of the spec repository.
    QTest::newRow("minimal") << validManifest() << true;
    QTest::newRow("full") << with(R"("author": "A", "homepage": "https://example.com", "license": "MIT",
        "os": ["windows", "linux", "macos"], "min_app_version": "2.0.0",
        "network_hosts": ["*:21", "api.example.com:443", "*.example.org:*", "192.0.2.10:8443", "[2001:db8::1]:443"],
        "permissions": ["storage", "network"], "future_property": {"ignored": true})")
                          << true;
    QTest::newRow("pre-release version") << base("version", R"("version": "1.2.0-beta.1")") << true;
    QTest::newRow("empty lists") << with(R"("os": [], "network_hosts": [], "permissions": [])") << true;

    QTest::newRow("not an object") << QByteArray("[1, 2]") << false;
    QTest::newRow("missing id") << base("id", R"("x": 1)") << false;
    QTest::newRow("missing description") << base("description", R"("x": 1)") << false;
    QTest::newRow("bad id") << manifestJson(QStringLiteral("Not A Reverse DNS Id")) << false;
    QTest::newRow("id without dot") << manifestJson(QStringLiteral("plugin")) << false;
    QTest::newRow("id path traversal") << manifestJson(QStringLiteral("com.example/../../evil")) << false;
    QTest::newRow("id dot dot") << manifestJson(QStringLiteral("com..example")) << false;
    QTest::newRow("id upper case") << manifestJson(QStringLiteral("Com.Example.Plugin")) << false;
    QTest::newRow("empty name") << base("name", R"("name": "")") << false;
    QTest::newRow("name not a string") << base("name", R"("name": 5)") << false;
    QTest::newRow("bad version") << base("version", R"("version": "1.0")") << false;
    QTest::newRow("api_version string") << base("api_version", R"("api_version": "1")") << false;
    QTest::newRow("api_version zero") << base("api_version", R"("api_version": 0)") << false;
    QTest::newRow("api_version fraction") << base("api_version", R"("api_version": 1.5)") << false;
    QTest::newRow("unknown os") << with(R"("os": ["amiga"])") << false;
    QTest::newRow("os not a list") << with(R"("os": "linux")") << false;
    QTest::newRow("duplicate os") << with(R"("os": ["linux", "linux"])") << false;
    QTest::newRow("host without port") << with(R"("network_hosts": ["example.com"])") << false;
    QTest::newRow("host as url") << with(R"("network_hosts": ["https://example.com:443"])") << false;
    QTest::newRow("host port zero") << with(R"("network_hosts": ["example.com:0"])") << false;
    QTest::newRow("host port too high") << with(R"("network_hosts": ["example.com:70000"])") << false;
    QTest::newRow("host upper case") << with(R"("network_hosts": ["Example.com:443"])") << false;
    QTest::newRow("wildcard in the middle") << with(R"("network_hosts": ["api.*.com:443"])") << false;
    QTest::newRow("unknown permission") << with(R"("permissions": ["filesystem"])") << false;
    QTest::newRow("bad min_app_version") << with(R"("min_app_version": "2")") << false;
    QTest::newRow("homepage scheme") << with(R"x("homepage": "javascript:alert(1)")x") << false;
}

void PluginPackageTest::testManifestValidation() {
    QFETCH(QByteArray, json);
    QFETCH(bool, valid);

    QString error;
    const std::optional<PluginManifest> manifest = PluginManifest::parse(json, &error);
    QCOMPARE(manifest.has_value(), valid);
    if (!valid)
        QVERIFY2(error.startsWith(QStringLiteral("manifest: ")), qPrintable(error));
}

void PluginPackageTest::testManifestFields() {
    QString error;
    const std::optional<PluginManifest> manifest = PluginManifest::parse(
        manifestJson(QStringLiteral("de.example.my-plugin"),
                     R"("author": "Jane", "homepage": "https://example.com/p", "license": "MIT", "os": ["linux"],
                        "min_app_version": "1.2.3", "network_hosts": ["a.example.com:443"], "permissions": [])"),
        &error);
    QVERIFY2(manifest, qPrintable(error));
    QCOMPARE(manifest->id, QStringLiteral("de.example.my-plugin"));
    QCOMPARE(manifest->author, QStringLiteral("Jane"));
    QCOMPARE(manifest->homepage, QStringLiteral("https://example.com/p"));
    QCOMPARE(manifest->license, QStringLiteral("MIT"));
    QCOMPARE(manifest->os, QStringList{QStringLiteral("linux")});
    QCOMPARE(manifest->minAppVersion, QStringLiteral("1.2.3"));
    QCOMPARE(manifest->networkHosts, QStringList{QStringLiteral("a.example.com:443")});
    QVERIFY(manifest->permissions.has_value());
    QVERIFY(manifest->permissions->isEmpty());
    QCOMPARE(manifest->supportsCurrentOs(), PluginManifest::currentOs() == QLatin1String("linux"));

    // No "permissions" member is different from an empty list.
    const std::optional<PluginManifest> minimal = PluginManifest::parse(validManifest(), &error);
    QVERIFY(minimal && !minimal->permissions.has_value());
    QVERIFY(minimal->supportsCurrentOs());
}

void PluginPackageTest::testCompareVersions() {
    QCOMPARE(PluginManifest::compareVersions("1.4.0", "1.4.0"), 0);
    QVERIFY(PluginManifest::compareVersions("1.4.0", "2.0.0") < 0);
    QVERIFY(PluginManifest::compareVersions("1.10.0", "1.9.9") > 0);
    QVERIFY(PluginManifest::compareVersions("2.0.1", "2.0.0") > 0);
    QCOMPARE(PluginManifest::compareVersions("1.4.0-beta.1", "1.4.0"), 0);
}
