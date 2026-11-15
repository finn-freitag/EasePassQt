#include "PluginTests.h"

#include "DatabaseFile.h"
#include "DatabaseService.h"
#include "PluginAdapters.h"
#include "PluginManagerFixture.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>

using namespace EasePass::Plugin;
using namespace EasePass::Core;
using namespace PluginTest;

namespace {

// Test-only Argon2 parameters (steps.txt 3.3). Never used outside tests.
const KdfParams kFastKdf{1, 64, 1};
const char *kPassword = "correct horse";

/// Waits for the callback of an adapter call and returns its value.
template <typename T, typename Start> T await(Start start, PluginError *error) {
    bool done = false;
    T result{};
    QObject context;
    start(&context, [&](const T &value, const PluginError &e) {
        result = value;
        *error = e;
        done = true;
    });
    if (!waitUntil([&] { return done; }, 150'000))
        *error = {PluginError::Kind::Host, {}, QStringLiteral("test: no answer")};
    return result;
}

PluginError awaitDone(const std::function<void(QObject *, PluginDoneCallback)> &start) {
    bool done = false;
    PluginError result;
    QObject context;
    start(&context, [&](const PluginError &e) {
        result = e;
        done = true;
    });
    if (!waitUntil([&] { return done; }, 150'000))
        result = {PluginError::Kind::Host, {}, QStringLiteral("test: no answer")};
    return result;
}

QList<std::shared_ptr<PluginDatabaseSource>> listDatabases(PluginDatabaseProvider &provider, PluginError *error) {
    return await<QList<std::shared_ptr<PluginDatabaseSource>>>(
        [&](QObject *context, auto callback) { provider.listDatabases(context, callback); }, error);
}

Value validItem() {
    return Value::record({{"display-name", Value::string("n")},
                          {"user-name", Value::string("u")},
                          {"email", Value::string("e")},
                          {"website", Value::string("w")},
                          {"password", Value::string("p")},
                          {"notes", Value::string("x")},
                          {"tags", Value::stringList({"a", " ", "b"})},
                          {"totp", Value::none()}});
}

Value replaceField(const Value &record, const QString &name, const std::optional<Value> &value) {
    QList<Value::Field> fields;
    for (const Value::Field &field : record.fields()) {
        if (field.first != name)
            fields << field;
        else if (value)
            fields << Value::Field(name, *value);
    }
    return Value::record(fields);
}

} // namespace

void PluginAdaptersTest::testMissingInterface() {
    // Asking a plugin for a type it does not have is an error, not a crash.
    ManagerFixture fixture;
    PluginImporter importer(fixture.installAndLoad(QStringLiteral("store-feed")));
    PluginError error;
    const auto sources = await<QList<PluginImportSource>>(
        [&](QObject *context, auto callback) { importer.sources(context, callback); }, &error);
    QCOMPARE(error.kind, PluginError::Kind::Host);
    QVERIFY(sources.isEmpty());
}

void PluginAdaptersTest::testImporter() {
    ManagerFixture fixture;
    PluginImporter importer(fixture.installAndLoad(QStringLiteral("importer")));
    PluginError error;

    const auto sources = await<QList<PluginImportSource>>(
        [&](QObject *context, auto callback) { importer.sources(context, callback); }, &error);
    QVERIFY2(error.ok(), qPrintable(error.message));
    QCOMPARE(sources.size(), 2);
    QCOMPARE(sources[0].id, QStringLiteral("fixed"));
    QCOMPARE(sources[0].name, QStringLiteral("Fixed items"));
    QCOMPARE(sources[1].id, QStringLiteral("csv-file"));


    // Plugin items become PasswordItems the database understands.
    const auto items = await<QList<PasswordItem>>([&](QObject *context, auto callback) {
        importer.importPasswords(QStringLiteral("fixed"), context, callback);
    }, &error);
    QVERIFY2(error.ok(), qPrintable(error.message));
    QCOMPARE(items.size(), 2);
    QCOMPARE(items[0].displayName, QStringLiteral("Example"));
    QCOMPARE(items[0].username, QStringLiteral("alice"));
    QCOMPARE(items[0].email, QStringLiteral("alice@example.com"));
    QCOMPARE(items[0].website, QStringLiteral("https://example.com"));
    QCOMPARE(items[0].password, QStringLiteral("correct horse battery staple"));
    QCOMPARE(items[0].notes, QStringLiteral("First conformance item"));
    QCOMPARE(items[0].tags, (QStringList{"conformance", "example"}));
    QVERIFY(!items[0].has2FA());
    QCOMPARE(items[0].digits, QStringLiteral("6")); // defaults of PasswordItem stay
    QCOMPARE(items[0].algorithm, QStringLiteral("SHA1"));
    QVERIFY(items[0].clicks.isEmpty());
    QCOMPARE(items[1].displayName, QString::fromUtf8("Ünïcödé ✓"));
    QCOMPARE(items[1].password, QString::fromUtf8("pässwörd-🔑"));
    QVERIFY(items[1].has2FA());
    QCOMPARE(items[1].secret, QStringLiteral("JBSWY3DPEHPK3PXP"));
    QCOMPARE(items[1].digits, QStringLiteral("6"));
    QCOMPARE(items[1].interval, QStringLiteral("30"));
    QCOMPARE(items[1].algorithm, QStringLiteral("SHA1"));
    // They survive the database's own JSON round trip.
    QCOMPARE(PasswordItem::fromJson(items[1].toJson()).password, items[1].password);

    await<QList<PasswordItem>>([&](QObject *context, auto callback) {
        importer.importPasswords(QStringLiteral("test-error"), context, callback);
    }, &error);
    QCOMPARE(error.kind, PluginError::Kind::Plugin);
    QCOMPARE(error.code, QStringLiteral("unavailable"));

    // The callback is skipped when its context object is gone.
    bool called = false;
    {
        QObject shortLived;
        importer.importPasswords(QStringLiteral("fixed"), &shortLived,
                                 [&](const QList<PasswordItem> &, const PluginError &) { called = true; });
    }
    QTest::qWait(300);
    QVERIFY(!called);
}

void PluginAdaptersTest::testImporterFilePickerGetsPluginIdentity() {
    ManagerFixture fixture;
    QString seenId, seenName, seenTitle;
    QStringList seenExtensions;
    QThread *dialogThread = nullptr;
    fixture.environment.pickFile = [&](const QString &id, const QString &name, const QString &title,
                                       const QStringList &extensions) {
        seenId = id;
        seenName = name;
        seenTitle = title;
        seenExtensions = extensions;
        dialogThread = QThread::currentThread();
        return std::optional<PickedFile>({QStringLiteral("x.csv"), QByteArray("Site;me;pw\n")});
    };
    fixture.reopen();
    PluginImporter importer(fixture.installAndLoad(QStringLiteral("importer")));

    PluginError error;
    const auto items = await<QList<PasswordItem>>([&](QObject *context, auto callback) {
        importer.importPasswords(QStringLiteral("csv-file"), context, callback);
    }, &error);
    QVERIFY2(error.ok(), qPrintable(error.message));
    QCOMPARE(items.size(), 1);
    QCOMPARE(items[0].displayName, QStringLiteral("Site"));
    QCOMPARE(items[0].username, QStringLiteral("me"));
    QCOMPARE(items[0].password, QStringLiteral("pw"));
    // The host knows which plugin asks (from the install record, not from the plugin).
    QCOMPARE(seenId, QStringLiteral("org.easepass.conformance.importer"));
    QCOMPARE(seenName, QStringLiteral("Conformance: importer"));
    QCOMPARE(seenTitle, QStringLiteral("Choose a CSV file"));
    QCOMPARE(seenExtensions, QStringList{QStringLiteral("csv")});
    // Plugin code and its host calls run on the plugin's own thread, never on this one.
    QVERIFY(dialogThread && dialogThread != QThread::currentThread());
}

void PluginAdaptersTest::testTrapAndTimeoutAreErrorsAndPluginRecovers() {
    ManagerFixture fixture;
    fixture.environment.limits.callTimeoutMs = 300;
    fixture.reopen();
    PluginImporter importer(fixture.installAndLoad(QStringLiteral("importer")));
    PluginError error;
    const auto import = [&](const QString &source) {
        return await<QList<PasswordItem>>([&](QObject *context, auto callback) {
            importer.importPasswords(source, context, callback);
        }, &error);
    };

    QVERIFY(import(QStringLiteral("test-trap")).isEmpty());
    QCOMPARE(error.kind, PluginError::Kind::Trap);
    // The worker starts a fresh instance for the next call.
    QCOMPARE(import(QStringLiteral("fixed")).size(), 2);
    QVERIFY2(error.ok(), qPrintable(error.message));

    QElapsedTimer timer;
    timer.start();
    QVERIFY(import(QStringLiteral("test-infinite-loop")).isEmpty());
    QCOMPARE(error.kind, PluginError::Kind::Timeout);
    QVERIFY2(timer.elapsed() < 10'000, qPrintable(QString::number(timer.elapsed())));
    QCOMPARE(import(QStringLiteral("fixed")).size(), 2);

    QVERIFY(import(QStringLiteral("test-allocate")).isEmpty());
    QCOMPARE(error.kind, PluginError::Kind::Trap);
    QCOMPARE(import(QStringLiteral("fixed")).size(), 2);
    QVERIFY(error.ok());

    // Destroying the manager while a plugin hangs does not wait for the full timeout.
    fixture.environment.limits.callTimeoutMs = 120'000;
    fixture.reopen();
    {
        PluginImporter hanging(fixture.manager->load(QStringLiteral("org.easepass.conformance.importer")));
        QObject context;
        hanging.importPasswords(QStringLiteral("test-infinite-loop"), &context,
                                [](const QList<PasswordItem> &, const PluginError &) {});
        QTest::qWait(200); // let the call start
        timer.restart();
    }
    fixture.manager.reset();
    QVERIFY2(timer.elapsed() < 10'000, qPrintable(QString::number(timer.elapsed())));
}

void PluginAdaptersTest::testPasswordGenerator() {
    ManagerFixture fixture;
    QStringList checked;
    QMutex mutex;
    fixture.environment.leakCheck = [&](const QString &password) {
        QMutexLocker lock(&mutex);
        checked << password;
        LeakCheckOutcome outcome;
        outcome.status = checked.size() == 1 ? QStringLiteral("leaked") : QStringLiteral("not-leaked");
        outcome.count = checked.size() == 1 ? 7 : 0;
        return outcome;
    };
    fixture.reopen();
    PluginPasswordGenerator generator(fixture.installAndLoad(QStringLiteral("password-generator")));
    PluginError error;
    const auto generate = [&](int length, const QString &characters, bool verify) {
        return await<QString>([&](QObject *context, auto callback) {
            generator.generate(length, characters, verify, context, callback);
        }, &error);
    };

    const QString password = generate(20, QStringLiteral("abc123"), false);
    QVERIFY2(error.ok(), qPrintable(error.message));
    QCOMPARE(password.size(), 20);
    for (const QChar c : password)
        QVERIFY(QStringLiteral("abc123").contains(c));
    QVERIFY(checked.isEmpty());

    // With the leak check the first (leaked) candidate is thrown away.
    const QString verified = generate(16, QStringLiteral("abcdefgh"), true);
    QVERIFY(error.ok());
    QCOMPARE(checked.size(), 2);
    QCOMPARE(verified, checked.last());
    QVERIFY(verified != checked.first());

    QVERIFY(generate(0, QStringLiteral("abc"), false).isEmpty());
    QCOMPARE(error.code, QStringLiteral("invalid-argument"));
    QVERIFY(generate(-5, QStringLiteral("abc"), false).isEmpty());
    QCOMPARE(error.code, QStringLiteral("invalid-argument"));
    QVERIFY(generate(12, QString(), false).isEmpty());
    QCOMPARE(error.code, QStringLiteral("invalid-argument"));
    QVERIFY(generate(5000, QStringLiteral("abc"), false).isEmpty());
    QCOMPARE(error.code, QStringLiteral("too-large"));
}

void PluginAdaptersTest::testStoreFeed() {
    ManagerFixture fixture;
    PluginStoreFeed feed(fixture.installAndLoad(QStringLiteral("store-feed")));
    PluginError error;
    const auto sources = await<QList<PluginFeedSource>>(
        [&](QObject *context, auto callback) { feed.feedSources(context, callback); }, &error);
    QVERIFY2(error.ok(), qPrintable(error.message));
    QCOMPARE(sources.size(), 1);
    QCOMPARE(sources[0].name, QStringLiteral("Conformance catalog"));
    QCOMPARE(sources[0].url, QUrl(QStringLiteral("https://example.com/easepass/conformance/PluginList.json")));
}

void PluginAdaptersTest::testDatabaseProviderConfig() {
    ManagerFixture fixture;
    PluginDatabaseProvider provider(fixture.installAndLoad(QStringLiteral("database-source")));
    PluginError error;
    const auto text = [&](void (PluginDatabaseProvider::*getter)(QObject *, PluginCallback<QString>)) {
        return await<QString>([&](QObject *context, auto callback) { (provider.*getter)(context, callback); }, &error);
    };

    // The schema is what the host renders a form from.
    const QJsonObject schema = QJsonDocument::fromJson(text(&PluginDatabaseProvider::configSchema).toUtf8()).object();
    QCOMPARE(schema.value("type").toString(), QStringLiteral("object"));
    const QJsonObject properties = schema.value("properties").toObject();
    QCOMPARE(properties.keys(), (QStringList{"database_name", "read_only"}));
    QCOMPARE(properties.value("database_name").toObject().value("type").toString(), QStringLiteral("string"));
    QCOMPARE(properties.value("read_only").toObject().value("type").toString(), QStringLiteral("boolean"));

    QCOMPARE(QJsonDocument::fromJson(text(&PluginDatabaseProvider::config).toUtf8()).object(),
             (QJsonObject{{"database_name", "Conformance database"}, {"read_only", false}}));

    const auto setConfig = [&](const QString &json) {
        return awaitDone([&](QObject *context, PluginDoneCallback callback) { provider.setConfig(json, context, callback); });
    };
    QVERIFY(setConfig(QStringLiteral(R"({ "read_only": false, "database_name": "Mein \"Tresor\" ✓" })")).ok());
    QCOMPARE(QJsonDocument::fromJson(text(&PluginDatabaseProvider::config).toUtf8()).object(),
             (QJsonObject{{"database_name", QString::fromUtf8("Mein \"Tresor\" ✓")}, {"read_only", false}}));
    const auto databases = listDatabases(provider, &error);
    QCOMPARE(databases.size(), 1);
    QCOMPARE(databases[0]->databaseName(), QString::fromUtf8("Mein \"Tresor\" ✓"));

    // Rejected configurations come back with a message for the user and change nothing.
    for (const char *invalid : {"", "not json", R"({"database_name": "x"})", R"({"database_name": "", "read_only": false})",
                                R"({"database_name": "x", "read_only": false, "extra": 1})"}) {
        const PluginError rejected = setConfig(QLatin1String(invalid));
        QCOMPARE(rejected.kind, PluginError::Kind::Plugin);
        QCOMPARE(rejected.code, QStringLiteral("invalid-argument"));
        QVERIFY(!rejected.message.isEmpty());
    }
    QCOMPARE(listDatabases(provider, &error)[0]->databaseName(), QString::fromUtf8("Mein \"Tresor\" ✓"));
}

void PluginAdaptersTest::testDatabaseSource() {
    ManagerFixture fixture;
    PluginDatabaseProvider provider(fixture.installAndLoad(QStringLiteral("database-source")));
    PluginError error;
    const auto databases = listDatabases(provider, &error);
    QVERIFY2(error.ok(), qPrintable(error.message));
    QCOMPARE(databases.size(), 1);
    IDatabaseSource *source = databases[0].get(); // used through the interface DatabaseService sees

    QCOMPARE(databases[0]->databaseId(), QStringLiteral("main"));
    QCOMPARE(source->databaseName(), QStringLiteral("Conformance database"));
    QCOMPARE(source->sourceDescription(), QStringLiteral("Plugin storage (conformance)"));
    QVERIFY(!source->isReadOnly());
    QCOMPARE(source->availability(), IDatabaseSource::Availability::Available);
    QVERIFY(!source->lastModified().isValid());

    // Nothing saved yet.
    QVERIFY(!source->getBytes().has_value());
    QCOMPARE(databases[0]->lastError().code, QStringLiteral("not-found"));

    QByteArray bytes;
    for (int i = 0; i < 3000; ++i)
        bytes.append(char(i * 7));
    QSignalSpy changed(source, &IDatabaseSource::changed);
    QVERIFY(source->saveBytes(bytes));
    QVERIFY(databases[0]->lastError().ok());
    QCOMPARE(changed.count(), 1); // the modification time changed
    QCOMPARE(source->lastModified().toSecsSinceEpoch(), fixture.clock.now);
    QCOMPARE(source->getBytes().value_or(QByteArray()), bytes);

    // The plugin stored exactly the bytes it was given, inside its own directory.
    QFile stored(QDir(fixture.manager->storageDirectory(QStringLiteral("org.easepass.conformance.database-source")))
                     .filePath(QStringLiteral("files/main.epdb")));
    QVERIFY(stored.open(QIODevice::ReadOnly));
    QCOMPARE(stored.readAll(), bytes);

    source->login();
    QVERIFY(databases[0]->lastError().ok());
    source->logout();
    QVERIFY(databases[0]->lastError().ok());

    // refresh() polls the plugin: a change made elsewhere (second source object) is noticed.
    const auto second = listDatabases(provider, &error);
    fixture.clock.now += 100;
    QVERIFY(second[0]->saveBytes("newer"));
    changed.clear();
    databases[0]->refresh();
    QVERIFY(waitUntil([&] { return changed.count() == 1; }));
    QCOMPARE(source->lastModified().toSecsSinceEpoch(), fixture.clock.now);
    QCOMPARE(source->getBytes().value_or(QByteArray()), QByteArray("newer"));
    databases[0]->refresh();
    QTest::qWait(200);
    QCOMPARE(changed.count(), 1); // nothing changed, no signal

    // An empty database file is a valid value, not an error.
    QVERIFY(source->saveBytes(QByteArray()));
    QVERIFY(source->getBytes().has_value());
    QVERIFY(source->getBytes()->isEmpty());

    // Too large for the plugin boundary: refused before the plugin is called.
    QVERIFY(!source->saveBytes(QByteArray(PluginLimits::MaxByteListSize + 1, 'x')));
    QCOMPARE(databases[0]->lastError().kind, PluginError::Kind::Host);
    QVERIFY(source->getBytes()->isEmpty());
}

void PluginAdaptersTest::testDatabaseSourceReadOnly() {
    ManagerFixture fixture;
    const auto worker = fixture.installAndLoad(QStringLiteral("database-source"));
    PluginDatabaseProvider provider(worker);
    PluginError error;
    const auto writable = listDatabases(provider, &error);
    QVERIFY(writable[0]->saveBytes("content"));

    QVERIFY(awaitDone([&](QObject *context, PluginDoneCallback callback) {
        provider.setConfig(QStringLiteral(R"({"database_name":"Archive","read_only":true})"), context, callback);
    }).ok());
    const auto readOnly = listDatabases(provider, &error);
    QVERIFY(readOnly[0]->isReadOnly());
    QCOMPARE(readOnly[0]->databaseName(), QStringLiteral("Archive"));
    QVERIFY(!readOnly[0]->saveBytes("changed"));
    QCOMPARE(readOnly[0]->getBytes().value_or(QByteArray()), QByteArray("content"));

    // A source object that still believes it is writable is stopped by the plugin itself.
    QVERIFY(!writable[0]->isReadOnly());
    QSignalSpy changed(writable[0].get(), &IDatabaseSource::changed);
    QVERIFY(!writable[0]->saveBytes("changed"));
    QCOMPARE(writable[0]->lastError().code, QStringLiteral("not-allowed"));
    QCOMPARE(writable[0]->getBytes().value_or(QByteArray()), QByteArray("content"));
    writable[0]->refresh();
    QVERIFY(waitUntil([&] { return changed.count() == 1; }));
    QVERIFY(writable[0]->isReadOnly());
}

void PluginAdaptersTest::testDatabaseSourceWithDatabaseService() {
    // The real DatabaseService opens, changes and saves a database that lives in a plugin.
    ManagerFixture fixture;
    PluginDatabaseProvider provider(fixture.installAndLoad(QStringLiteral("database-source")));
    PluginError error;
    const std::shared_ptr<PluginDatabaseSource> source = listDatabases(provider, &error).value(0);
    QVERIFY(source);

    DatabaseFile initial;
    initial.masterPassword = kPassword;
    PasswordItem first;
    first.displayName = QStringLiteral("First");
    first.password = QStringLiteral("pw1");
    initial.items = {first};
    const QByteArray encrypted = initial.serialize(kFastKdf);
    QVERIFY(!encrypted.isEmpty());
    QVERIFY(source->saveBytes(encrypted));

    DatabaseService service(kFastKdf);
    QSignalSpy opened(&service, &DatabaseService::openFinished);
    QVERIFY(service.open(source, kPassword));
    QVERIFY(opened.wait(60'000));
    QCOMPARE(opened.at(0).at(0).value<LoadResult>(), LoadResult::Success);
    QVERIFY(service.isOpen());
    QVERIFY(!service.isReadOnly());
    QCOMPARE(service.itemCount(), 1);
    QCOMPARE(service.items().first().displayName, QStringLiteral("First"));

    PasswordItem second;
    second.displayName = QStringLiteral("Second");
    second.password = QStringLiteral("pw2");
    QVERIFY(service.addItem(second));
    bool saved = false;
    DatabaseOpResult saveResult;
    service.forceSave([&](const DatabaseOpResult &result) {
        saveResult = result;
        saved = true;
    });
    QVERIFY(waitUntil([&] { return saved; }, 60'000));
    QVERIFY(saveResult.ok());
    service.shutdown();

    // What the plugin holds now is the encrypted database with both items ...
    const QByteArray stored = source->getBytes().value_or(QByteArray());
    QVERIFY(stored != encrypted);
    DatabaseFile reloaded;
    QCOMPARE(DatabaseFile::loadFromBytes(stored, kPassword, reloaded, SecretString(), kFastKdf), LoadResult::Success);
    QCOMPARE(reloaded.items.size(), 2);
    QCOMPARE(reloaded.items.last().displayName, QStringLiteral("Second"));
    // ... and it never saw a plain-text password or item name.
    for (const char *secret : {"pw1", "pw2", "First", "Second", "correct horse"})
        QVERIFY2(!stored.contains(secret), secret);

    // Wrong password: reported by the service as usual.
    DatabaseService wrong(kFastKdf);
    QSignalSpy failed(&wrong, &DatabaseService::openFinished);
    QVERIFY(wrong.open(source, "not the password"));
    QVERIFY(failed.wait(60'000));
    QCOMPARE(failed.at(0).at(0).value<LoadResult>(), LoadResult::WrongPassword);
    wrong.shutdown();
}

void PluginAdaptersTest::testUntrustedValuesRejected() {
    // Values come from plugin code: wrong shapes are refused, never half-converted.
    PasswordItem item;
    QVERIFY(PluginImporter::toPasswordItem(validItem(), &item));
    QCOMPARE(item.tags, (QStringList{"a", "b"})); // blank tags are dropped
    QCOMPARE(item.username, QStringLiteral("u"));

    PasswordItem untouched;
    untouched.displayName = QStringLiteral("keep");
    QVERIFY(!PluginImporter::toPasswordItem(Value::string("not a record"), &untouched));
    QVERIFY(!PluginImporter::toPasswordItem(replaceField(validItem(), "password", std::nullopt), &untouched));
    QVERIFY(!PluginImporter::toPasswordItem(replaceField(validItem(), "password", Value::u32(5)), &untouched));
    QVERIFY(!PluginImporter::toPasswordItem(replaceField(validItem(), "tags", Value::string("a")), &untouched));
    QVERIFY(!PluginImporter::toPasswordItem(replaceField(validItem(), "tags", Value::list({Value::u8(1)})), &untouched));
    QVERIFY(!PluginImporter::toPasswordItem(replaceField(validItem(), "totp", Value::string("secret")), &untouched));
    const auto totp = [](const QString &algorithm) {
        return Value::some(Value::record({{"secret", Value::string("JBSWY3DPEHPK3PXP")},
                                          {"digits", Value::u8(8)},
                                          {"interval", Value::u32(60)},
                                          {"algorithm", Value::enumCase(algorithm)}}));
    };
    QVERIFY(!PluginImporter::toPasswordItem(replaceField(validItem(), "totp", totp("md5")), &untouched));
    QCOMPARE(untouched.displayName, QStringLiteral("keep"));
    QVERIFY(PluginImporter::toPasswordItem(replaceField(validItem(), "totp", totp("sha512")), &item));
    QCOMPARE(item.algorithm, QStringLiteral("SHA512"));
    QCOMPARE(item.digits, QStringLiteral("8"));
    QCOMPARE(item.interval, QStringLiteral("60"));

    const Value info = Value::record({{"id", Value::string("main")},
                                      {"name", Value::string("n")},
                                      {"description", Value::string("d")},
                                      {"read-only", Value::boolean(true)},
                                      {"availability", Value::enumCase("locked-by-other-user")},
                                      {"last-modified", Value::some(Value::u64(1'700'000'000))}});
    PluginDatabaseSource::Info parsed;
    QVERIFY(PluginDatabaseSource::toInfo(info, &parsed));
    QVERIFY(parsed.readOnly);
    QCOMPARE(parsed.availability, IDatabaseSource::Availability::LockedByOtherUser);
    QCOMPARE(parsed.lastModified.toSecsSinceEpoch(), 1'700'000'000);
    QVERIFY(PluginDatabaseSource::toInfo(replaceField(info, "availability", Value::enumCase("made-up")), &parsed));
    QCOMPARE(parsed.availability, IDatabaseSource::Availability::UnknownState);
    QVERIFY(!PluginDatabaseSource::toInfo(replaceField(info, "read-only", Value::string("yes")), &parsed));
    QVERIFY(!PluginDatabaseSource::toInfo(replaceField(info, "id", std::nullopt), &parsed));
    QVERIFY(!PluginDatabaseSource::toInfo(Value::list({}), &parsed));

    // A function that should return result<T, error> but returns something else.
    PluginError error;
    PluginError::unwrap(Value::string("surprise"), &error);
    QCOMPARE(error.kind, PluginError::Kind::Host);
    error = {};
    QCOMPARE(PluginError::unwrap(Value::ok(Value::u32(5)), &error).toU64(), 5u);
    QVERIFY(error.ok());
    QVERIFY(PluginError::unwrap(Value::error("timeout", "slow"), &error).isUnit());
    QCOMPARE(error.kind, PluginError::Kind::Plugin);
    QCOMPARE(error.code, QStringLiteral("timeout"));
    QCOMPARE(error.message, QStringLiteral("slow"));
}
