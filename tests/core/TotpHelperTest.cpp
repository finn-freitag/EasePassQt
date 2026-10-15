#include "TotpHelperTest.h"
#include "TotpHelper.h"
#include "IClock.h"
#include <QTest>

using namespace EasePass::Core;

namespace {
class FakeClock : public IClock {
public:
    explicit FakeClock(qint64 time) : m_time(time) {}
    qint64 currentSecsSinceEpoch() const override { return m_time; }
    void setTime(qint64 t) { m_time = t; }

private:
    qint64 m_time;
};
} // namespace

void TotpHelperTest::testRfc6238Sha1Vectors() {
    // RFC 6238 Secret for SHA1 (20 bytes "12345678901234567890")
    QString secret = "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ";

    // Table from RFC 6238 Appendix B
    QCOMPARE(TotpHelper::generateToken(secret, 59, 8, 30, "SHA1"), QString("94287082"));
    QCOMPARE(TotpHelper::generateToken(secret, 1111111109, 8, 30, "SHA1"), QString("07081804"));
    QCOMPARE(TotpHelper::generateToken(secret, 1111111111, 8, 30, "SHA1"), QString("14050471"));
    QCOMPARE(TotpHelper::generateToken(secret, 1234567890, 8, 30, "SHA1"), QString("89005924"));
    QCOMPARE(TotpHelper::generateToken(secret, 2000000000, 8, 30, "SHA1"), QString("69279037"));
}

void TotpHelperTest::testRfc6238Sha256Vectors() {
    // RFC 6238 Secret for SHA256 (32 bytes "12345678901234567890123456789012")
    QString secret = "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQGEZA";

    QCOMPARE(TotpHelper::generateToken(secret, 59, 8, 30, "SHA256"), QString("46119246"));
    QCOMPARE(TotpHelper::generateToken(secret, 1111111109, 8, 30, "SHA256"), QString("68084774"));
    QCOMPARE(TotpHelper::generateToken(secret, 1111111111, 8, 30, "SHA256"), QString("67062674"));
    QCOMPARE(TotpHelper::generateToken(secret, 1234567890, 8, 30, "SHA256"), QString("91819424"));
    QCOMPARE(TotpHelper::generateToken(secret, 2000000000, 8, 30, "SHA256"), QString("90698825"));
}

void TotpHelperTest::testRfc6238Sha512Vectors() {
    // RFC 6238 Secret for SHA512 (64 bytes: 3x 20-byte chunks + 4-byte chunk)
    QString secret =
        "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQGEZDGNA=";

    QCOMPARE(TotpHelper::generateToken(secret, 59, 8, 30, "SHA512"), QString("90693936"));
    QCOMPARE(TotpHelper::generateToken(secret, 1111111109, 8, 30, "SHA512"), QString("25091201"));
    QCOMPARE(TotpHelper::generateToken(secret, 1111111111, 8, 30, "SHA512"), QString("99943326"));
    QCOMPARE(TotpHelper::generateToken(secret, 1234567890, 8, 30, "SHA512"), QString("93441116"));
    QCOMPARE(TotpHelper::generateToken(secret, 2000000000, 8, 30, "SHA512"), QString("38618901"));
}

void TotpHelperTest::testDigitsAndInterval() {
    QString secret = "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ";

    // 6 digits vs 8 digits
    QString token6 = TotpHelper::generateToken(secret, 59, 6, 30, "SHA1");
    QString token8 = TotpHelper::generateToken(secret, 59, 8, 30, "SHA1");
    QCOMPARE(token6.length(), 6);
    QCOMPARE(token8.length(), 8);
    QCOMPARE(token6, QString("287082"));
    QCOMPARE(token8, QString("94287082"));

    // Interval 30 vs 60
    QString tokenPeriod30 = TotpHelper::generateToken(secret, 60, 6, 30, "SHA1");
    QString tokenPeriod60 = TotpHelper::generateToken(secret, 60, 6, 60, "SHA1");
    // At t=60, counter for 30s is 2, while counter for 60s is 1 (same as t=59 for 30s)
    QCOMPARE(tokenPeriod60, token6);
    QVERIFY(tokenPeriod30 != tokenPeriod60);
}

void TotpHelperTest::testBase32Decoding() {
    // "Hello" (5 bytes) in base32 is "JBSWY3DP"
    QByteArray decoded = TotpHelper::base32Decode("JBSWY3DP");
    QCOMPARE(decoded, QByteArray("Hello"));

    // "Hello!" (6 bytes) in base32 is "JBSWY3DPEE"
    QByteArray decodedExcl = TotpHelper::base32Decode("JBSWY3DPEE");
    QCOMPARE(decodedExcl, QByteArray("Hello!"));

    // Padding, spaces, hyphens, and lowercase should be handled
    QByteArray withFormatting = TotpHelper::base32Decode("  jb-sw y3dp==  ");
    QCOMPARE(withFormatting, QByteArray("Hello"));

    // Empty input returns empty
    QCOMPARE(TotpHelper::base32Decode(""), QByteArray());
}

