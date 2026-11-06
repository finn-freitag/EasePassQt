#include "DatabaseFileTest.h"
#include "DatabaseFile.h"
#include "CryptoHelper.h"
#include "SecureBuffer.h"
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

using namespace EasePass::Core;

void DatabaseFileTest::testLoadCSharpTestDbFixture() {
    QString testDbPath = QStringLiteral(EASEPASS_TEST_FIXTURES_DIR "/test_db.epdb");
    QVERIFY(QFile::exists(testDbPath));

    DatabaseFile db;
    LoadResult res = DatabaseFile::loadFromFile(testDbPath, "SecretPassword123!", db);
    QCOMPARE(res, LoadResult::Success);

    QCOMPARE(db.version, 1.4);
    QCOMPARE(db.useSecondFactor, false);
    QCOMPARE(db.items.size(), 1);

    const auto &item = db.items.first();
    QCOMPARE(item.displayName, QString("Google Account"));
    QCOMPARE(item.username, QString("john_doe"));
    QCOMPARE(item.password, QString("myP@ssw0rd!"));
    QCOMPARE(item.email, QString("john@example.com"));
    QCOMPARE(item.website, QString("https://accounts.google.com"));
    QCOMPARE(item.notes, QString("Some notes"));
    QCOMPARE(item.secret, QString("JBSWY3DPEHPK3PXP"));
    QCOMPARE(item.digits, QString("6"));
    QCOMPARE(item.interval, QString("30"));
    QCOMPARE(item.algorithm, QString("SHA1"));
    QCOMPARE(item.tags, QStringList({"tech", "mail"}));
}

void DatabaseFileTest::testLoadCSharpOfficialTestEpdbFixture() {
    QString fixturePath = QStringLiteral(EASEPASS_TEST_FIXTURES_DIR "/test.epdb");
    QVERIFY(QFile::exists(fixturePath));

    DatabaseFile db;
    LoadResult res = DatabaseFile::loadFromFile(fixturePath, "123456", db);
    QCOMPARE(res, LoadResult::Success);

    QCOMPARE(db.version, 1.4);
    QCOMPARE(db.useSecondFactor, false);
    QCOMPARE(db.items.size(), 4);

    // Item 0
    QCOMPARE(db.items[0].displayName, QString("Test123"));
    QCOMPARE(db.items[0].username, QString("Julius"));
    QCOMPARE(db.items[0].password, QString("sooSicherOder?"));
    QCOMPARE(db.items[0].email, QString("mail@ph.de"));
    QCOMPARE(db.items[0].notes, QString("Baum.dummy"));
    QCOMPARE(db.items[0].tags, QStringList({"123124"}));

    // Item 1
    QCOMPARE(db.items[1].displayName, QString("Nudel"));
    QCOMPARE(db.items[1].username, QString("Kuchen"));
    QCOMPARE(db.items[1].password, QString("123123"));
    QCOMPARE(db.items[1].email, QString("Keks@mail.com"));
    QCOMPARE(db.items[1].website, QString("http://baum.dummy.com"));
    QCOMPARE(db.items[1].notes, QString("NOTITTEN"));

    // Item 2
    QCOMPARE(db.items[2].displayName, QString("Peters Futterecken"));
    QCOMPARE(db.items[2].username, QString("Peter"));
    QCOMPARE(db.items[2].password, QString("umdieeckefuttern"));
    QCOMPARE(db.items[2].email, QString("futter@ecke.uk"));
    QCOMPARE(db.items[2].website, QString("http://nee.com"));
    QCOMPARE(db.items[2].secret, QString("BBJVUWUYKNKUOBIOYMPN"));
    QCOMPARE(db.items[2].clicks, QStringList({"14.10.2026"}));

    // Item 3 (empty item)
    QCOMPARE(db.items[3].displayName, QString(""));
    QCOMPARE(db.items[3].password, QString(""));
}

