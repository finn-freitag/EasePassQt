#include "PluginPocTest.h"

#include "ImporterPlugin.h"
#include "PluginInspector.h"

#include <QElapsedTimer>
#include <QFile>
#include <QTest>
#include <QThread>
#include <thread>

using namespace EasePass::Plugin;

namespace {

QByteArray fixture(const QString &name) {
    QFile file(QStringLiteral(EASEPASS_TEST_FIXTURES_DIR "/plugins/") + name);
    if (!file.open(QIODevice::ReadOnly))
        qFatal("missing fixture %s", qPrintable(file.fileName()));
    return file.readAll();
}

QByteArray importerBytes() { return fixture(QStringLiteral("conformance-importer.wasm")); }

struct Loaded {
    std::unique_ptr<ImporterPlugin> plugin;
    PluginError error;
};

Loaded loadImporter(PluginHost host = {}, PluginLimits limits = {},
                    const QStringList &approved = {QStringLiteral("pick-file")}) {
    Loaded loaded;
    loaded.plugin = ImporterPlugin::load(importerBytes(), approved, std::move(host), limits, &loaded.error);
    return loaded;
}

} // namespace

void PluginPocTest::testInspectConformancePlugins_data() {
    QTest::addColumn<QString>("file");
    QTest::addColumn<QStringList>("types");
    QTest::addColumn<QStringList>("permissions");

    // Must equal expected.json of each conformance plugin in the spec repository.
    QTest::newRow("about") << "conformance-about.wasm" << QStringList{"about", "initializer"} << QStringList{};
    QTest::newRow("importer") << "conformance-importer.wasm" << QStringList{"importer"} << QStringList{"pick-file"};
    QTest::newRow("database-source")
        << "conformance-database-source.wasm" << QStringList{"database-source"} << QStringList{"storage"};
    QTest::newRow("password-generator")
        << "conformance-password-generator.wasm" << QStringList{"password-generator"} << QStringList{"leak-check"};
    QTest::newRow("store-feed") << "conformance-store-feed.wasm" << QStringList{"store-feed"} << QStringList{};
}

void PluginPocTest::testInspectConformancePlugins() {
    QFETCH(QString, file);
    QFETCH(QStringList, types);
    QFETCH(QStringList, permissions);

    const PluginInspection inspection = PluginInspector::inspect(fixture(file));
    QVERIFY2(inspection.accepted(), qPrintable(inspection.rejected.join("; ")));
    QCOMPARE(inspection.types, types);
    QCOMPARE(inspection.permissions, permissions);
    QVERIFY(!inspection.exports.isEmpty());
}

void PluginPocTest::testInspectDoesNotNeedApproval() {
    // Inspection reports the raw imports without linking or running anything.
    const PluginInspection inspection = PluginInspector::inspect(importerBytes());
    QVERIFY(inspection.imports.contains(QStringLiteral("easepass:plugin/file-picker@1.0.0")));
    QVERIFY(inspection.imports.contains(QStringLiteral("easepass:plugin/log@1.0.0")));
    QCOMPARE(inspection.exports, QStringList{QStringLiteral("easepass:plugin/importer@1.0.0")});
    QCOMPARE(PluginInspector::importOfPermission(QStringLiteral("pick-file")),
             QStringLiteral("easepass:plugin/file-picker"));
}

void PluginPocTest::testForbiddenImportsRejected_data() {
    QTest::addColumn<QString>("file");
    QTest::addColumn<QString>("reason");

    QTest::newRow("wasi:filesystem") << "negative/forbidden-import.wasm"
                                     << "import 'wasi:filesystem/preopens@0.2.0' is not on the allow-list";
    QTest::newRow("unknown namespace") << "negative/unknown-namespace.wasm"
                                       << "import 'evil:backdoor/shell@1.0.0' is not on the allow-list";
    QTest::newRow("future api version") << "negative/future-api-version.wasm"
                                        << "import 'easepass:plugin/storage@2.0.0' has an unsupported version";
}

void PluginPocTest::testForbiddenImportsRejected() {
    QFETCH(QString, file);
    QFETCH(QString, reason);

    const QByteArray bytes = fixture(file);
    const PluginInspection inspection = PluginInspector::inspect(bytes);
    QVERIFY(!inspection.accepted());
    QVERIFY2(inspection.rejected.contains(reason), qPrintable(inspection.rejected.join("; ")));
    QVERIFY(inspection.permissions.isEmpty());

    // Loading refuses it as well, even if the caller approves everything.
    PluginError error;
    const auto plugin = ImporterPlugin::load(
        bytes, {"storage", "pick-file", "network", "leak-check"}, {}, {}, &error);
    QVERIFY(!plugin);
    QCOMPARE(error.kind, PluginError::Kind::Rejected);
}