void TotpHelperTest::testRemainingSecondsAndProgress() {
    // Period 30
    QCOMPARE(TotpHelper::getRemainingSeconds(30, 25), 5);
    QCOMPARE(TotpHelper::getRemainingSeconds(30, 29), 1);
    QCOMPARE(TotpHelper::getRemainingSeconds(30, 30), 30);
    QCOMPARE(TotpHelper::getRemainingSeconds(30, 60), 30);

    // Progress
    QCOMPARE(TotpHelper::getProgress(30, 25), 5.0 / 30.0);
    QCOMPARE(TotpHelper::getProgress(30, 15), 0.5);
    QCOMPARE(TotpHelper::getProgress(30, 30), 1.0);
}

void TotpHelperTest::testRemainingSecondsWithExplicitZeroTimestamp() {
    // Fix for P11: explicit t=0 should NOT be replaced with current time
    int rem = TotpHelper::getRemainingSeconds(30, 0);
    QCOMPARE(rem, 30);

    double prog = TotpHelper::getProgress(30, 0);
    QCOMPARE(prog, 1.0);
}

void TotpHelperTest::testClockInjection() {
    QString secret = "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ";
    FakeClock clock(1111111109);

    QString token = TotpHelper::generateCurrentToken(secret, 8, 30, "SHA1", &clock);
    QCOMPARE(token, QString("07081804"));

    int rem = TotpHelper::getRemainingSeconds(30, std::nullopt, &clock);
    // 1111111109 % 30 = 29 -> remaining 30 - 29 = 1
    QCOMPARE(rem, 1);
}

void TotpHelperTest::testOtpauthUriParsingValid() {
    QString uri = "otpauth://totp/"
                  "GitHub:user%40example.com?secret=JBSWY3DPEHPK3PXP&issuer=GitHub&algorithm=SHA256&digits=8&period=60";
    TotpParameters params;
    QVERIFY(TotpHelper::parseOtpauthUri(uri, params));

    QCOMPARE(params.secret, QString("JBSWY3DPEHPK3PXP"));
    QCOMPARE(params.issuer, QString("GitHub"));
    QCOMPARE(params.account, QString("user@example.com"));
    QCOMPARE(params.algorithm, QString("SHA256"));
    QCOMPARE(params.digits, 8);
    QCOMPARE(params.period, 60);
}

void TotpHelperTest::testOtpauthUriParsingOddCasing() {
    QString uri = "OTPAUTH://TOTP/Provider:alice?secret=JBSWY3DPEHPK3PXP";
    TotpParameters params;
    QVERIFY(TotpHelper::parseOtpauthUri(uri, params));
    QCOMPARE(params.secret, QString("JBSWY3DPEHPK3PXP"));
    QCOMPARE(params.issuer, QString("Provider"));
    QCOMPARE(params.account, QString("alice"));
    QCOMPARE(params.algorithm, QString("SHA1"));
    QCOMPARE(params.digits, 6);
    QCOMPARE(params.period, 30);
}

void TotpHelperTest::testOtpauthUriParsingUrlEncoded() {
    QString uri = "otpauth://totp/Acme%20Corp:john%2Bdoe%40acme.com?secret=JBSWY3DPEHPK3PXP";
    TotpParameters params;
    QVERIFY(TotpHelper::parseOtpauthUri(uri, params));
    QCOMPARE(params.issuer, QString("Acme Corp"));
    QCOMPARE(params.account, QString("john+doe@acme.com"));
}

void TotpHelperTest::testOtpauthUriParsingMissingSecretFails() {
    QString uri = "otpauth://totp/GitHub:user?issuer=GitHub";
    TotpParameters params;
    QVERIFY(!TotpHelper::parseOtpauthUri(uri, params));

    QString invalidScheme = "https://example.com/totp?secret=JBSWY3DPEHPK3PXP";
    QVERIFY(!TotpHelper::parseOtpauthUri(invalidScheme, params));
}

void TotpHelperTest::testOtpauthUriRoundtrip() {
    TotpParameters orig;
    orig.secret = "HXDMVJECJJWSRB3HWIZR4IFUGFTMXBOZ";
    orig.issuer = "Linux Distro";
    orig.account = "dev@distro.org";
    orig.algorithm = "SHA512";
    orig.digits = 8;
    orig.period = 45;

    QString uri = TotpHelper::generateOtpauthUri(orig);
    TotpParameters parsed;
    QVERIFY(TotpHelper::parseOtpauthUri(uri, parsed));

    QCOMPARE(parsed.secret, orig.secret);
    QCOMPARE(parsed.issuer, orig.issuer);
    QCOMPARE(parsed.account, orig.account);
    QCOMPARE(parsed.algorithm, orig.algorithm);
    QCOMPARE(parsed.digits, orig.digits);
    QCOMPARE(parsed.period, orig.period);
}