void DatabaseFileTest::testLoadWrongPassword() {
    QString testDbPath = QStringLiteral(EASEPASS_TEST_FIXTURES_DIR "/test_db.epdb");
    QVERIFY(QFile::exists(testDbPath));

    DatabaseFile db;
    LoadResult res = DatabaseFile::loadFromFile(testDbPath, "CompletelyWrongPassword!", db);
    QCOMPARE(res, LoadResult::WrongPassword);
}

void DatabaseFileTest::testSaveAndLoadRoundtrip() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString filePath = tempDir.filePath("roundtrip.epdb");

    DatabaseFile db;
    db.filePath = filePath;
    db.masterPassword = "RoundtripPassword789!";
    db.version = 1.4;
    db.useSecondFactor = false;

    PasswordItem item1;
    item1.displayName = "Service Alpha";
    item1.username = "alpha_user";
    item1.password = "P@ss1";
    item1.tags = {"TagA", "TagB"};
    db.addItem(item1);

    PasswordItem item2;
    item2.displayName = "Service Beta";
    item2.username = "beta_user";
    item2.password = "P@ss2";
    item2.secret = "JBSWY3DPEHPK3PXP";
    db.addItem(item2);

    QVERIFY(db.saveToFile(filePath));
    QVERIFY(QFile::exists(filePath));

    DatabaseFile loaded;
    LoadResult res = DatabaseFile::loadFromFile(filePath, "RoundtripPassword789!", loaded);
    QCOMPARE(res, LoadResult::Success);
    QCOMPARE(loaded.items.size(), 2);
    QCOMPARE(loaded.items[0].displayName, QString("Service Alpha"));
    QCOMPARE(loaded.items[0].username, QString("alpha_user"));
    QCOMPARE(loaded.items[0].tags, QStringList({"TagA", "TagB"}));
    QCOMPARE(loaded.items[1].displayName, QString("Service Beta"));
    QCOMPARE(loaded.items[1].secret, QString("JBSWY3DPEHPK3PXP"));
}

void DatabaseFileTest::testSecondFactorHandling() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString filePath = tempDir.filePath("2fa_test.epdb");

    QString masterPw = "SecretPassword123!";
    QString secondFactor = "MyOtpToken123";

    // Build database with 2FA enabled
    QJsonArray itemsArr;
    PasswordItem it;
    it.displayName = "Secure Service";
    it.password = "SecretPass!";
    itemsArr.append(it.toJson());
    QByteArray innerPlain = QJsonDocument(itemsArr).toJson();

    QByteArray innerKey = CryptoHelper::deriveInnerKey(masterPw, secondFactor, false);
    QByteArray innerCipher = CryptoHelper::encryptAes(innerPlain, innerKey);

    QJsonObject settings;
    settings["UseSecondFactor"] = true;
    settings["SecondFactorType"] = 1;

    QJsonObject outerObj;
    outerObj["DatabaseFileType"] = 0;
    outerObj["Version"] = 1.4;
    outerObj["Settings"] = settings;
    outerObj["Data"] = QString::fromUtf8(innerCipher.toBase64());

    QByteArray outerPlain = QJsonDocument(outerObj).toJson(QJsonDocument::Compact);
    QByteArray outerKey = CryptoHelper::deriveOuterKey(masterPw);
    QByteArray outerCipher = CryptoHelper::encryptAes(outerPlain, outerKey);
    QByteArray finalBytes = CryptoHelper::addVersionTag(outerCipher, 3);

    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(finalBytes);
    file.close();

    // 1. Without second factor -> NeedsSecondFactor
    DatabaseFile db1;
    LoadResult res1 = DatabaseFile::loadFromFile(filePath, masterPw, db1, "");
    QCOMPARE(res1, LoadResult::NeedsSecondFactor);
    QCOMPARE(db1.useSecondFactor, true);

    // 2. With wrong second factor -> WrongPassword
    DatabaseFile db2;
    LoadResult res2 = DatabaseFile::loadFromFile(filePath, masterPw, db2, "Wrong2FAToken");
    QCOMPARE(res2, LoadResult::WrongPassword);

    // 3. With correct second factor -> Success
    DatabaseFile db3;
    LoadResult res3 = DatabaseFile::loadFromFile(filePath, masterPw, db3, secondFactor);
    QCOMPARE(res3, LoadResult::Success);
    QCOMPARE(db3.items.size(), 1);
    QCOMPARE(db3.items[0].displayName, QString("Secure Service"));
}

