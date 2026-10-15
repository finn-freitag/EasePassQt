#pragma once

#include <QObject>

class TotpHelperTest : public QObject {
    Q_OBJECT

private slots:
    void testRfc6238Sha1Vectors();
    void testRfc6238Sha256Vectors();
    void testRfc6238Sha512Vectors();
    void testDigitsAndInterval();
    void testBase32Decoding();
    void testRemainingSecondsAndProgress();
    void testRemainingSecondsWithExplicitZeroTimestamp();
    void testClockInjection();
    void testOtpauthUriParsingValid();
    void testOtpauthUriParsingOddCasing();
    void testOtpauthUriParsingUrlEncoded();
    void testOtpauthUriParsingMissingSecretFails();
    void testOtpauthUriRoundtrip();
};
