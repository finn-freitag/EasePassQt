#include "PluginTests.h"

#include "IClock.h"
#include "PluginInspector.h"
#include "PluginTestSupport.h"

#include <QTemporaryDir>
#include <QTest>
#include <thread>

using namespace EasePass::Plugin;
using namespace PluginTest;

namespace {

const QString Importer = QStringLiteral("easepass:plugin/importer");
const QStringList AllPermissions = {QStringLiteral("storage"), QStringLiteral("pick-file"), QStringLiteral("network"),
                                    QStringLiteral("leak-check")};

struct FixedClock : EasePass::Core::IClock {
    qint64 now = 1'700'000'000;
    qint64 currentSecsSinceEpoch() const override { return now; }
};

std::unique_ptr<PluginInstance> instantiate(const QString &name, const HostServices &services,
                                            const QStringList &approved, PluginError *error,
                                            PluginLimits limits = {}) {
    return PluginInstance::create(component(name), buildHostInterfaces(services, approved), limits, error);
}

std::unique_ptr<PluginInstance> importer(const HostServices &services = {}, PluginLimits limits = {}) {
    PluginError error;
    auto instance = instantiate(QStringLiteral("importer"), services, {QStringLiteral("pick-file")}, &error, limits);
    if (!instance)
        qWarning("importer not instantiated: %s", qPrintable(error.message));
    return instance;
}

/// import-passwords(source) with the result unwrapped.
Value importPasswords(PluginInstance &instance, const QString &source, PluginError *error) {
    const Value raw = instance.call(Importer, QStringLiteral("import-passwords"), {Value::string(source)}, 0, error);
    return error->ok() ? PluginError::unwrap(raw, error) : Value();
}

} // namespace

void PluginRuntimeTest::testInspectConformancePlugins_data() {
    QTest::addColumn<QString>("name");
    QTest::addColumn<QStringList>("types");
    QTest::addColumn<QStringList>("permissions");

    // Must equal expected.json of each conformance plugin in the spec repository.
    QTest::newRow("about") << "about" << QStringList{"about", "initializer"} << QStringList{};
    QTest::newRow("importer") << "importer" << QStringList{"importer"} << QStringList{"pick-file"};
    QTest::newRow("database-source") << "database-source" << QStringList{"database-source"} << QStringList{"storage"};
    QTest::newRow("password-generator")
        << "password-generator" << QStringList{"password-generator"} << QStringList{"leak-check"};
    QTest::newRow("store-feed") << "store-feed" << QStringList{"store-feed"} << QStringList{};
    QTest::newRow("network") << "network" << QStringList{"importer"} << QStringList{"network"};
}

void PluginRuntimeTest::testInspectConformancePlugins() {
    QFETCH(QString, name);
    QFETCH(QStringList, types);
    QFETCH(QStringList, permissions);

    const PluginInspection inspection = PluginInspector::inspect(component(name));
    QVERIFY2(inspection.accepted(), qPrintable(inspection.rejected.join("; ")));
    QCOMPARE(inspection.types, types);
    QCOMPARE(inspection.permissions, permissions);
    QVERIFY(!inspection.exports.isEmpty());
}

void PluginRuntimeTest::testInspectDoesNotNeedApproval() {
    // Inspection reports the raw imports without linking or running anything.
    const PluginInspection inspection = PluginInspector::inspect(component(QStringLiteral("importer")));
    QVERIFY(inspection.imports.contains(QStringLiteral("easepass:plugin/file-picker@1.0.0")));
    QVERIFY(inspection.imports.contains(QStringLiteral("easepass:plugin/log@1.0.0")));
    QCOMPARE(inspection.exports, QStringList{QStringLiteral("easepass:plugin/importer@1.0.0")});
    QCOMPARE(PluginInspector::importOfPermission(QStringLiteral("pick-file")),
             QStringLiteral("easepass:plugin/file-picker"));
}

void PluginRuntimeTest::testForbiddenImportsRejected_data() {
    QTest::addColumn<QString>("file");
    QTest::addColumn<QString>("reason");

    QTest::newRow("wasi:filesystem") << "negative/forbidden-import.wasm"
                                     << "import 'wasi:filesystem/preopens@0.2.0' is not on the allow-list";
    QTest::newRow("unknown namespace") << "negative/unknown-namespace.wasm"
                                       << "import 'evil:backdoor/shell@1.0.0' is not on the allow-list";
    QTest::newRow("future api version") << "negative/future-api-version.wasm"
                                        << "import 'easepass:plugin/storage@2.0.0' has an unsupported version";
}