void DatabaseFileTest::testLoadDatabaseNotFound() {
    DatabaseFile db;
    LoadResult res = DatabaseFile::loadFromFile("/nonexistent/path/to/database.epdb", "pw", db);
    QCOMPARE(res, LoadResult::DatabaseNotFound);
}

void DatabaseFileTest::testLoadWrongFormatNotEpdb() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString filePath = tempDir.filePath("corrupted.epdb");

    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("Random corrupted data without any epdbversion tag");
    file.close();

    DatabaseFile db;
    LoadResult res = DatabaseFile::loadFromFile(filePath, "pw", db);
    QCOMPARE(res, LoadResult::WrongFormat);
}

void DatabaseFileTest::testLoadWrongFormatBadVersionTag() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString filePath = tempDir.filePath("bad_version_tag.epdb");

    QByteArray data = CryptoHelper::addVersionTag("SomeCipherBytes", 99); // version 99 != 3
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(data);
    file.close();

    DatabaseFile db;
    LoadResult res = DatabaseFile::loadFromFile(filePath, "pw", db);
    QCOMPARE(res, LoadResult::WrongFormat);
}

void DatabaseFileTest::testLoadWrongFormatInvalidOuterJson() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString filePath = tempDir.filePath("invalid_outer_json.epdb");

    // Outer cipher decrypts successfully, but plaintext is not valid JSON
    QByteArray key = CryptoHelper::deriveOuterKey("SecretPassword123!");
    QByteArray invalidJson = "This is definitely not valid JSON! { [ ";
    QByteArray cipher = CryptoHelper::encryptAes(invalidJson, key);
    QByteArray fileData = CryptoHelper::addVersionTag(cipher, 3);

    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(fileData);
    file.close();

    DatabaseFile db;
    LoadResult res = DatabaseFile::loadFromFile(filePath, "SecretPassword123!", db);
    QCOMPARE(res, LoadResult::WrongFormat);
}

void DatabaseFileTest::testLoadWrongFormatInvalidInnerJson() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString filePath = tempDir.filePath("invalid_inner_json.epdb");

    QString masterPw = "SecretPassword123!";
    QByteArray innerKey = CryptoHelper::deriveInnerKey(masterPw);
    // Inner plaintext is a JSON object instead of a JSON array
    QByteArray notArrayJson = "{\"Not\": \"An Array\"}";
    QByteArray innerCipher = CryptoHelper::encryptAes(notArrayJson, innerKey);

    QJsonObject outerObj;
    outerObj["DatabaseFileType"] = 0;
    outerObj["Version"] = 1.4;
    outerObj["Data"] = QString::fromUtf8(innerCipher.toBase64());

    QByteArray outerKey = CryptoHelper::deriveOuterKey(masterPw);
    QByteArray outerCipher = CryptoHelper::encryptAes(QJsonDocument(outerObj).toJson(), outerKey);
    QByteArray fileData = CryptoHelper::addVersionTag(outerCipher, 3);

    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(fileData);
    file.close();

    DatabaseFile db;
    LoadResult res = DatabaseFile::loadFromFile(filePath, masterPw, db);
    QCOMPARE(res, LoadResult::WrongFormat);
}

void DatabaseFileTest::testSaveReadOnlyFails() {
    DatabaseFile db;
    db.isReadOnly = true;
    db.filePath = "/tmp/test_readonly.epdb";
    QVERIFY(!db.saveToFile());
}