void PluginPocTest::testNotAComponentRejected() {
    for (const QByteArray &bytes : {QByteArray(), QByteArray("not wasm at all"),
                                    // a valid but empty core module (not a component)
                                    QByteArray::fromHex("0061736d01000000")}) {
        const PluginInspection inspection = PluginInspector::inspect(bytes);
        QVERIFY(!inspection.accepted());
        PluginError error;
        QVERIFY(!ImporterPlugin::load(bytes, {}, {}, {}, &error));
        QCOMPARE(error.kind, PluginError::Kind::Rejected);
    }
}

void PluginPocTest::testUnapprovedImportFailsToInstantiate() {
    // The importer imports file-picker. Without the approved permission the interface is
    // not defined in the linker, so the component cannot be instantiated at all.
    const Loaded denied = loadImporter({}, {}, {});
    QVERIFY(!denied.plugin);
    QCOMPARE(denied.error.kind, PluginError::Kind::Instantiate);
    QVERIFY2(denied.error.message.contains(QStringLiteral("file-picker")), qPrintable(denied.error.message));

    // Approving an unrelated permission does not help either.
    const Loaded wrong = loadImporter({}, {}, {QStringLiteral("storage")});
    QVERIFY(!wrong.plugin);
    QCOMPARE(wrong.error.kind, PluginError::Kind::Instantiate);

    const Loaded approved = loadImporter();
    QVERIFY2(approved.plugin, qPrintable(approved.error.message));
}

void PluginPocTest::testSources() {
    const Loaded loaded = loadImporter();
    QVERIFY2(loaded.plugin, qPrintable(loaded.error.message));

    PluginError error;
    const QList<ImportSource> sources = loaded.plugin->sources(&error);
    QVERIFY2(error.ok(), qPrintable(error.message));
    QCOMPARE(sources.size(), 2);
    QCOMPARE(sources[0].id, QStringLiteral("fixed"));
    QCOMPARE(sources[0].name, QStringLiteral("Fixed items"));
    QCOMPARE(sources[1].id, QStringLiteral("csv-file"));
    QCOMPARE(sources[1].description, QStringLiteral("Lines of 'display name;user name;password'."));
}

void PluginPocTest::testImportFixedItems() {
    QStringList log;
    PluginHost host;
    host.log = [&](const QString &level, const QString &message) { log << level + QLatin1Char(':') + message; };
    const Loaded loaded = loadImporter(host);
    QVERIFY2(loaded.plugin, qPrintable(loaded.error.message));

    PluginError error;
    const QList<ImportedPassword> items = loaded.plugin->importPasswords(QStringLiteral("fixed"), &error);
    QVERIFY2(error.ok(), qPrintable(error.message));
    QCOMPARE(log, QStringList{QStringLiteral("info:conformance-importer: importing fixed items")});

    QCOMPARE(items.size(), 2);
    QCOMPARE(items[0].displayName, QStringLiteral("Example"));
    QCOMPARE(items[0].userName, QStringLiteral("alice"));
    QCOMPARE(items[0].email, QStringLiteral("alice@example.com"));
    QCOMPARE(items[0].website, QStringLiteral("https://example.com"));
    QCOMPARE(items[0].password, QStringLiteral("correct horse battery staple"));
    QCOMPARE(items[0].notes, QStringLiteral("First conformance item"));
    QCOMPARE(items[0].tags, (QStringList{QStringLiteral("conformance"), QStringLiteral("example")}));
    QVERIFY(!items[0].hasTotp);

    QCOMPARE(items[1].displayName, QString::fromUtf8("Ünïcödé ✓"));
    QCOMPARE(items[1].userName, QStringLiteral("bob"));
    QCOMPARE(items[1].password, QString::fromUtf8("pässwörd-🔑"));
    QVERIFY(items[1].email.isEmpty());
    QVERIFY(items[1].tags.isEmpty());
    QVERIFY(items[1].hasTotp);
    QCOMPARE(items[1].totpSecret, QStringLiteral("JBSWY3DPEHPK3PXP"));
    QCOMPARE(items[1].totpDigits, 6);
    QCOMPARE(items[1].totpInterval, 30);
    QCOMPARE(items[1].totpAlgorithm, QStringLiteral("sha1"));

    // The instance stays usable for further calls.
    QCOMPARE(loaded.plugin->importPasswords(QStringLiteral("fixed"), &error).size(), 2);
    QVERIFY(error.ok());
}