void PluginRuntimeTest::testForbiddenImportsRejected() {
    QFETCH(QString, file);
    QFETCH(QString, reason);

    const QByteArray bytes = fixture(file);
    const PluginInspection inspection = PluginInspector::inspect(bytes);
    QVERIFY(!inspection.accepted());
    QVERIFY2(inspection.rejected.contains(reason), qPrintable(inspection.rejected.join("; ")));
    QVERIFY(inspection.permissions.isEmpty());

    // Instantiation refuses it as well, even if the caller approves everything.
    PluginError error;
    QVERIFY(!PluginInstance::create(bytes, buildHostInterfaces({}, AllPermissions), {}, &error));
    QCOMPARE(error.kind, PluginError::Kind::Rejected);
}

void PluginRuntimeTest::testNotAComponentRejected() {
    for (const QByteArray &bytes : {QByteArray(), QByteArray("not wasm at all"),
                                    // a valid but empty core module (not a component)
                                    QByteArray::fromHex("0061736d01000000")}) {
        QVERIFY(!PluginInspector::inspect(bytes).accepted());
        PluginError error;
        QVERIFY(!PluginInstance::create(bytes, buildHostInterfaces({}, AllPermissions), {}, &error));
        QCOMPARE(error.kind, PluginError::Kind::Rejected);
    }
}

void PluginRuntimeTest::testHostInterfaceOnlyWhenApproved_data() {
    QTest::addColumn<QString>("plugin");
    QTest::addColumn<QString>("permission");

    QTest::newRow("pick-file") << "importer" << "pick-file";
    QTest::newRow("storage") << "database-source" << "storage";
    QTest::newRow("leak-check") << "password-generator" << "leak-check";
    QTest::newRow("network") << "network" << "network";
}

void PluginRuntimeTest::testHostInterfaceOnlyWhenApproved() {
    QFETCH(QString, plugin);
    QFETCH(QString, permission);
    const QString interface = PluginInspector::importOfPermission(permission).section(QLatin1Char('/'), 1);

    // Nothing approved: the interface is not in the linker, the component cannot be instantiated.
    PluginError error;
    QVERIFY(!instantiate(plugin, {}, {}, &error));
    QCOMPARE(error.kind, PluginError::Kind::Instantiate);
    QVERIFY2(error.message.contains(interface), qPrintable(error.message));

    // Every OTHER permission approved: still not.
    QStringList others = AllPermissions;
    others.removeAll(permission);
    QVERIFY(!instantiate(plugin, {}, others, &error));
    QCOMPARE(error.kind, PluginError::Kind::Instantiate);

    // Exactly the needed one: works.
    QVERIFY2(instantiate(plugin, {}, {permission}, &error), qPrintable(error.message));

    // And the builder really leaves unapproved interfaces out.
    for (const HostInterface &hostInterface : buildHostInterfaces({}, others))
        QVERIFY(!hostInterface.name.contains(QLatin1Char('/') + interface + QLatin1Char('@')));
}

void PluginRuntimeTest::testImportFixedItems() {
    QStringList log;
    HostServices services;
    services.log = [&](const QString &level, const QString &message) { log << level + QLatin1Char(':') + message; };
    const auto plugin = importer(services);
    QVERIFY(plugin);

    PluginError error;
    const Value sources = plugin->call(Importer, QStringLiteral("sources"), {}, 0, &error);
    QVERIFY2(error.ok(), qPrintable(error.message));
    QCOMPARE(sources.items().size(), 2);
    QCOMPARE(sources.items()[0].field("id").toString(), QStringLiteral("fixed"));
    QCOMPARE(sources.items()[1].field("description").toString(),
             QStringLiteral("Lines of 'display name;user name;password'."));

    const QList<Value> items = importPasswords(*plugin, QStringLiteral("fixed"), &error).items();
    QVERIFY2(error.ok(), qPrintable(error.message));
    QCOMPARE(log, QStringList{QStringLiteral("info:conformance-importer: importing fixed items")});
    QCOMPARE(items.size(), 2);
    QCOMPARE(items[0].field("display-name").toString(), QStringLiteral("Example"));
    QCOMPARE(items[0].field("password").toString(), QStringLiteral("correct horse battery staple"));
    QCOMPARE(items[0].field("tags").toStringList(), (QStringList{"conformance", "example"}));
    QVERIFY(!items[0].field("totp").isSome());
    QCOMPARE(items[1].field("display-name").toString(), QString::fromUtf8("Ünïcödé ✓"));
    QCOMPARE(items[1].field("password").toString(), QString::fromUtf8("pässwörd-🔑"));
    const Value totp = items[1].field("totp").payload();
    QCOMPARE(totp.field("secret").toString(), QStringLiteral("JBSWY3DPEHPK3PXP"));
    QCOMPARE(totp.field("digits").toU64(), 6u);
    QCOMPARE(totp.field("interval").toU64(), 30u);
    QCOMPARE(totp.field("algorithm").toString(), QStringLiteral("sha1"));

    // The instance stays usable for further calls.
    QCOMPARE(importPasswords(*plugin, QStringLiteral("fixed"), &error).items().size(), 2);
    QVERIFY(plugin->usable());
}