void DatabaseFileTest::testSaveEmptyPathFails() {
    DatabaseFile db;
    db.filePath = "";
    QVERIFY(!db.saveToFile(""));
}

void DatabaseFileTest::testItemAddUpdateDeleteBounds() {
    DatabaseFile db;
    QCOMPARE(db.items.size(), 0);

    PasswordItem it1;
    it1.displayName = "Item 1";
    db.addItem(it1);
    QCOMPARE(db.items.size(), 1);

    PasswordItem it2;
    it2.displayName = "Item 2";
    db.addItem(it2);
    QCOMPARE(db.items.size(), 2);

    // Update out-of-bounds
    PasswordItem badItem;
    badItem.displayName = "Bad";
    db.updateItem(-1, badItem);
    db.updateItem(5, badItem);
    QCOMPARE(db.items[0].displayName, QString("Item 1"));
    QCOMPARE(db.items[1].displayName, QString("Item 2"));

    // Update in-bounds
    PasswordItem updated;
    updated.displayName = "Item 1 Updated";
    db.updateItem(0, updated);
    QCOMPARE(db.items[0].displayName, QString("Item 1 Updated"));

    // Delete out-of-bounds
    db.deleteItem(-1);
    db.deleteItem(10);
    QCOMPARE(db.items.size(), 2);

    // Delete in-bounds
    db.deleteItem(0);
    QCOMPARE(db.items.size(), 1);
    QCOMPARE(db.items[0].displayName, QString("Item 2"));

    db.deleteItem(0);
    QCOMPARE(db.items.size(), 0);
}

void DatabaseFileTest::testCountOccurrences() {
    DatabaseFile db;
    PasswordItem it1;
    it1.password = "SharedPassword123";
    db.addItem(it1);
    PasswordItem it2;
    it2.password = "UniquePassword";
    db.addItem(it2);
    PasswordItem it3;
    it3.password = "SharedPassword123";
    db.addItem(it3);

    QCOMPARE(db.countOccurrences("SharedPassword123"), 2);
    QCOMPARE(db.countOccurrences("UniquePassword"), 1);
    QCOMPARE(db.countOccurrences("NonExistent"), 0);
}

// --- Wrong keys that pass the PKCS7 padding check (T24) --------------------------------
// AES-CBC with a wrong key yields valid padding about once in 256 tries. The load used to
// report WrongFormat then (and skip the old associated-data key), which made
// test_core.testImportPasswordsFromFile flaky. The tests search for such a key, so they are
// deterministic whatever the random IV is.
namespace {
const KdfParams kPaddingKdf{1, 64, 1};

QByteArray buildDb(const SecretString &password, const SecretString &secondFactor, bool oldInnerAd,
                   const QByteArray &innerJson, QByteArray *innerCipherOut = nullptr) {
    const QByteArray innerCipher = innerCipherOut && !innerCipherOut->isEmpty()
                                       ? *innerCipherOut
                                       : CryptoHelper::encryptAes(innerJson, CryptoHelper::deriveInnerKey(
                                                                                 password, secondFactor, oldInnerAd, kPaddingKdf));
    if (innerCipherOut)
        *innerCipherOut = innerCipher;
    QJsonObject outer;
    outer["DatabaseFileType"] = 0;
    outer["Version"] = 1.4;
    outer["Settings"] = QJsonObject{{"UseSecondFactor", !secondFactor.isEmpty()}, {"SecondFactorType", 0}};
    outer["Data"] = QString::fromLatin1(innerCipher.toBase64());
    const QByteArray outerCipher = CryptoHelper::encryptAes(QJsonDocument(outer).toJson(QJsonDocument::Compact),
                                                            CryptoHelper::deriveOuterKey(password, kPaddingKdf));
    return CryptoHelper::addVersionTag(outerCipher);
}

const QByteArray kItemsJson = R"([{"DisplayName":"A","Password":"pw","Username":"","Email":"","Website":"","Notes":"","Secret":"","Digits":"6","Interval":"30","Algorithm":"SHA1","Tags":[],"Clicks":[]}])";
} // namespace

