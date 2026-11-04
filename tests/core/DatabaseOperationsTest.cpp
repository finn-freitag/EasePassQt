#include "DatabaseOperationsTest.h"

#include "AppSettings.h"
#include "DatabaseOperations.h"
#include "DatabaseService.h"
#include "FakeDatabaseSource.h"
#include "LocalFileDatabaseSource.h"
#include "SecondFactor.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <optional>

using namespace EasePass::Core;

namespace {

// Test-only Argon2 parameters (steps.txt 3.3). Never used outside tests.
const KdfParams kFastKdf{1, 64, 1};
const char *kPassword = "correct horse";

std::unique_ptr<QTemporaryDir> g_dir;

QString tmpPath(const QString &name) {
    return g_dir->filePath(name);
}

PasswordItem makeItem(const QString &name, const QString &password = QStringLiteral("pw")) {
    PasswordItem item;
    item.displayName = name;
    item.password = password;
    return item;
}

QByteArray makeDbBytes(const QList<PasswordItem> &items, const SecretString &password = kPassword,
                       const SecretString &secondFactor = SecretString(),
                       SecondFactorType type = SecondFactorType::Token) {
    DatabaseFile db;
    db.masterPassword = password;
    db.secondFactor = secondFactor;
    db.useSecondFactor = !secondFactor.isEmpty();
    db.secondFactorType = secondFactor.isEmpty() ? 0 : static_cast<int>(type);
    db.items = items;
    return db.serialize(kFastKdf);
}

void writeFile(const QString &path, const QByteArray &bytes) {
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    QCOMPARE(f.write(bytes), bytes.size());
}

QString writeDb(const QString &name, const QList<PasswordItem> &items, const SecretString &password = kPassword,
                const SecretString &secondFactor = SecretString()) {
    const QString path = tmpPath(name);
    writeFile(path, makeDbBytes(items, password, secondFactor));
    return path;
}

QByteArray readFile(const QString &path) {
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

/// Decrypts @p bytes with the test KDF. names receives the item names on success.
LoadResult decode(const QByteArray &bytes, const SecretString &password, const SecretString &secondFactor = {},
                  QStringList *names = nullptr, DatabaseFile *out = nullptr) {
    DatabaseFile db;
    const LoadResult res = DatabaseFile::loadFromBytes(bytes, password, db, secondFactor, kFastKdf);
    if (res == LoadResult::Success) {
        if (names) {
            names->clear();
            for (const auto &item : db.items) {
                *names << item.displayName;
            }
        }
        if (out) {
            out->useSecondFactor = db.useSecondFactor;
            out->secondFactorType = db.secondFactorType;
            out->items = db.items;
        }
    }
    return res;
}

QStringList itemNames(const QList<PasswordItem> &items) {
    QStringList names;
    for (const auto &item : items) {
        names << item.displayName;
    }
    return names;
}

/// Starts an operation taking a DatabaseOpCallback and waits for its (asynchronous) result.
template <typename Start>
DatabaseOpResult runOp(Start start) {
    std::optional<DatabaseOpResult> result;
    start([&result](const DatabaseOpResult &r) { result = r; });
    if (result) {
        // Callbacks must never be invoked synchronously.
        return DatabaseOpResult(DatabaseOpStatus::IoError, QStringLiteral("<callback was synchronous>"));
    }
    if (!QTest::qWaitFor([&result]() { return result.has_value(); }, 30000)) {
        return DatabaseOpResult(DatabaseOpStatus::IoError, QStringLiteral("<timeout>"));
    }
    return *result;
}

LoadResult openAndWait(DatabaseService &svc, const std::shared_ptr<IDatabaseSource> &source,
                       const SecretString &password = kPassword, const SecretString &secondFactor = SecretString()) {
    QSignalSpy spy(&svc, &DatabaseService::openFinished);
    if (!svc.open(source, password, secondFactor)) {
        return LoadResult::Error;
    }
    if (!spy.wait(30000)) {
        return LoadResult::Error;
    }
    return spy.at(0).at(0).value<LoadResult>();
}

LoadResult openLocal(DatabaseService &svc, const QString &path, const SecretString &password = kPassword,
                     const SecretString &secondFactor = SecretString()) {
    return openAndWait(svc, std::make_shared<LocalFileDatabaseSource>(path), password, secondFactor);
}

std::shared_ptr<FakeDatabaseSource> makeSource(const QList<PasswordItem> &items = {makeItem("A")}) {
    return std::make_shared<FakeDatabaseSource>(makeDbBytes(items));
}

std::shared_ptr<AppSettings> makeSettings(const QStringList &paths = {}) {
    auto settings = AppSettings::createInMemory();
    settings->setDatabasePaths(paths);
    return settings;
}

} // namespace

void DatabaseOperationsTest::init() {
    QStandardPaths::setTestModeEnabled(true);
    g_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(g_dir->isValid());
}

void DatabaseOperationsTest::cleanup() {
    g_dir.reset();
}

// --- Helpers / validation -------------------------------------------------------------------

void DatabaseOperationsTest::testSecondFactorTypesAndTokenGenerator() {
    QCOMPARE(SecondFactor::typeFromInt(0), SecondFactorType::None);
    QCOMPARE(SecondFactor::typeFromInt(1), SecondFactorType::Token);
    QCOMPARE(SecondFactor::typeFromInt(2), SecondFactorType::OTP);
    QCOMPARE(SecondFactor::typeFromInt(3), SecondFactorType::Authenticator);
    QCOMPARE(SecondFactor::typeFromInt(42), SecondFactorType::None);
    QVERIFY(SecondFactor::isSupported(SecondFactorType::None));
    QVERIFY(SecondFactor::isSupported(SecondFactorType::Token));
    QVERIFY(SecondFactor::isSupported(SecondFactorType::OTP));
    QVERIFY(!SecondFactor::isSupported(SecondFactorType::Authenticator));
    QVERIFY(!SecondFactor::requiresToken(SecondFactorType::None));
    QVERIFY(SecondFactor::requiresToken(SecondFactorType::Token));
    QVERIFY(SecondFactor::requiresToken(SecondFactorType::OTP));

    const SecretString token = SecondFactor::generateToken();
    QCOMPARE(token.size(), size_t(12));
    for (char c : token.toUtf8()) {
        QVERIFY(c >= '0' && c <= '9');
    }
    QVERIFY(SecondFactor::isSecureToken(token, 12, 4)); // C#: maxNumOccurence = length / 3
    QVERIFY(SecondFactor::generateToken() != token);
    QCOMPARE(SecondFactor::generateToken(0).size(), size_t(1));
    QCOMPARE(SecondFactor::generateToken(30).size(), size_t(30));

    QVERIFY(SecondFactor::isSecureToken("111222333444", 12, 4));
    QVERIFY(!SecondFactor::isSecureToken("111112222233", 12, 4));
    QVERIFY(!SecondFactor::isSecureToken("123", 12, 4));
}

void DatabaseOperationsTest::testSanitizeAndBuildDatabasePath() {
    QCOMPARE(DatabaseOperations::sanitizeDatabaseName("a<b>:c\"/d\\e|f?g*"), QString("abcdefg"));
    QCOMPARE(DatabaseOperations::sanitizeDatabaseName(QString("x") + QChar(0x01) + "y"), QString("xy"));
    QCOMPARE(DatabaseOperations::sanitizeDatabaseName("  My DB  "), QString("My DB"));
    QCOMPARE(DatabaseOperations::sanitizeDatabaseName(QString::fromUtf8("Pässwörter 🔑")),
             QString::fromUtf8("Pässwörter 🔑"));
    QCOMPARE(DatabaseOperations::sanitizeDatabaseName(".."), QString());
    QCOMPARE(DatabaseOperations::sanitizeDatabaseName("///"), QString());

    QCOMPARE(DatabaseOperations::buildDatabasePath("", "x"), QString());
    QCOMPARE(DatabaseOperations::buildDatabasePath(g_dir->path(), ""), QDir(g_dir->path()).filePath("Database.epdb"));
    QCOMPARE(DatabaseOperations::buildDatabasePath(g_dir->path(), "My/DB"), QDir(g_dir->path()).filePath("MyDB.epdb"));

    QCOMPARE(DatabaseOperations::defaultBackupFileName("My DB", QDateTime(QDate(2026, 1, 2), QTime(3, 4, 5))),
             QString("My DB_backup_2026-01-02_03-04-05.epdb"));
    QCOMPARE(DatabaseOperations::defaultBackupFileName("", QDateTime(QDate(2026, 1, 2), QTime(3, 4, 5))),
             QString("Database_backup_2026-01-02_03-04-05.epdb"));

    QVERIFY(DatabaseOperations::isSamePath(tmpPath("a/../b.epdb"), tmpPath("b.epdb")));
    QVERIFY(!DatabaseOperations::isSamePath(tmpPath("a.epdb"), tmpPath("b.epdb")));
    QVERIFY(!DatabaseOperations::isSamePath(QString(), QString()));

    LocalFileDatabaseSource local(tmpPath("x.epdb"));
    FakeDatabaseSource fake;
    QCOMPARE(DatabaseOperations::localPath(&local), tmpPath("x.epdb"));
    QCOMPARE(DatabaseOperations::localPath(&fake), QString());
    QCOMPARE(DatabaseOperations::localPath(nullptr), QString());
}

void DatabaseOperationsTest::testValidateNewPasswordAndCreate() {
    QCOMPARE(SecretString(QString::fromUtf8("äöü")).codePointCount(), qsizetype(3));

    QCOMPARE(DatabaseOperations::validateNewPassword("abc", "abc"), DatabaseOpStatus::PasswordTooShort);
    QCOMPARE(DatabaseOperations::validateNewPassword("abc", "abd"), DatabaseOpStatus::PasswordTooShort);
    QCOMPARE(DatabaseOperations::validateNewPassword("abcd", "abce"), DatabaseOpStatus::PasswordsDoNotMatch);
    QCOMPARE(DatabaseOperations::validateNewPassword("abcd", "abcd"), DatabaseOpStatus::Success);
    // Length counts characters, not UTF-8 bytes
    QCOMPARE(DatabaseOperations::validateNewPassword(QString::fromUtf8("äöü"), QString::fromUtf8("äöü")),
             DatabaseOpStatus::PasswordTooShort);
    QCOMPARE(DatabaseOperations::validateNewPassword(QString::fromUtf8("äöüß"), QString::fromUtf8("äöüß")),
             DatabaseOpStatus::Success);

    DatabaseOperations::CreateRequest req;
    req.path = tmpPath("new.epdb");
    req.password = "abcd";
    req.passwordRepeat = "abcd";
    QCOMPARE(DatabaseOperations::validateCreate(req), DatabaseOpStatus::Success);

    // C# order: match, path, length, exists
    DatabaseOperations::CreateRequest r = req;
    r.passwordRepeat = "x";
    r.path.clear();
    QCOMPARE(DatabaseOperations::validateCreate(r), DatabaseOpStatus::PasswordsDoNotMatch);
    r = req;
    r.path.clear();
    r.password = r.passwordRepeat = "ab";
    QCOMPARE(DatabaseOperations::validateCreate(r), DatabaseOpStatus::InvalidPath);
    r = req;
    r.path = tmpPath("in|valid.epdb");
    QCOMPARE(DatabaseOperations::validateCreate(r), DatabaseOpStatus::InvalidPath);
    r = req;
    r.password = r.passwordRepeat = "abc";
    QCOMPARE(DatabaseOperations::validateCreate(r), DatabaseOpStatus::PasswordTooShort);
    r = req;
    writeFile(tmpPath("exists.epdb"), "x");
    r.path = tmpPath("exists.epdb");
    QCOMPARE(DatabaseOperations::validateCreate(r), DatabaseOpStatus::PathAlreadyExists);
    r = req;
    r.secondFactorType = SecondFactorType::Authenticator;
    r.secondFactor = "123";
    QCOMPARE(DatabaseOperations::validateCreate(r), DatabaseOpStatus::UnsupportedSecondFactorType);
    r = req;
    r.secondFactorType = SecondFactorType::OTP;
    QCOMPARE(DatabaseOperations::validateCreate(r), DatabaseOpStatus::InvalidSecondFactor);

    QVERIFY(!DatabaseOpResult::describe(DatabaseOpStatus::PasswordTooShort).isEmpty());
    QVERIFY(DatabaseOpResult::describe(DatabaseOpStatus::Success).isEmpty());
    QCOMPARE(DatabaseOpResult::fromLoadResult(LoadResult::DatabaseNotFound), DatabaseOpStatus::NotFound);
    QCOMPARE(DatabaseOpResult::fromLoadResult(LoadResult::NeedsSecondFactor), DatabaseOpStatus::NeedsSecondFactor);
}

// --- File operations ------------------------------------------------------------------------

void DatabaseOperationsTest::testCreateDatabase() {
    auto settings = makeSettings();
    DatabaseOperations ops(settings, nullptr, kFastKdf);

    DatabaseOperations::CreateRequest req;
    req.path = DatabaseOperations::buildDatabasePath(g_dir->path(), "Created");
    req.password = req.passwordRepeat = kPassword;

    const DatabaseOpResult res = runOp([&](auto cb) { ops.createDatabase(req, cb); });
    QVERIFY2(res.ok(), qPrintable(res.message() + res.path));
    QCOMPARE(res.path, tmpPath("Created.epdb"));
    QVERIFY(QFile::exists(res.path));
    QCOMPARE(settings->databasePaths(), QStringList({res.path}));

    DatabaseFile db;
    QStringList names{"<unset>"};
    QCOMPARE(decode(readFile(res.path), kPassword, {}, &names, &db), LoadResult::Success);
    QVERIFY(names.isEmpty());
    QVERIFY(!db.useSecondFactor);
    QCOMPARE(db.secondFactorType, 0);
    QCOMPARE(decode(readFile(res.path), "wrong"), LoadResult::WrongPassword);

    // addToKnownDatabases = false
    req.path = tmpPath("Unlisted.epdb");
    req.addToKnownDatabases = false;
    QVERIFY(runOp([&](auto cb) { ops.createDatabase(req, cb); }).ok());
    QCOMPARE(settings->databasePaths().size(), 1);
}

void DatabaseOperationsTest::testCreateDatabaseWithSecondFactor() {
    auto settings = makeSettings();
    DatabaseOperations ops(settings, nullptr, kFastKdf);
    const SecretString token = SecondFactor::generateToken();

    DatabaseOperations::CreateRequest req;
    req.path = tmpPath("sf.epdb");
    req.password = req.passwordRepeat = kPassword;
    req.secondFactorType = SecondFactorType::Token;
    req.secondFactor = token;
    QVERIFY(runOp([&](auto cb) { ops.createDatabase(req, cb); }).ok());

    const QByteArray bytes = readFile(req.path);
    QCOMPARE(decode(bytes, kPassword), LoadResult::NeedsSecondFactor);
    QCOMPARE(decode(bytes, kPassword, "000000000000"), LoadResult::WrongPassword);
    DatabaseFile db;
    QCOMPARE(decode(bytes, kPassword, token, nullptr, &db), LoadResult::Success);
    QVERIFY(db.useSecondFactor);
    QCOMPARE(db.secondFactorType, static_cast<int>(SecondFactorType::Token));

    // Service-level: opening requires the second factor
    DatabaseService svc(kFastKdf);
    QCOMPARE(openLocal(svc, req.path), LoadResult::NeedsSecondFactor);
    QCOMPARE(openLocal(svc, req.path, kPassword, token), LoadResult::Success);
    QCOMPARE(svc.secondFactorType(), SecondFactorType::Token);
}

void DatabaseOperationsTest::testCreateDatabaseNeverOverwrites() {
    auto settings = makeSettings();
    DatabaseOperations ops(settings, nullptr, kFastKdf);
    writeFile(tmpPath("taken.epdb"), "original");

    DatabaseOperations::CreateRequest req;
    req.path = tmpPath("taken.epdb");
    req.password = req.passwordRepeat = kPassword;
    const DatabaseOpResult res = runOp([&](auto cb) { ops.createDatabase(req, cb); });
    QCOMPARE(res.status, DatabaseOpStatus::PathAlreadyExists);
    QCOMPARE(readFile(tmpPath("taken.epdb")), QByteArray("original"));
    QVERIFY(settings->databasePaths().isEmpty());

    // Missing folder -> IoError, nothing added
    req.path = tmpPath("missing/dir/db.epdb");
    QCOMPARE(runOp([&](auto cb) { ops.createDatabase(req, cb); }).status, DatabaseOpStatus::IoError);
    QVERIFY(settings->databasePaths().isEmpty());

    // Validation errors are reported through the (asynchronous) callback as well
    req.path = tmpPath("short.epdb");
    req.password = req.passwordRepeat = "abc";
    QCOMPARE(runOp([&](auto cb) { ops.createDatabase(req, cb); }).status, DatabaseOpStatus::PasswordTooShort);
    QVERIFY(!QFile::exists(req.path));
}

void DatabaseOperationsTest::testRenameDatabase() {
    const QString a = writeDb("a.epdb", {makeItem("A")});
    const QString other = writeDb("other.epdb", {});
    writeFile(LocalFileDatabaseSource::backupPathFor(a), "backup");
    auto settings = makeSettings({a, other});
    settings->setLastDatabase(a);
    DatabaseOperations ops(settings, nullptr, kFastKdf);

    const DatabaseOpResult res = ops.renameDatabase(a, "  re|named ");
    QVERIFY2(res.ok(), qPrintable(res.message()));
    QCOMPARE(res.path, tmpPath("renamed.epdb"));
    QVERIFY(!QFile::exists(a));
    QVERIFY(QFile::exists(res.path));
    QCOMPARE(readFile(LocalFileDatabaseSource::backupPathFor(res.path)), QByteArray("backup"));
    QVERIFY(!QFile::exists(LocalFileDatabaseSource::backupPathFor(a)));
    QCOMPARE(settings->databasePaths(), QStringList({res.path, other})); // position kept
    QCOMPARE(settings->lastDatabase(), res.path);
    QCOMPARE(decode(readFile(res.path), kPassword), LoadResult::Success);

    QCOMPARE(ops.renameDatabase(res.path, "renamed").status, DatabaseOpStatus::Success); // no-op
    QCOMPARE(ops.renameDatabase(res.path, " / ").status, DatabaseOpStatus::InvalidPath);
    QCOMPARE(ops.renameDatabase(tmpPath("nope.epdb"), "x").status, DatabaseOpStatus::NotFound);
}

void DatabaseOperationsTest::testRenameRejectsLoadedAndExisting() {
    const QString a = writeDb("a.epdb", {makeItem("A")});
    const QString b = writeDb("b.epdb", {makeItem("B")});
    auto settings = makeSettings({a, b});
    DatabaseService svc(kFastKdf);
    DatabaseOperations ops(settings, &svc, kFastKdf);

    QCOMPARE(openLocal(svc, a), LoadResult::Success);
    QCOMPARE(ops.renameDatabase(a, "x").status, DatabaseOpStatus::IsLoadedDatabase);
    QVERIFY(QFile::exists(a));

    const QByteArray bBytes = readFile(b);
    QCOMPARE(ops.renameDatabase(b, "a").status, DatabaseOpStatus::PathAlreadyExists);
    QCOMPARE(readFile(a).isEmpty(), false);
    QCOMPARE(readFile(b), bBytes);
    QCOMPARE(settings->databasePaths(), QStringList({a, b}));
}

void DatabaseOperationsTest::testDeleteDatabaseRemovesFromKnownList() {
    const QString a = writeDb("a.epdb", {});
    const QString b = writeDb("b.epdb", {});
    writeFile(LocalFileDatabaseSource::backupPathFor(a), "backup");
    auto settings = makeSettings({a, b});
    settings->setLastDatabase(a);
    DatabaseOperations ops(settings, nullptr, kFastKdf);

    const DatabaseOpResult res = ops.deleteDatabase(a);
    QVERIFY2(res.ok(), qPrintable(res.message()));
    QVERIFY(!QFile::exists(a));
    QVERIFY(!QFile::exists(LocalFileDatabaseSource::backupPathFor(a)));
    QCOMPARE(settings->databasePaths(), QStringList({b}));
    QVERIFY(settings->lastDatabase().isEmpty());

    // A known path whose file vanished is just removed from the list
    const QString c = tmpPath("c.epdb");
    settings->setDatabasePaths({b, c});
    QVERIFY(ops.deleteDatabase(c).ok());
    QCOMPARE(settings->databasePaths(), QStringList({b}));
    QVERIFY(QFile::exists(b));

    QCOMPARE(ops.deleteDatabase(QString()).status, DatabaseOpStatus::InvalidPath);
}

void DatabaseOperationsTest::testDeleteRejectsLastAndLoaded() {
    const QString a = writeDb("a.epdb", {makeItem("A")});
    const QString b = writeDb("b.epdb", {makeItem("B")});
    auto settings = makeSettings({a});
    DatabaseService svc(kFastKdf);
    DatabaseOperations ops(settings, &svc, kFastKdf);

    // C#: CantDeleteDatabase - the only known database
    QCOMPARE(ops.deleteDatabase(a).status, DatabaseOpStatus::LastDatabase);
    QVERIFY(QFile::exists(a));
    QCOMPARE(settings->databasePaths(), QStringList({a}));

    // C#: CantDeleteLoadedDatabase
    settings->setDatabasePaths({a, b});
    QCOMPARE(openLocal(svc, a), LoadResult::Success);
    QCOMPARE(ops.deleteDatabase(a).status, DatabaseOpStatus::IsLoadedDatabase);
    QVERIFY(QFile::exists(a));
    QCOMPARE(settings->databasePaths(), QStringList({a, b}));

    // The other one can be deleted
    QVERIFY(ops.deleteDatabase(b).ok());
    QCOMPARE(settings->databasePaths(), QStringList({a}));
}

void DatabaseOperationsTest::testImportDatabaseFile() {
    const QString a = writeDb("a.epdb", {makeItem("A")});
    const QString sf = writeDb("sf.epdb", {makeItem("S")}, kPassword, "123456789012");
    writeFile(tmpPath("garbage.epdb"), "not a database");
    auto settings = makeSettings();
    DatabaseOperations ops(settings, nullptr, kFastKdf);

    QCOMPARE(runOp([&](auto cb) { ops.importDatabaseFile(a, "wrong", {}, cb); }).status,
             DatabaseOpStatus::WrongPassword);
    QVERIFY(settings->databasePaths().isEmpty());

    const DatabaseOpResult ok = runOp([&](auto cb) { ops.importDatabaseFile(a, kPassword, {}, cb); });
    QVERIFY(ok.ok());
    QCOMPARE(ok.path, a);
    QCOMPARE(settings->databasePaths(), QStringList({a}));

    QCOMPARE(runOp([&](auto cb) { ops.importDatabaseFile(tmpPath("missing.epdb"), kPassword, {}, cb); }).status,
             DatabaseOpStatus::NotFound);
    QCOMPARE(runOp([&](auto cb) { ops.importDatabaseFile(tmpPath("garbage.epdb"), kPassword, {}, cb); }).status,
             DatabaseOpStatus::WrongFormat);
    QCOMPARE(runOp([&](auto cb) { ops.importDatabaseFile(sf, kPassword, {}, cb); }).status,
             DatabaseOpStatus::NeedsSecondFactor);
    QCOMPARE(runOp([&](auto cb) { ops.importDatabaseFile(sf, kPassword, "999999999999", cb); }).status,
             DatabaseOpStatus::WrongPassword);
    QCOMPARE(runOp([&](auto cb) { ops.importDatabaseFile(QString(), kPassword, {}, cb); }).status,
             DatabaseOpStatus::InvalidPath);
    QCOMPARE(settings->databasePaths(), QStringList({a}));
    QVERIFY(runOp([&](auto cb) { ops.importDatabaseFile(sf, kPassword, "123456789012", cb); }).ok());
    QCOMPARE(settings->databasePaths(), QStringList({a, sf}));
}

void DatabaseOperationsTest::testReadItemsFromFileWrongPassword() {
    const QString a = writeDb("a.epdb", {makeItem("X"), makeItem("Y")});
    DatabaseOperations ops(makeSettings(), nullptr, kFastKdf);

    std::optional<std::pair<DatabaseOpResult, QList<PasswordItem>>> result;
    auto read = [&](const SecretString &pw) {
        result.reset();
        ops.readItemsFromFile(a, pw, {}, [&](const DatabaseOpResult &r, const QList<PasswordItem> &items) {
            result = std::make_pair(r, items);
        });
        QVERIFY(!result); // asynchronous
        QTRY_VERIFY_WITH_TIMEOUT(result.has_value(), 30000);
    };

    read(kPassword);
    QVERIFY(result->first.ok());
    QCOMPARE(itemNames(result->second), QStringList({"X", "Y"}));

    read("wrong");
    QCOMPARE(result->first.status, DatabaseOpStatus::WrongPassword);
    QVERIFY(result->second.isEmpty());
}

// --- Operations on the open database --------------------------------------------------------

void DatabaseOperationsTest::testChangeMasterPassword() {
    const SecretString token = "123456789012";
    const QString path = writeDb("db.epdb", {makeItem("A"), makeItem("B")}, kPassword, token);
    DatabaseService svc(kFastKdf);
    QCOMPARE(openLocal(svc, path, kPassword, token), LoadResult::Success);
    QSignalSpy settingsChanged(&svc, &DatabaseService::databaseSettingsChanged);

    const DatabaseOpResult res =
        runOp([&](auto cb) { svc.changeMasterPassword(kPassword, "new password", "new password", cb); });
    QVERIFY2(res.ok(), qPrintable(res.message()));
    QCOMPARE(res.path, path);
    QCOMPARE(settingsChanged.count(), 1);
    QVERIFY(svc.isOpen());
    QVERIFY(!svc.isDirty());

    const QByteArray bytes = readFile(path);
    QStringList names;
    QCOMPARE(decode(bytes, "new password", token, &names), LoadResult::Success);
    QCOMPARE(names, QStringList({"A", "B"}));
    QCOMPARE(decode(bytes, kPassword, token), LoadResult::WrongPassword);

    // The new password is now the "current" one
    QCOMPARE(runOp([&](auto cb) { svc.changeMasterPassword(kPassword, "third one", "third one", cb); }).status,
             DatabaseOpStatus::WrongPassword);
    QVERIFY(runOp([&](auto cb) { svc.changeMasterPassword("new password", "third one", "third one", cb); }).ok());
    svc.close();
    QCOMPARE(openLocal(svc, path, "third one", token), LoadResult::Success);
}

void DatabaseOperationsTest::testChangeMasterPasswordValidation() {
    auto src = makeSource();
    DatabaseService svc(kFastKdf);
    QCOMPARE(openAndWait(svc, src), LoadResult::Success);

    // C# order: too short, current password, repeat
    QCOMPARE(runOp([&](auto cb) { svc.changeMasterPassword("wrong", "abc", "xyz", cb); }).status,
             DatabaseOpStatus::PasswordTooShort);
    QCOMPARE(runOp([&](auto cb) { svc.changeMasterPassword("wrong", "abcd", "xyz", cb); }).status,
             DatabaseOpStatus::WrongPassword);
    QCOMPARE(runOp([&](auto cb) { svc.changeMasterPassword("", "abcd", "abcd", cb); }).status,
             DatabaseOpStatus::WrongPassword);
    QCOMPARE(runOp([&](auto cb) { svc.changeMasterPassword(kPassword, "abcd", "abce", cb); }).status,
             DatabaseOpStatus::PasswordsDoNotMatch);
    QCOMPARE(src->saveCount(), 0);
    QCOMPARE(decode(src->bytes(), kPassword), LoadResult::Success);
}

void DatabaseOperationsTest::testChangeMasterPasswordSaveFailureRestoresOld() {
    auto src = makeSource({makeItem("A")});
    DatabaseService svc(kFastKdf);
    QCOMPARE(openAndWait(svc, src), LoadResult::Success);
    QSignalSpy settingsChanged(&svc, &DatabaseService::databaseSettingsChanged);
    QSignalSpy saveFailed(&svc, &DatabaseService::saveFailed);

    src->setFailSaves(true);
    QCOMPARE(runOp([&](auto cb) { svc.changeMasterPassword(kPassword, "new password", "new password", cb); }).status,
             DatabaseOpStatus::IoError);
    QCOMPARE(settingsChanged.count(), 2); // changed + restored
    QCOMPARE(saveFailed.count(), 1);

    // The old password is still the current one, and later saves use it
    src->setFailSaves(false);
    QVERIFY(runOp([&](auto cb) { svc.forceSave(cb); }).ok());
    QCOMPARE(decode(src->bytes(), kPassword), LoadResult::Success);
    QCOMPARE(decode(src->bytes(), "new password"), LoadResult::WrongPassword);
    QCOMPARE(runOp([&](auto cb) { svc.changeMasterPassword("new password", "abcd", "abcd", cb); }).status,
             DatabaseOpStatus::WrongPassword);
}

void DatabaseOperationsTest::testSetAndRemoveSecondFactor() {
    auto src = makeSource({makeItem("A")});
    DatabaseService svc(kFastKdf);
    QCOMPARE(openAndWait(svc, src), LoadResult::Success);
    QVERIFY(!svc.usesSecondFactor());
    QCOMPARE(svc.secondFactorType(), SecondFactorType::None);

    const SecretString token = SecondFactor::generateToken();
    QVERIFY(runOp([&](auto cb) { svc.setSecondFactor(SecondFactorType::Token, token, cb); }).ok());
    QVERIFY(svc.usesSecondFactor());
    QCOMPARE(svc.secondFactorType(), SecondFactorType::Token);
    QCOMPARE(svc.database()->secondFactor, token);

    // second factor set -> load requires it
    DatabaseFile db;
    QCOMPARE(decode(src->bytes(), kPassword), LoadResult::NeedsSecondFactor);
    QCOMPARE(decode(src->bytes(), kPassword, token, nullptr, &db), LoadResult::Success);
    QCOMPARE(db.secondFactorType, 1);
    svc.close();
    QCOMPARE(openAndWait(svc, src), LoadResult::NeedsSecondFactor);
    QCOMPARE(openAndWait(svc, src, kPassword, token), LoadResult::Success);

    QVERIFY(runOp([&](auto cb) { svc.removeSecondFactor(cb); }).ok());
    QVERIFY(!svc.usesSecondFactor());
    QVERIFY(svc.database()->secondFactor.isEmpty());
    QCOMPARE(decode(src->bytes(), kPassword, {}, nullptr, &db), LoadResult::Success);
    QVERIFY(!db.useSecondFactor);
    QCOMPARE(db.secondFactorType, 0);
    svc.close();
    QCOMPARE(openAndWait(svc, src), LoadResult::Success);
}

void DatabaseOperationsTest::testSecondFactorUnsupportedAndInvalid() {
    auto src = makeSource();
    DatabaseService svc(kFastKdf);
    QCOMPARE(openAndWait(svc, src), LoadResult::Success);
    QSignalSpy settingsChanged(&svc, &DatabaseService::databaseSettingsChanged);

    QCOMPARE(runOp([&](auto cb) { svc.setSecondFactor(SecondFactorType::Authenticator, "123", cb); }).status,
             DatabaseOpStatus::UnsupportedSecondFactorType);
    QCOMPARE(runOp([&](auto cb) { svc.setSecondFactor(SecondFactorType::Token, SecretString(), cb); }).status,
             DatabaseOpStatus::InvalidSecondFactor);
    QCOMPARE(runOp([&](auto cb) { svc.setSecondFactor(SecondFactorType::OTP, SecretString(), cb); }).status,
             DatabaseOpStatus::InvalidSecondFactor);
    QCOMPARE(settingsChanged.count(), 0);
    QCOMPARE(src->saveCount(), 0);
    QVERIFY(!svc.usesSecondFactor());
}

void DatabaseOperationsTest::testRotateOneTimeSecondFactor() {
    auto src = makeSource({makeItem("A")});
    DatabaseService svc(kFastKdf);
    QCOMPARE(openAndWait(svc, src), LoadResult::Success);

    std::optional<std::pair<DatabaseOpResult, SecretString>> rotated;
    auto rotate = [&]() {
        rotated.reset();
        svc.rotateOneTimeSecondFactor([&](const DatabaseOpResult &r, const SecretString &token) {
            rotated = std::make_pair(r, token);
        });
        QVERIFY(!rotated);
        QTRY_VERIFY_WITH_TIMEOUT(rotated.has_value(), 30000);
    };

    // Not an OTP database
    rotate();
    QCOMPARE(rotated->first.status, DatabaseOpStatus::InvalidSecondFactor);
    QVERIFY(rotated->second.isEmpty());

    const SecretString first = "111222333444";
    QVERIFY(runOp([&](auto cb) { svc.setSecondFactor(SecondFactorType::OTP, first, cb); }).ok());
    QCOMPARE(svc.secondFactorType(), SecondFactorType::OTP);

    rotate();
    QVERIFY(rotated->first.ok());
    const SecretString second = rotated->second;
    QCOMPARE(second.size(), size_t(SecondFactor::DefaultTokenLength));
    QVERIFY(second != first);
    QCOMPARE(decode(src->bytes(), kPassword, second), LoadResult::Success);
    QCOMPARE(decode(src->bytes(), kPassword, first), LoadResult::WrongPassword);

    // Failed save: no token handed out, the previous token stays valid (in memory and on disk)
    src->setFailSaves(true);
    rotate();
    QCOMPARE(rotated->first.status, DatabaseOpStatus::IoError);
    QVERIFY(rotated->second.isEmpty());
    QCOMPARE(svc.database()->secondFactor, second);
    src->setFailSaves(false);
    QVERIFY(runOp([&](auto cb) { svc.forceSave(cb); }).ok());
    QCOMPARE(decode(src->bytes(), kPassword, second), LoadResult::Success);
}

void DatabaseOperationsTest::testImportPasswordsMergeRules() {
    auto src = makeSource({makeItem("A"), makeItem("B")});
    DatabaseService svc(kFastKdf);
    svc.setSaveDelayMs(60000);
    QCOMPARE(openAndWait(svc, src), LoadResult::Success);
    QSignalSpy itemsChanged(&svc, &DatabaseService::itemsChanged);

    // "Add": appended, duplicates are kept (C#: DatabaseItem.AddRange)
    QVERIFY(svc.importItems({makeItem("B"), makeItem("C")}, DatabaseService::ImportMode::Append));
    QCOMPARE(itemNames(svc.items()), QStringList({"A", "B", "B", "C"}));
    QVERIFY(svc.isDirty());
    QCOMPARE(itemsChanged.count(), 1);

    QVERIFY(svc.importItems({}, DatabaseService::ImportMode::Append));
    QCOMPARE(svc.itemCount(), 4);
    QCOMPARE(itemsChanged.count(), 1);

    // "Override": existing items are replaced
    QVERIFY(svc.importItems({makeItem("X")}, DatabaseService::ImportMode::Override));
    QCOMPARE(itemNames(svc.items()), QStringList({"X"}));

    svc.close();
    svc.waitForBackgroundWork();
    QStringList names;
    QCOMPARE(decode(src->bytes(), kPassword, {}, &names), LoadResult::Success);
    QCOMPARE(names, QStringList({"X"}));

    // Read-only databases reject imports
    src->setReadOnly(true);
    QCOMPARE(openAndWait(svc, src), LoadResult::Success);
    QVERIFY(!svc.importItems({makeItem("Y")}));
    QCOMPARE(itemNames(svc.items()), QStringList({"X"}));
}

void DatabaseOperationsTest::testImportPasswordsFromFile() {
    const QString main = writeDb("main.epdb", {makeItem("A")});
    const QString other = writeDb("other.epdb", {makeItem("X"), makeItem("Y")}, "other password");
    DatabaseService svc(kFastKdf);
    DatabaseOperations ops(makeSettings({main}), &svc, kFastKdf);

    QCOMPARE(runOp([&](auto cb) {
                 ops.importPasswordsFromFile(other, "other password", {}, DatabaseService::ImportMode::Append, cb);
             }).status,
             DatabaseOpStatus::NoDatabaseOpen);

    QCOMPARE(openLocal(svc, main), LoadResult::Success);
    QCOMPARE(runOp([&](auto cb) {
                 ops.importPasswordsFromFile(other, kPassword, {}, DatabaseService::ImportMode::Append, cb);
             }).status,
             DatabaseOpStatus::WrongPassword);
    QCOMPARE(itemNames(svc.items()), QStringList({"A"}));

    QVERIFY(runOp([&](auto cb) {
                ops.importPasswordsFromFile(other, "other password", {}, DatabaseService::ImportMode::Append, cb);
            }).ok());
    QCOMPARE(itemNames(svc.items()), QStringList({"A", "X", "Y"}));
    svc.close();
    svc.waitForBackgroundWork();
    QStringList names;
    QCOMPARE(decode(readFile(main), kPassword, {}, &names), LoadResult::Success);
    QCOMPARE(names, QStringList({"A", "X", "Y"}));
}

void DatabaseOperationsTest::testExportDatabaseCopy() {
    const QString main = writeDb("main.epdb", {makeItem("A"), makeItem("B")});
    const QByteArray mainBytes = readFile(main);
    DatabaseService svc(kFastKdf);
    auto settings = makeSettings({main});
    DatabaseOperations ops(settings, &svc, kFastKdf);
    QCOMPARE(openLocal(svc, main), LoadResult::Success);

    const QString target = tmpPath("copy.epdb");
    const DatabaseOpResult res =
        runOp([&](auto cb) { ops.exportDatabase(target, DatabaseService::ExportOptions{}, cb); });
    QVERIFY2(res.ok(), qPrintable(res.message()));
    QCOMPARE(res.path, target);

    QStringList names;
    QCOMPARE(decode(readFile(target), kPassword, {}, &names), LoadResult::Success);
    QCOMPARE(names, QStringList({"A", "B"}));
    // The open database and the known list are untouched
    QCOMPARE(readFile(main), mainBytes);
    QCOMPARE(svc.itemCount(), 2);
    QVERIFY(!svc.isDirty());
    QCOMPARE(settings->databasePaths(), QStringList({main}));
}

void DatabaseOperationsTest::testExportWithNewPasswordOpensWithNewPasswordOnly() {
    const SecretString token = "123456789012";
    const QString main = writeDb("main.epdb", {makeItem("A")}, kPassword, token);
    DatabaseService svc(kFastKdf);
    DatabaseOperations ops(makeSettings({main}), &svc, kFastKdf);
    QCOMPARE(openLocal(svc, main, kPassword, token), LoadResult::Success);

    DatabaseService::ExportOptions options;
    options.newPassword = SecretString("export password");
    const QString target = tmpPath("export.epdb");
    QVERIFY(runOp([&](auto cb) { ops.exportDatabase(target, options, cb); }).ok());

    const QByteArray bytes = readFile(target);
    QStringList names;
    QCOMPARE(decode(bytes, "export password", token, &names), LoadResult::Success);
    QCOMPARE(names, QStringList({"A"}));
    QCOMPARE(decode(bytes, kPassword, token), LoadResult::WrongPassword);
    // The second factor is kept (C#: export.SecondFactor = db.SecondFactor)
    QCOMPARE(decode(bytes, "export password"), LoadResult::NeedsSecondFactor);
    // The open database keeps its password
    QCOMPARE(decode(readFile(main), kPassword, token), LoadResult::Success);

    options.newPassword = SecretString("abc");
    QCOMPARE(runOp([&](auto cb) { ops.exportDatabase(tmpPath("short.epdb"), options, cb); }).status,
             DatabaseOpStatus::PasswordTooShort);
    QVERIFY(!QFile::exists(tmpPath("short.epdb")));
}

void DatabaseOperationsTest::testExportSelectedItemsAndBackup() {
    const QString main = writeDb("main.epdb", {makeItem("A"), makeItem("B"), makeItem("C")});
    DatabaseService svc(kFastKdf);
    DatabaseOperations ops(makeSettings({main}), &svc, kFastKdf);
    QCOMPARE(openLocal(svc, main), LoadResult::Success);

    DatabaseService::ExportOptions options;
    options.items = QList<PasswordItem>{svc.items().at(0), svc.items().at(2)};
    QVERIFY(runOp([&](auto cb) { ops.exportDatabase(tmpPath("selected.epdb"), options, cb); }).ok());
    QStringList names;
    QCOMPARE(decode(readFile(tmpPath("selected.epdb")), kPassword, {}, &names), LoadResult::Success);
    QCOMPARE(names, QStringList({"A", "C"}));

    // Backup = all items, current password; unsaved edits are included
    QVERIFY(svc.addItem(makeItem("D")));
    const QString backup =
        tmpPath(DatabaseOperations::defaultBackupFileName(svc.displayName(), QDateTime::currentDateTime()));
    QVERIFY(runOp([&](auto cb) { ops.backupDatabase(backup, cb); }).ok());
    QCOMPARE(decode(readFile(backup), kPassword, {}, &names), LoadResult::Success);
    QCOMPARE(names, QStringList({"A", "B", "C", "D"}));
    QVERIFY(backup.contains("main_backup_"));

    // Exporting works for read-only (temporary) databases, too
    svc.close();
    svc.waitForBackgroundWork();
    QSignalSpy finished(&svc, &DatabaseService::openFinished);
    QVERIFY(ops.openTemporaryDatabase(main, kPassword));
    QVERIFY(finished.wait(30000));
    QVERIFY(svc.isReadOnly());
    QVERIFY(runOp([&](auto cb) { ops.backupDatabase(tmpPath("fromTemp.epdb"), cb); }).ok());
    QCOMPARE(decode(readFile(tmpPath("fromTemp.epdb")), kPassword, {}, &names), LoadResult::Success);
    QCOMPARE(names, QStringList({"A", "B", "C", "D"}));
}

void DatabaseOperationsTest::testExportRejectsOpenDatabaseFile() {
    const QString main = writeDb("main.epdb", {makeItem("A")});
    const QByteArray mainBytes = readFile(main);
    DatabaseService svc(kFastKdf);
    DatabaseOperations ops(makeSettings({main}), &svc, kFastKdf);
    QCOMPARE(openLocal(svc, main), LoadResult::Success);

    DatabaseService::ExportOptions options;
    options.newPassword = SecretString("other password");
    QCOMPARE(runOp([&](auto cb) { ops.exportDatabase(main, options, cb); }).status,
             DatabaseOpStatus::SameAsOpenDatabase);
    QCOMPARE(runOp([&](auto cb) { ops.exportDatabase(tmpPath("x/../main.epdb"), options, cb); }).status,
             DatabaseOpStatus::SameAsOpenDatabase);
    QCOMPARE(runOp([&](auto cb) { ops.exportDatabase(QString(), options, cb); }).status, DatabaseOpStatus::InvalidPath);
    QCOMPARE(readFile(main), mainBytes);

    // Service level
    QCOMPARE(runOp([&](auto cb) { svc.exportDatabase(svc.sharedSource(), {}, cb); }).status,
             DatabaseOpStatus::SameAsOpenDatabase);
    QCOMPARE(runOp([&](auto cb) { svc.exportDatabase(nullptr, {}, cb); }).status, DatabaseOpStatus::InvalidPath);
    auto readOnlyTarget = std::make_shared<FakeDatabaseSource>();
    readOnlyTarget->setReadOnly(true);
    QCOMPARE(runOp([&](auto cb) { svc.exportDatabase(readOnlyTarget, {}, cb); }).status, DatabaseOpStatus::ReadOnly);
    auto failingTarget = std::make_shared<FakeDatabaseSource>();
    failingTarget->setFailSaves(true);
    QCOMPARE(runOp([&](auto cb) { svc.exportDatabase(failingTarget, {}, cb); }).status, DatabaseOpStatus::IoError);
}

void DatabaseOperationsTest::testForceSave() {
    auto src = makeSource({makeItem("A")});
    DatabaseService svc(kFastKdf);
    svc.setSaveDelayMs(100);
    QCOMPARE(openAndWait(svc, src), LoadResult::Success);

    // Saves even without changes (C#: ForceSaveAsync)
    QVERIFY(runOp([&](auto cb) { svc.forceSave(cb); }).ok());
    QCOMPARE(src->saveCount(), 1);

    // Cancels the pending deferred save
    QVERIFY(svc.addItem(makeItem("B")));
    QVERIFY(runOp([&](auto cb) { svc.forceSave(cb); }).ok());
    QCOMPARE(src->saveCount(), 2);
    QVERIFY(!svc.isDirty());
    QTest::qWait(300);
    QCOMPARE(src->saveCount(), 2);

    src->setFailSaves(true);
    QCOMPARE(runOp([&](auto cb) { svc.forceSave(cb); }).status, DatabaseOpStatus::IoError);
}

void DatabaseOperationsTest::testTemporaryDatabaseIsReadOnly() {
    const QString path = writeDb("temp.epdb", {makeItem("A")});
    const QByteArray bytes = readFile(path);
    auto settings = makeSettings();
    DatabaseService svc(kFastKdf);
    DatabaseOperations ops(settings, &svc, kFastKdf);

    QSignalSpy finished(&svc, &DatabaseService::openFinished);
    QVERIFY(ops.openTemporaryDatabase(path, kPassword));
    QVERIFY(finished.wait(30000));
    QCOMPARE(finished.at(0).at(0).value<LoadResult>(), LoadResult::Success);

    QVERIFY(svc.isTemporary());
    QVERIFY(svc.isReadOnly());
    QCOMPARE(svc.displayName(), QString("temp (Temp) (ReadOnly)"));
    QCOMPARE(svc.itemCount(), 1);
    QVERIFY(!svc.addItem(makeItem("B")));
    QVERIFY(!svc.importItems({makeItem("C")}));
    QCOMPARE(runOp([&](auto cb) { svc.changeMasterPassword(kPassword, "abcdef", "abcdef", cb); }).status,
             DatabaseOpStatus::ReadOnly);
    QCOMPARE(runOp([&](auto cb) { svc.setSecondFactor(SecondFactorType::Token, "123456789012", cb); }).status,
             DatabaseOpStatus::ReadOnly);
    QCOMPARE(runOp([&](auto cb) { svc.forceSave(cb); }).status, DatabaseOpStatus::ReadOnly);
    // Also a delete through the operations is blocked while it is loaded
    settings->setDatabasePaths({path, writeDb("other.epdb", {})});
    QCOMPARE(ops.deleteDatabase(path).status, DatabaseOpStatus::IsLoadedDatabase);

    svc.close();
    svc.waitForBackgroundWork();
    QVERIFY(!svc.isTemporary());
    QCOMPARE(readFile(path), bytes);

    // A normal open of the same file is writable and not temporary
    QCOMPARE(openLocal(svc, path), LoadResult::Success);
    QVERIFY(!svc.isTemporary());
    QVERIFY(!svc.isReadOnly());
    QCOMPARE(svc.displayName(), QString("temp"));

    // Wrong password: open fails, nothing opened
    svc.close();
    finished.clear();
    QVERIFY(ops.openTemporaryDatabase(path, "wrong"));
    QVERIFY(finished.wait(30000));
    QCOMPARE(finished.at(0).at(0).value<LoadResult>(), LoadResult::WrongPassword);
    QVERIFY(!svc.isOpen());
    QVERIFY(!svc.isTemporary());
}

void DatabaseOperationsTest::testOperationsWithoutOpenDatabase() {
    DatabaseService svc(kFastKdf);
    QCOMPARE(runOp([&](auto cb) { svc.changeMasterPassword(kPassword, "abcdef", "abcdef", cb); }).status,
             DatabaseOpStatus::NoDatabaseOpen);
    QCOMPARE(runOp([&](auto cb) { svc.setSecondFactor(SecondFactorType::Token, "123456789012", cb); }).status,
             DatabaseOpStatus::NoDatabaseOpen);
    QCOMPARE(runOp([&](auto cb) { svc.removeSecondFactor(cb); }).status, DatabaseOpStatus::NoDatabaseOpen);
    QCOMPARE(runOp([&](auto cb) { svc.forceSave(cb); }).status, DatabaseOpStatus::NoDatabaseOpen);
    QCOMPARE(runOp([&](auto cb) { svc.exportDatabase(std::make_shared<FakeDatabaseSource>(), {}, cb); }).status,
             DatabaseOpStatus::NoDatabaseOpen);
    QVERIFY(!svc.importItems({makeItem("A")}));
    QVERIFY(svc.displayName().isEmpty());
    QVERIFY(!svc.isTemporary());

    std::optional<DatabaseOpResult> rotated;
    svc.rotateOneTimeSecondFactor([&](const DatabaseOpResult &r, const SecretString &) { rotated = r; });
    QTRY_VERIFY(rotated.has_value());
    QCOMPARE(rotated->status, DatabaseOpStatus::NoDatabaseOpen);

    DatabaseOperations ops(makeSettings(), nullptr, kFastKdf);
    QVERIFY(!ops.openTemporaryDatabase(tmpPath("x.epdb"), kPassword));
    QCOMPARE(runOp([&](auto cb) { ops.exportDatabase(tmpPath("x.epdb"), {}, cb); }).status,
             DatabaseOpStatus::NoDatabaseOpen);
    QCOMPARE(runOp([&](auto cb) { ops.backupDatabase(tmpPath("x.epdb"), cb); }).status,
             DatabaseOpStatus::NoDatabaseOpen);

    // Callbacks are dropped when the operations object is destroyed before they are delivered
    bool called = false;
    {
        DatabaseOperations shortLived(makeSettings(), nullptr, kFastKdf);
        shortLived.importDatabaseFile(tmpPath("missing.epdb"), kPassword, {},
                                      [&called](const DatabaseOpResult &) { called = true; });
    }
    QTest::qWait(50);
    QVERIFY(!called);
}