void PluginRuntimeTest::testImportCsvThroughFilePicker() {
    QString seenTitle;
    QStringList seenExtensions;
    QByteArray content = QString::fromUtf8("Mail;carol;s3cret\n\nBänk;dave;geld €\n").toUtf8();
    HostServices services;
    services.pickFile = [&](const QString &title, const QStringList &extensions) {
        seenTitle = title;
        seenExtensions = extensions;
        return std::optional<PickedFile>({QStringLiteral("export.csv"), content});
    };
    const auto plugin = importer(services);
    QVERIFY(plugin);

    PluginError error;
    const QList<Value> items = importPasswords(*plugin, QStringLiteral("csv-file"), &error).items();
    QVERIFY2(error.ok(), qPrintable(error.message));
    QCOMPARE(seenTitle, QStringLiteral("Choose a CSV file"));
    QCOMPARE(seenExtensions, QStringList{QStringLiteral("csv")});
    QCOMPARE(items.size(), 2);
    QCOMPARE(items[0].field("user-name").toString(), QStringLiteral("carol"));
    QCOMPARE(items[1].field("display-name").toString(), QString::fromUtf8("Bänk"));
    QCOMPARE(items[1].field("password").toString(), QString::fromUtf8("geld €"));

    // Malformed content is an error value of the plugin, not a crash.
    content = "only;two\n";
    importPasswords(*plugin, QStringLiteral("csv-file"), &error);
    QCOMPARE(error.kind, PluginError::Kind::Plugin);
    QCOMPARE(error.code, QStringLiteral("invalid-argument"));
    QCOMPARE(error.message, QStringLiteral("line 1: expected 3 fields, found 2"));

    // Binary data with every byte value passes through unharmed.
    content.clear();
    for (int i = 0; i < 256; ++i)
        content.append(char(i));
    importPasswords(*plugin, QStringLiteral("csv-file"), &error);
    QCOMPARE(error.code, QStringLiteral("invalid-argument"));
    QCOMPARE(error.message, QStringLiteral("file is not valid UTF-8"));
}

void PluginRuntimeTest::testFilePickerCancelledAndTooLarge() {
    int dialogs = 0;
    std::optional<PickedFile> answer;
    HostServices services;
    services.pickFile = [&](const QString &, const QStringList &) {
        ++dialogs;
        return answer;
    };
    const auto plugin = importer(services);
    QVERIFY(plugin);

    PluginError error;
    importPasswords(*plugin, QStringLiteral("csv-file"), &error);
    QCOMPARE(dialogs, 1);
    QCOMPARE(error.kind, PluginError::Kind::Plugin);
    QCOMPARE(error.code, QStringLiteral("cancelled"));

    // A file above the byte-list cap never reaches the plugin.
    answer = PickedFile{QStringLiteral("huge.csv"), QByteArray(PluginLimits::MaxByteListSize + 1, 'a')};
    importPasswords(*plugin, QStringLiteral("csv-file"), &error);
    QCOMPARE(error.code, QStringLiteral("too-large"));

    // No dialog available at all (headless host).
    const auto headless = importer({});
    QVERIFY(headless);
    importPasswords(*headless, QStringLiteral("csv-file"), &error);
    QCOMPARE(error.code, QStringLiteral("unsupported"));
}

