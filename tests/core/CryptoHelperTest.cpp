#include "CryptoHelperTest.h"
#include "CryptoHelper.h"
#include <QTest>

using namespace EasePass::Core;

void CryptoHelperTest::testDeriveOuterKeyKnownVector() {
    QString masterPw = "SecretPassword123!";
    QByteArray outerKey = CryptoHelper::deriveOuterKey(masterPw);
    QCOMPARE(outerKey.size(), 32);

    QByteArray expectedOuterKey =
        QByteArray::fromHex("D44D3A9FB911C7BFD65541BDE722CF82EADB8B75E6B8DA34D30B785D1A7F7FD9");
    QCOMPARE(outerKey, expectedOuterKey);
}

void CryptoHelperTest::testDeriveInnerKeyKnownVector() {
    QString masterPw = "SecretPassword123!";
    QByteArray innerKey = CryptoHelper::deriveInnerKey(masterPw);
    QCOMPARE(innerKey.size(), 32);

    QByteArray expectedInnerKey =
        QByteArray::fromHex("574237911AC1B8D592BE7E8C2775CCAFD3EFAF11EB7D19DA160374F7EA4240A0");
    QCOMPARE(innerKey, expectedInnerKey);
}

void CryptoHelperTest::testAssociatedDataOldVsNew() {
    QString masterPw = "SecretPassword123!";
    QByteArray innerKeyNew = CryptoHelper::deriveInnerKey(masterPw, QString(), false);
    QByteArray innerKeyOld = CryptoHelper::deriveInnerKey(masterPw, QString(), true);

    QCOMPARE(innerKeyNew.size(), 32);
    QCOMPARE(innerKeyOld.size(), 32);
    QVERIFY(innerKeyNew != innerKeyOld);

    // Also with second factor
    QByteArray sfKeyNew = CryptoHelper::deriveInnerKey(masterPw, "SecondFactorToken", false);
    QByteArray sfKeyOld = CryptoHelper::deriveInnerKey(masterPw, "SecondFactorToken", true);
    QCOMPARE(sfKeyNew.size(), 32);
    QCOMPARE(sfKeyOld.size(), 32);
    QVERIFY(sfKeyNew != sfKeyOld);
    QVERIFY(sfKeyNew != innerKeyNew);
}

void CryptoHelperTest::testToUtf16LE() {
    // ASCII string
    QString ascii = "Hello";
    QByteArray asciiBytes = CryptoHelper::toUtf16LE(ascii);
    // 'H' = 0x0048 -> 48 00, 'e' = 0x0065 -> 65 00, ...
    QByteArray expectedAscii = QByteArray::fromHex("480065006c006c006f00");
    QCOMPARE(asciiBytes, expectedAscii);

    // Unicode string with German umlaut
    QString umlaut = "Über";
    QByteArray umlautBytes = CryptoHelper::toUtf16LE(umlaut);
    // 'Ü' = 0x00dc -> dc 00
    QCOMPARE(static_cast<uint8_t>(umlautBytes[0]), 0xdc);
    QCOMPARE(static_cast<uint8_t>(umlautBytes[1]), 0x00);

    // Empty string
    QCOMPARE(CryptoHelper::toUtf16LE("").size(), 0);
}

void CryptoHelperTest::testCustomToBase64() {
    // 3-byte and 6-byte inputs (aligned to 24 bits) produce standard base64 strings
    QByteArray input3 = "ABC";
    QByteArray out3 = CryptoHelper::customToBase64(input3);
    QCOMPARE(out3, QByteArray("QUJD"));

    QByteArray input6 = "ABCDEF";
    QByteArray out6 = CryptoHelper::customToBase64(input6);
    QCOMPARE(out6, QByteArray("QUJDREVG"));

    // Non-multiple-of-3 inputs reflect the legacy C# implementation behavior (see P12)
    QByteArray input1 = "A";
    QByteArray out1 = CryptoHelper::customToBase64(input1);
    QCOMPARE(out1, QByteArray("Q=\x00\x00", 4));

    QByteArray input2 = "AB";
    QByteArray out2 = CryptoHelper::customToBase64(input2);
    QCOMPARE(out2, QByteArray("QU=\x00", 4));
}