void DatabaseFileTest::testWrongPasswordPassingPaddingCheck() {
    const QByteArray file = buildDb(SecretString("right password"), SecretString(), false, kItemsJson);
    int version = 0;
    QByteArray outerCipher;
    QVERIFY(CryptoHelper::extractVersionTag(file, version, outerCipher));
    QString wrong;
    for (int i = 0; i < 20000 && wrong.isEmpty(); ++i) {
        const QString candidate = QStringLiteral("wrong %1").arg(i);
        QByteArray plain;
        if (CryptoHelper::decryptAes(outerCipher, CryptoHelper::deriveOuterKey(SecretString(candidate), kPaddingKdf), plain))
            wrong = candidate;
    }
    QVERIFY2(!wrong.isEmpty(), "no wrong password with valid padding found");
    DatabaseFile db;
    QCOMPARE(DatabaseFile::loadFromBytes(file, SecretString(wrong), db, SecretString(), kPaddingKdf),
             LoadResult::WrongPassword);
    QCOMPARE(DatabaseFile::loadFromBytes(file, SecretString("right password"), db, SecretString(), kPaddingKdf),
             LoadResult::Success);
}

void DatabaseFileTest::testWrongSecondFactorPassingPaddingCheck() {
    const SecretString password("pw");
    QByteArray innerCipher;
    const QByteArray file = buildDb(password, SecretString("123456789012"), false, kItemsJson, &innerCipher);
    QString wrong;
    for (int i = 0; i < 20000 && wrong.isEmpty(); ++i) {
        const QString candidate = QString::number(100000000000LL + i);
        SecureBuffer plain;
        const bool newKey = CryptoHelper::decryptAes(
            innerCipher, CryptoHelper::deriveInnerKey(password, SecretString(candidate), false, kPaddingKdf), plain);
        const bool oldKey = CryptoHelper::decryptAes(
            innerCipher, CryptoHelper::deriveInnerKey(password, SecretString(candidate), true, kPaddingKdf), plain);
        if (newKey || oldKey)
            wrong = candidate;
    }
    QVERIFY2(!wrong.isEmpty(), "no wrong second factor with valid padding found");
    DatabaseFile db;
    QCOMPARE(DatabaseFile::loadFromBytes(file, password, db, SecretString(wrong), kPaddingKdf), LoadResult::WrongPassword);
    QCOMPARE(DatabaseFile::loadFromBytes(file, password, db, SecretString("123456789012"), kPaddingKdf),
             LoadResult::Success);
}

void DatabaseFileTest::testOldAssociatedDataWhenNewKeyPassesPadding() {
    // A file written with the old "Database_Version_1,4" associated data (German C# builds)
    // whose ciphertext also decrypts with valid padding under the current key.
    const SecretString password("pw");
    const SecureBuffer newKey = CryptoHelper::deriveInnerKey(password, SecretString(), false, kPaddingKdf);
    const SecureBuffer oldKey = CryptoHelper::deriveInnerKey(password, SecretString(), true, kPaddingKdf);
    QByteArray innerCipher;
    for (int i = 0; i < 20000; ++i) {
        const QByteArray candidate = CryptoHelper::encryptAes(kItemsJson, oldKey); // random IV each time
        SecureBuffer plain;
        if (CryptoHelper::decryptAes(candidate, newKey, plain)) {
            innerCipher = candidate;
            break;
        }
    }
    QVERIFY2(!innerCipher.isEmpty(), "no ciphertext with valid padding under the current key found");
    const QByteArray file = buildDb(password, SecretString(), true, kItemsJson, &innerCipher);
    DatabaseFile db;
    QCOMPARE(DatabaseFile::loadFromBytes(file, password, db, SecretString(), kPaddingKdf), LoadResult::Success);
    QCOMPARE(db.items.size(), 1);
    QCOMPARE(db.items.at(0).password, QStringLiteral("pw"));
}
