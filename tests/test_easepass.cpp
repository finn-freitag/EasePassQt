#include <QCoreApplication>
#include <QTest>
#include <QFile>
#include <QDir>
#include "CryptoHelper.h"
#include "DatabaseFile.h"
#include "TotpHelper.h"
#include "PasswordGenerator.h"
#include "QrCodeScanner.h"

using namespace EasePass::Core;

class TestEasePass : public QObject {
    Q_OBJECT

private slots:
    void testArgon2AndAes();
    void testDecryptCSharpDatabase();
    void testSaveAndLoadDatabaseRoundtrip();
    void testTotpGeneration();
    void testTotpUriParsing();
    void testPasswordGenerator();
};

void TestEasePass::testArgon2AndAes() {
    QString masterPw = "SecretPassword123!";
    QByteArray outerKey = CryptoHelper::deriveOuterKey(masterPw);
    QCOMPARE(outerKey.size(), 32);

    // Verify outerKey matches C# vector
    QByteArray expectedOuterKey = QByteArray::fromHex("D44D3A9FB911C7BFD65541BDE722CF82EADB8B75E6B8DA34D30B785D1A7F7FD9");
    QCOMPARE(outerKey, expectedOuterKey);

    // Inner key without second factor
    QByteArray innerKey = CryptoHelper::deriveInnerKey(masterPw);
    QCOMPARE(innerKey.size(), 32);
    QByteArray expectedInnerKey = QByteArray::fromHex("574237911AC1B8D592BE7E8C2775CCAFD3EFAF11EB7D19DA160374F7EA4240A0");
    QCOMPARE(innerKey, expectedInnerKey);

    // AES encryption and decryption roundtrip
    QByteArray plaintext = "Hello from EasePass Qt rewrite!";
    QByteArray cipher = CryptoHelper::encryptAes(plaintext, outerKey);
    QVERIFY(!cipher.isEmpty());
    QVERIFY(cipher.size() > 16);

    QByteArray decrypted;
    QVERIFY(CryptoHelper::decryptAes(cipher, outerKey, decrypted));
    QCOMPARE(decrypted, plaintext);

    // Wrong key fails decryption
    QByteArray wrongKey(32, 'X');
    QByteArray failedDecrypted;
    QVERIFY(!CryptoHelper::decryptAes(cipher, wrongKey, failedDecrypted));
}

void TestEasePass::testDecryptCSharpDatabase() {
    QString testDbPath = "/home/finn/.gemini/antigravity/brain/d1125c4e-31fc-4747-bf21-ccd82f682f5b/scratch/verify_crypto/test_db.epdb";
    QVERIFY(QFile::exists(testDbPath));

    DatabaseFile db;
    LoadResult res = DatabaseFile::loadFromFile(testDbPath, "SecretPassword123!", db);
    QCOMPARE(res, LoadResult::Success);

    QCOMPARE(db.version, 1.4);
    QCOMPARE(db.useSecondFactor, false);
    QCOMPARE(db.items.size(), 1);

    const auto& item = db.items.first();
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

    // Wrong password test
    DatabaseFile wrongDb;
    LoadResult wrongRes = DatabaseFile::loadFromFile(testDbPath, "WrongPassword!", wrongDb);
    QCOMPARE(wrongRes, LoadResult::WrongPassword);
}