void PluginRuntimeTest::testPluginErrorIsReported() {
    const auto plugin = importer();
    QVERIFY(plugin);

    PluginError error;
    importPasswords(*plugin, QStringLiteral("test-error"), &error);
    QCOMPARE(error.kind, PluginError::Kind::Plugin);
    QCOMPARE(error.code, QStringLiteral("unavailable"));
    QCOMPARE(error.message, QStringLiteral("requested test error"));

    importPasswords(*plugin, QStringLiteral("no such source"), &error);
    QCOMPARE(error.code, QStringLiteral("not-found"));

    // An error value is a normal return: the instance keeps working.
    QCOMPARE(importPasswords(*plugin, QStringLiteral("fixed"), &error).items().size(), 2);
}

void PluginRuntimeTest::testUnknownExportAndArguments() {
    const auto plugin = importer();
    QVERIFY(plugin);
    PluginError error;

    plugin->call(Importer, QStringLiteral("no-such-function"), {}, 0, &error);
    QCOMPARE(error.kind, PluginError::Kind::Host);
    plugin->call(QStringLiteral("easepass:plugin/database-source"), QStringLiteral("provider-name"), {}, 0, &error);
    QCOMPARE(error.kind, PluginError::Kind::Host);
    // Wrong number of arguments is caught before entering the plugin.
    plugin->call(Importer, QStringLiteral("import-passwords"), {}, 0, &error);
    QCOMPARE(error.kind, PluginError::Kind::Host);
    QVERIFY(plugin->usable());
    // A wrongly typed argument is refused by the runtime; the host survives.
    plugin->call(Importer, QStringLiteral("import-passwords"), {Value::u32(7)}, 0, &error);
    QVERIFY(!error.ok());
}

void PluginRuntimeTest::testTrapDoesNotCrashHost() {
    const auto plugin = importer();
    QVERIFY(plugin);

    PluginError error;
    importPasswords(*plugin, QStringLiteral("test-trap"), &error);
    QCOMPARE(error.kind, PluginError::Kind::Trap);
    QVERIFY(!error.message.isEmpty());
    QVERIFY(!plugin->usable());

    // The crashed instance is never entered again ...
    importPasswords(*plugin, QStringLiteral("fixed"), &error);
    QCOMPARE(error.kind, PluginError::Kind::Unusable);

    // ... but the host is fine and a fresh instance of the same plugin works.
    const auto fresh = importer();
    QVERIFY(fresh);
    QCOMPARE(importPasswords(*fresh, QStringLiteral("fixed"), &error).items().size(), 2);
    QVERIFY(error.ok());
}

void PluginRuntimeTest::testInfiniteLoopInterrupted() {
    PluginLimits limits;
    limits.callTimeoutMs = 200;
    const auto plugin = importer({}, limits);
    QVERIFY(plugin);

    QElapsedTimer timer;
    timer.start();
    PluginError error;
    importPasswords(*plugin, QStringLiteral("test-infinite-loop"), &error);
    const qint64 elapsed = timer.elapsed();
    QCOMPARE(error.kind, PluginError::Kind::Timeout);
    QVERIFY2(elapsed >= 200, qPrintable(QString::number(elapsed)));
    QVERIFY2(elapsed < 5000, qPrintable(QString::number(elapsed)));

    importPasswords(*plugin, QStringLiteral("fixed"), &error);
    QCOMPARE(error.kind, PluginError::Kind::Unusable);

    // A per-call timeout overrides the default of the instance.
    const auto second = importer();
    QVERIFY(second);
    timer.restart();
    const Value raw = second->call(Importer, QStringLiteral("import-passwords"),
                                   {Value::string(QStringLiteral("test-infinite-loop"))}, 150, &error);
    Q_UNUSED(raw);
    QCOMPARE(error.kind, PluginError::Kind::Timeout);
    QVERIFY(timer.elapsed() < 5000);
}

void PluginRuntimeTest::testTimeoutDoesNotAffectOtherPlugin() {
    // One plugin hangs on another thread while this one keeps answering.
    PluginLimits shortLimit;
    shortLimit.callTimeoutMs = 400;
    PluginError hangingError;
    std::thread other([&] {
        const auto hanging = importer({}, shortLimit);
        if (hanging)
            importPasswords(*hanging, QStringLiteral("test-infinite-loop"), &hangingError);
    });

    const auto healthy = importer();
    QVERIFY(healthy);
    QElapsedTimer timer;
    timer.start();
    int calls = 0;
    bool allOk = true;
    while (timer.elapsed() < 800) {
        PluginError error;
        allOk = allOk && importPasswords(*healthy, QStringLiteral("fixed"), &error).items().size() == 2 && error.ok();
        ++calls;
    }
    other.join();
    QVERIFY(allOk);
    QVERIFY(calls > 0);
    QCOMPARE(hangingError.kind, PluginError::Kind::Timeout);
}