void PluginPocTest::testImportCsvThroughFilePicker() {
    QString seenTitle;
    QStringList seenExtensions;
    QByteArray content = QString::fromUtf8("Mail;carol;s3cret\n\nBänk;dave;geld €\n").toUtf8();
    PluginHost host;
    host.pickFile = [&](const QString &title, const QStringList &extensions) {
        seenTitle = title;
        seenExtensions = extensions;
        return std::optional<PluginHost::PickedFile>({QStringLiteral("export.csv"), content});
    };
    const Loaded loaded = loadImporter(host);
    QVERIFY2(loaded.plugin, qPrintable(loaded.error.message));

    PluginError error;
    const QList<ImportedPassword> items = loaded.plugin->importPasswords(QStringLiteral("csv-file"), &error);
    QVERIFY2(error.ok(), qPrintable(error.message));
    QCOMPARE(seenTitle, QStringLiteral("Choose a CSV file"));
    QCOMPARE(seenExtensions, QStringList{QStringLiteral("csv")});
    QCOMPARE(items.size(), 2);
    QCOMPARE(items[0].displayName, QStringLiteral("Mail"));
    QCOMPARE(items[0].userName, QStringLiteral("carol"));
    QCOMPARE(items[0].password, QStringLiteral("s3cret"));
    QCOMPARE(items[1].displayName, QString::fromUtf8("Bänk"));
    QCOMPARE(items[1].password, QString::fromUtf8("geld €"));

    // Malformed content is an error value of the plugin, not a crash.
    content = "only;two\n";
    QVERIFY(loaded.plugin->importPasswords(QStringLiteral("csv-file"), &error).isEmpty());
    QCOMPARE(error.kind, PluginError::Kind::Plugin);
    QCOMPARE(error.code, QStringLiteral("invalid-argument"));
    QCOMPARE(error.message, QStringLiteral("line 1: expected 3 fields, found 2"));

    // Binary garbage (invalid UTF-8) with every byte value passes through unharmed.
    content.clear();
    for (int i = 0; i < 256; ++i)
        content.append(char(i));
    QVERIFY(loaded.plugin->importPasswords(QStringLiteral("csv-file"), &error).isEmpty());
    QCOMPARE(error.code, QStringLiteral("invalid-argument"));
    QCOMPARE(error.message, QStringLiteral("file is not valid UTF-8"));
}

void PluginPocTest::testFilePickerCancelled() {
    int dialogs = 0;
    PluginHost host;
    host.pickFile = [&](const QString &, const QStringList &) {
        ++dialogs;
        return std::optional<PluginHost::PickedFile>();
    };
    const Loaded loaded = loadImporter(host);
    QVERIFY2(loaded.plugin, qPrintable(loaded.error.message));

    PluginError error;
    QVERIFY(loaded.plugin->importPasswords(QStringLiteral("csv-file"), &error).isEmpty());
    QCOMPARE(dialogs, 1);
    QCOMPARE(error.kind, PluginError::Kind::Plugin);
    QCOMPARE(error.code, QStringLiteral("cancelled"));
}

void PluginPocTest::testPluginErrorIsReported() {
    const Loaded loaded = loadImporter();
    QVERIFY2(loaded.plugin, qPrintable(loaded.error.message));

    PluginError error;
    QVERIFY(loaded.plugin->importPasswords(QStringLiteral("test-error"), &error).isEmpty());
    QCOMPARE(error.kind, PluginError::Kind::Plugin);
    QCOMPARE(error.code, QStringLiteral("unavailable"));
    QCOMPARE(error.message, QStringLiteral("requested test error"));

    QVERIFY(loaded.plugin->importPasswords(QStringLiteral("no such source"), &error).isEmpty());
    QCOMPARE(error.code, QStringLiteral("not-found"));

    // An error value is a normal return: the instance keeps working.
    QCOMPARE(loaded.plugin->importPasswords(QStringLiteral("fixed"), &error).size(), 2);
}

void PluginPocTest::testTrapDoesNotCrashHost() {
    const Loaded loaded = loadImporter();
    QVERIFY2(loaded.plugin, qPrintable(loaded.error.message));

    PluginError error;
    QVERIFY(loaded.plugin->importPasswords(QStringLiteral("test-trap"), &error).isEmpty());
    QCOMPARE(error.kind, PluginError::Kind::Trap);
    QVERIFY(!error.message.isEmpty());

    // The crashed instance is never entered again ...
    QVERIFY(loaded.plugin->importPasswords(QStringLiteral("fixed"), &error).isEmpty());
    QCOMPARE(error.kind, PluginError::Kind::Unusable);
    QVERIFY(loaded.plugin->sources(&error).isEmpty());
    QCOMPARE(error.kind, PluginError::Kind::Unusable);

    // ... but the host is fine and a fresh instance of the same plugin works.
    const Loaded fresh = loadImporter();
    QVERIFY2(fresh.plugin, qPrintable(fresh.error.message));
    QCOMPARE(fresh.plugin->importPasswords(QStringLiteral("fixed"), &error).size(), 2);
    QVERIFY(error.ok());
}