void TestEasePass::testSaveAndLoadDatabaseRoundtrip() {
    QString tempPath = QDir::tempPath() + "/easepass_unit_test.epdb";
    QFile::remove(tempPath);

    DatabaseFile db;
    db.filePath = tempPath;
    db.masterPassword = "TestPassword456$";
    db.version = 1.4;
    db.useSecondFactor = false;

    PasswordItem item1;
    item1.displayName = "GitHub";
    item1.username = "finn_dev";
    item1.password = "SuperSecretToken#99";
    item1.website = "https://github.com";
    item1.tags = {"Development", "Git"};
    item1.secret = "HXDMVJECJJWSRB3HWIZR4IFUGFTMXBOZ";
    db.addItem(item1);

    PasswordItem item2;
    item2.displayName = "NixOS Forum";
    item2.username = "finn";
    item2.password = "NixOS_Linux_2026!";
    item2.website = "https://discourse.nixos.org";
    item2.tags = {"Linux"};
    db.addItem(item2);

    QVERIFY(db.saveToFile(tempPath));
    QVERIFY(QFile::exists(tempPath));

    // Load back
    DatabaseFile loadedDb;
    LoadResult res = DatabaseFile::loadFromFile(tempPath, "TestPassword456$", loadedDb);
    QCOMPARE(res, LoadResult::Success);
    QCOMPARE(loadedDb.items.size(), 2);

    QCOMPARE(loadedDb.items[0].displayName, QString("GitHub"));
    QCOMPARE(loadedDb.items[0].username, QString("finn_dev"));
    QCOMPARE(loadedDb.items[0].password, QString("SuperSecretToken#99"));
    QCOMPARE(loadedDb.items[0].secret, QString("HXDMVJECJJWSRB3HWIZR4IFUGFTMXBOZ"));
    QCOMPARE(loadedDb.items[0].tags, QStringList({"Development", "Git"}));

    QCOMPARE(loadedDb.items[1].displayName, QString("NixOS Forum"));
    QCOMPARE(loadedDb.items[1].password, QString("NixOS_Linux_2026!"));

    QFile::remove(tempPath);
}

void TestEasePass::testTotpGeneration() {
    // Secret "JBSWY3DPEHPK3PXP" is "Hello!\xde\xad\xbe\xef"
    QString secret = "JBSWY3DPEHPK3PXP";

    // Test at timestamp 59 (interval 30 -> counter 1)
    QString token1 = TotpHelper::generateToken(secret, 59, 6, 30, "SHA1");
    QCOMPARE(token1.length(), 6);

    // Test remaining seconds
    int rem = TotpHelper::getRemainingSeconds(30, 25);
    QCOMPARE(rem, 5); // 30 - (25 % 30) = 5
    QCOMPARE(TotpHelper::getRemainingSeconds(30, 0), 30);
}

void TestEasePass::testTotpUriParsing() {
    QString uri = "otpauth://totp/GitHub:user%40example.com?secret=JBSWY3DPEHPK3PXP&issuer=GitHub&algorithm=SHA256&digits=8&period=60";
    TotpParameters params;
    QVERIFY(TotpHelper::parseOtpauthUri(uri, params));

    QCOMPARE(params.secret, QString("JBSWY3DPEHPK3PXP"));
    QCOMPARE(params.issuer, QString("GitHub"));
    QCOMPARE(params.account, QString("user@example.com"));
    QCOMPARE(params.algorithm, QString("SHA256"));
    QCOMPARE(params.digits, 8);
    QCOMPARE(params.period, 60);

    // Roundtrip URI generation
    QString generated = TotpHelper::generateOtpauthUri(params);
    TotpParameters parsedBack;
    QVERIFY(TotpHelper::parseOtpauthUri(generated, parsedBack));
    QCOMPARE(parsedBack.secret, params.secret);
    QCOMPARE(parsedBack.digits, 8);
    QCOMPARE(parsedBack.period, 60);
}

void TestEasePass::testPasswordGenerator() {
    PasswordGeneratorOptions opts;
    opts.length = 24;
    opts.useUpper = true;
    opts.useLower = true;
    opts.useDigits = true;
    opts.useSymbols = true;

    QString pw = PasswordGenerator::generate(opts);
    QCOMPARE(pw.length(), 24);

    bool hasUpper = false, hasLower = false, hasDigit = false, hasSymbol = false;
    for (QChar c : pw) {
        if (c.isUpper()) hasUpper = true;
        else if (c.isLower()) hasLower = true;
        else if (c.isDigit()) hasDigit = true;
        else hasSymbol = true;
    }

    QVERIFY(hasUpper);
    QVERIFY(hasLower);
    QVERIFY(hasDigit);
    QVERIFY(hasSymbol);
}

QTEST_MAIN(TestEasePass)
#include "test_easepass.moc"