void PluginRuntimeTest::testMemoryCapHit() {
    PluginLimits limits;
    limits.maxMemoryBytes = 16 * 1024 * 1024;
    limits.callTimeoutMs = 60'000; // must be stopped by the memory cap, not by the timeout
    const auto plugin = importer({}, limits);
    QVERIFY(plugin);

    QElapsedTimer timer;
    timer.start();
    PluginError error;
    importPasswords(*plugin, QStringLiteral("test-allocate"), &error);
    QCOMPARE(error.kind, PluginError::Kind::Trap);
    QVERIFY2(timer.elapsed() < 30'000, "stopped by the timeout instead of the memory cap");

    // With the cap the plugin can still do normal work in a fresh instance.
    const auto fresh = importer({}, limits);
    QVERIFY(fresh);
    QCOMPARE(importPasswords(*fresh, QStringLiteral("fixed"), &error).items().size(), 2);

    // A cap below the plugin's initial memory makes instantiation fail cleanly.
    limits.maxMemoryBytes = 64 * 1024;
    QVERIFY(!instantiate(QStringLiteral("importer"), {}, {QStringLiteral("pick-file")}, &error, limits));
    QCOMPARE(error.kind, PluginError::Kind::Instantiate);

    // The default cap (64 MiB) stops the allocation loop too.
    const auto standard = importer();
    QVERIFY(standard);
    importPasswords(*standard, QStringLiteral("test-allocate"), &error);
    QCOMPARE(error.kind, PluginError::Kind::Trap);
}

void PluginRuntimeTest::testDialogTimeNotCounted() {
    // The user may think about the file dialog longer than the call timeout.
    PluginLimits limits;
    limits.callTimeoutMs = 150;
    HostServices services;
    services.pickFile = [](const QString &, const QStringList &) {
        QThread::msleep(500);
        return std::optional<PickedFile>({QStringLiteral("a.csv"), QByteArray("a;b;c\n")});
    };
    const auto plugin = importer(services, limits);
    QVERIFY(plugin);

    PluginError error;
    QCOMPARE(importPasswords(*plugin, QStringLiteral("csv-file"), &error).items().size(), 1);
    QVERIFY2(error.ok(), qPrintable(error.message));
}

void PluginRuntimeTest::testCancelInterruptsCall() {
    PluginLimits limits;
    limits.callTimeoutMs = 60'000;
    limits.cancel = std::make_shared<std::atomic_bool>(false);
    const auto plugin = importer({}, limits);
    QVERIFY(plugin);

    std::thread canceller([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        limits.cancel->store(true);
    });
    QElapsedTimer timer;
    timer.start();
    PluginError error;
    importPasswords(*plugin, QStringLiteral("test-infinite-loop"), &error);
    canceller.join();
    QCOMPARE(error.kind, PluginError::Kind::Timeout);
    QVERIFY2(timer.elapsed() < 5000, qPrintable(QString::number(timer.elapsed())));
}

void PluginRuntimeTest::testLogSanitizedAndThrottled() {
    QCOMPARE(sanitizeLogMessage(QStringLiteral("a\nb\tc\x07" "d\x1b[31m") + QChar(0x202e) + QStringLiteral("e")),
             QStringLiteral("a b cd[31me"));
    const QString longText = sanitizeLogMessage(QString(10'000, QChar(0x00e4))); // 2 bytes each
    QVERIFY(longText.toUtf8().size() <= 4096);
    QVERIFY(longText.size() >= 2000);
    QVERIFY(!longText.contains(QChar(0xFFFD)));

    // A plugin that floods the log is cut off after a burst.
    QStringList lines;
    HostServices services;
    services.log = [&](const QString &level, const QString &message) { lines << level + QLatin1Char(':') + message; };
    HostFunction log;
    for (const HostInterface &interface : buildHostInterfaces(services, {})) {
        if (interface.name == QLatin1String("easepass:plugin/log@1.0.0"))
            log = interface.functions.value(QStringLiteral("log"));
    }
    QVERIFY(log.function);
    for (int i = 0; i < 500; ++i)
        log.function({Value::enumCase(QStringLiteral("error")), Value::string(QStringLiteral("line\n%1").arg(i))});
    QCOMPARE(lines.size(), 201);
    QCOMPARE(lines.first(), QStringLiteral("error:line 0"));
    QVERIFY(lines.last().startsWith(QStringLiteral("warning:")));
    // An unknown level cannot be smuggled in.
    lines.clear();
    HostFunction second;
    for (const HostInterface &interface : buildHostInterfaces(services, {}))
        if (interface.name == QLatin1String("easepass:plugin/log@1.0.0"))
            second = interface.functions.value(QStringLiteral("log"));
    second.function({Value::enumCase(QStringLiteral("critical] fake")), Value::string(QStringLiteral("x"))});
    QCOMPARE(lines, QStringList{QStringLiteral("info:x")});
}

void PluginRuntimeTest::testRandomAndClock() {
    // wasi:random through the generator plugin.
    int leakChecks = 0;
    int leakedAnswers = 3;
    HostServices services;
    services.leakCheck = [&](const QString &password) {
        ++leakChecks;
        LeakCheckOutcome outcome;
        outcome.status = leakChecks <= leakedAnswers ? QStringLiteral("leaked") : QStringLiteral("not-leaked");
        outcome.count = password.size();
        return outcome;
    };
    PluginError error;
    const auto generator =
        instantiate(QStringLiteral("password-generator"), services, {QStringLiteral("leak-check")}, &error);
    QVERIFY2(generator, qPrintable(error.message));
    const QString interface = QStringLiteral("easepass:plugin/password-generator");
    const auto generate = [&](quint32 length, const QString &characters, bool verify) {
        const Value options = Value::record({{QStringLiteral("length"), Value::u32(length)},
                                             {QStringLiteral("characters"), Value::string(characters)},
                                             {QStringLiteral("verify-not-leaked"), Value::boolean(verify)}});
        const Value raw = generator->call(interface, QStringLiteral("generate"), {options}, 0, &error);
        return error.ok() ? PluginError::unwrap(raw, &error).toString() : QString();
    };
    const QString first = generate(64, QStringLiteral("ab"), false);
    const QString second = generate(64, QStringLiteral("ab"), false);
    QCOMPARE(first.size(), 64);
    QVERIFY(first.count(QLatin1Char('a')) + first.count(QLatin1Char('b')) == 64);
    QVERIFY(first.contains(QLatin1Char('a')) && first.contains(QLatin1Char('b')));
    QVERIFY(first != second);
    QCOMPARE(leakChecks, 0);

    QCOMPARE(generate(12, QString::fromUtf8("äö€"), true).size(), 12);
    QCOMPARE(leakChecks, 4); // three leaked answers, then accepted
    leakChecks = 0;
    leakedAnswers = 1000;
    QCOMPARE(generate(12, QStringLiteral("abc"), true).size(), 12);
    QCOMPARE(leakChecks, 9); // gives up after 10 passwords

    generate(0, QStringLiteral("abc"), false);
    QCOMPARE(error.code, QStringLiteral("invalid-argument"));
    generate(5000, QStringLiteral("abc"), false);
    QCOMPARE(error.code, QStringLiteral("too-large"));

    // wasi:clocks and storage through the database-source plugin.
    QTemporaryDir directory;
    FixedClock clock;
    HostServices databaseServices;
    databaseServices.clock = &clock;
    databaseServices.storage = std::make_shared<PluginStorage>(directory.filePath(QStringLiteral("db")));
    const auto database =
        instantiate(QStringLiteral("database-source"), databaseServices, {QStringLiteral("storage")}, &error);
    QVERIFY2(database, qPrintable(error.message));
    const QString databaseInterface = QStringLiteral("easepass:plugin/database-source");
    Value raw = database->call(databaseInterface, QStringLiteral("write-database"),
                               {Value::string(QStringLiteral("main")), Value::bytes(QByteArray("\x00\x01\xff", 3))}, 0,
                               &error);
    QVERIFY(error.ok() && raw.isOk());
    raw = database->call(databaseInterface, QStringLiteral("get-database"), {Value::string(QStringLiteral("main"))}, 0,
                         &error);
    QCOMPARE(PluginError::unwrap(raw, &error).field("last-modified").payload().toU64(), quint64(clock.now));
    raw = database->call(databaseInterface, QStringLiteral("read-database"), {Value::string(QStringLiteral("main"))},
                         0, &error);
    QCOMPARE(PluginError::unwrap(raw, &error).toBytes(), QByteArray("\x00\x01\xff", 3));
}
