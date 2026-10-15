#pragma once

#include <QObject>

class CryptoHelperTest : public QObject {
    Q_OBJECT

private slots:
    void testDeriveOuterKeyKnownVector();
    void testDeriveInnerKeyKnownVector();
    void testAssociatedDataOldVsNew();
    void testToUtf16LE();
    void testCustomToBase64();
    void testAesEncryptionDecryptionRoundtrip();
    void testAesWrongKeyFailsDecryption();
    void testAesCorruptedCiphertextFails();
    void testAesTruncatedCiphertextFails();
    void testVersionTagGood();
    void testVersionTagMalformedHeader();
    void testVersionTagTruncated();
};