void PluginPocTest::testInfiniteLoopInterrupted() {
    PluginLimits limits;
    limits.callTimeoutMs = 200;
    const Loaded loaded = loadImporter({}, limits);
    QVERIFY2(loaded.plugin, qPrintable(loaded.error.message));

    QElapsedTimer timer;
    timer.start();
    PluginError error;
    QVERIFY(loaded.plugin->importPasswords(QStringLiteral("test-infinite-loop"), &error).isEmpty());
    const qint64 elapsed = timer.elapsed();
    QCOMPARE(error.kind, PluginError::Kind::Timeout);
    QVERIFY2(elapsed >= 200, qPrintable(QString::number(elapsed)));
    QVERIFY2(elapsed < 5000, qPrintable(QString::number(elapsed)));

    QVERIFY(loaded.plugin->importPasswords(QStringLiteral("fixed"), &error).isEmpty());
    QCOMPARE(error.kind, PluginError::Kind::Unusable);
}

void PluginPocTest::testTimeoutDoesNotAffectOtherPlugin() {
    // One plugin hangs on a worker thread while another one keeps answering on this thread.
    PluginLimits shortLimit;
    shortLimit.callTimeoutMs = 400;
    PluginError hangingError;
    std::thread worker([&] {
        const Loaded hanging = loadImporter({}, shortLimit);
        if (hanging.plugin)
            hanging.plugin->importPasswords(QStringLiteral("test-infinite-loop"), &hangingError);
    });

    const Loaded healthy = loadImporter();
    QVERIFY2(healthy.plugin, qPrintable(healthy.error.message));
    QElapsedTimer timer;
    timer.start();
    int calls = 0;
    while (timer.elapsed() < 800) {
        PluginError error;
        QCOMPARE(healthy.plugin->importPasswords(QStringLiteral("fixed"), &error).size(), 2);
        QVERIFY2(error.ok(), qPrintable(error.message));
        ++calls;
    }
    worker.join();
    QVERIFY(calls > 0);
    QCOMPARE(hangingError.kind, PluginError::Kind::Timeout);
}

void PluginPocTest::testMemoryCapHit() {
    PluginLimits limits;
    limits.maxMemoryBytes = 16 * 1024 * 1024;
    limits.callTimeoutMs = 60'000; // must be stopped by the memory cap, not by the timeout
    const Loaded loaded = loadImporter({}, limits);
    QVERIFY2(loaded.plugin, qPrintable(loaded.error.message));

    QElapsedTimer timer;
    timer.start();
    PluginError error;
    QVERIFY(loaded.plugin->importPasswords(QStringLiteral("test-allocate"), &error).isEmpty());
    QCOMPARE(error.kind, PluginError::Kind::Trap);
    QVERIFY2(timer.elapsed() < 30'000, "stopped by the timeout instead of the memory cap");

    // With the cap the plugin can still do normal work in a fresh instance.
    const Loaded fresh = loadImporter({}, limits);
    QVERIFY2(fresh.plugin, qPrintable(fresh.error.message));
    QCOMPARE(fresh.plugin->importPasswords(QStringLiteral("fixed"), &error).size(), 2);

    // A cap below the plugin's initial memory makes instantiation fail cleanly.
    limits.maxMemoryBytes = 64 * 1024;
    const Loaded tiny = loadImporter({}, limits);
    QVERIFY(!tiny.plugin);
    QCOMPARE(tiny.error.kind, PluginError::Kind::Instantiate);
}

void PluginPocTest::testDialogTimeNotCounted() {
    // The user may think about the file dialog longer than the call timeout.
    PluginLimits limits;
    limits.callTimeoutMs = 150;
    PluginHost host;
    host.pickFile = [](const QString &, const QStringList &) {
        QThread::msleep(500);
        return std::optional<PluginHost::PickedFile>({QStringLiteral("a.csv"), QByteArray("a;b;c\n")});
    };
    const Loaded loaded = loadImporter(host, limits);
    QVERIFY2(loaded.plugin, qPrintable(loaded.error.message));

    PluginError error;
    const QList<ImportedPassword> items = loaded.plugin->importPasswords(QStringLiteral("csv-file"), &error);
    QVERIFY2(error.ok(), qPrintable(error.message));
    QCOMPARE(items.size(), 1);
}