void CryptoHelperTest::testAesEncryptionDecryptionRoundtrip() {
    QByteArray key(32, 0);
    for (int i = 0; i < 32; ++i)
        key[i] = static_cast<char>(i * 7);

    QByteArray plaintext = "A quick brown fox jumps over the lazy dog. 1234567890!@#$%^&*()";
    QByteArray cipher = CryptoHelper::encryptAes(plaintext, key);

    QVERIFY(!cipher.isEmpty());
    QVERIFY(cipher.size() > 16); // IV (16) + at least 1 AES block (16)

    QByteArray decrypted;
    QVERIFY(CryptoHelper::decryptAes(cipher, key, decrypted));
    QCOMPARE(decrypted, plaintext);

    // Empty plaintext
    QByteArray emptyCipher = CryptoHelper::encryptAes(QByteArray(), key);
    QVERIFY(!emptyCipher.isEmpty());
    QByteArray emptyDecrypted;
    QVERIFY(CryptoHelper::decryptAes(emptyCipher, key, emptyDecrypted));
    QCOMPARE(emptyDecrypted, QByteArray());
}

void CryptoHelperTest::testAesWrongKeyFailsDecryption() {
    QByteArray correctKey(32, 'A');
    QByteArray wrongKey(32, 'B');

    QByteArray plaintext = "Secret data payload";
    QByteArray cipher = CryptoHelper::encryptAes(plaintext, correctKey);

    QByteArray decrypted;
    QVERIFY(!CryptoHelper::decryptAes(cipher, wrongKey, decrypted));
}

void CryptoHelperTest::testAesCorruptedCiphertextFails() {
    QByteArray key(32, 'K');
    QByteArray plaintext = "Confidential password manager record";
    QByteArray cipher = CryptoHelper::encryptAes(plaintext, key);

    // Corrupt one byte in the ciphertext body
    cipher[cipher.size() - 5] = ~cipher[cipher.size() - 5];

    QByteArray decrypted;
    QVERIFY(!CryptoHelper::decryptAes(cipher, key, decrypted));
}

void CryptoHelperTest::testAesTruncatedCiphertextFails() {
    QByteArray key(32, 'K');
    QByteArray shortData(15, 'x'); // smaller than 16-byte IV

    QByteArray decrypted;
    QVERIFY(!CryptoHelper::decryptAes(shortData, key, decrypted));

    // Invalid key length
    QByteArray badKey(16, 'K');
    QVERIFY(!CryptoHelper::decryptAes(shortData, badKey, decrypted));
}

void CryptoHelperTest::testVersionTagGood() {
    QByteArray payload = "SomeDatabasePayload";
    int version = 3;
    QByteArray tagged = CryptoHelper::addVersionTag(payload, version);

    QVERIFY(tagged.startsWith("epdbversion"));
    QCOMPARE(tagged.size(), 11 + 4 + payload.size());

    int extractedVersion = 0;
    QByteArray extractedData;
    QVERIFY(CryptoHelper::extractVersionTag(tagged, extractedVersion, extractedData));
    QCOMPARE(extractedVersion, version);
    QCOMPARE(extractedData, payload);
}

void CryptoHelperTest::testVersionTagMalformedHeader() {
    QByteArray payload = "epdbbadhead\x03\x00\x00\x00SomePayload";
    int outVersion = 0;
    QByteArray outData;
    QVERIFY(!CryptoHelper::extractVersionTag(payload, outVersion, outData));
}

void CryptoHelperTest::testVersionTagTruncated() {
    QByteArray shortTag = "epdbversion12"; // 13 bytes < 15
    int outVersion = 0;
    QByteArray outData;
    QVERIFY(!CryptoHelper::extractVersionTag(shortTag, outVersion, outData));
}
