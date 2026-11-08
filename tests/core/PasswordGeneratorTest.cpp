#include "PasswordGeneratorTest.h"
#include "PasswordGenerator.h"
#include "AppSettings.h"
#include "PasswordStrength.h"
#include <QTest>
#include <QSet>
#include <algorithm>

using namespace EasePass::Core;

void PasswordGeneratorTest::testLengthBounds() {
    PasswordGeneratorOptions opts;
    opts.useUpper = true;
    opts.useLower = true;
    opts.useDigits = true;
    opts.useSymbols = true;

    // Length smaller than mandatory classes (4) clamps to 4
    opts.length = 2;
    QString pwShort = PasswordGenerator::generate(opts);
    QCOMPARE(pwShort.length(), 4);

    opts.length = 0;
    QString pwZero = PasswordGenerator::generate(opts);
    QCOMPARE(pwZero.length(), 4);

    opts.length = 100;
    QString pwLong = PasswordGenerator::generate(opts);
    QCOMPARE(pwLong.length(), 100);
}

void PasswordGeneratorTest::testEachEnabledClassPresent() {
    PasswordGeneratorOptions opts;
    opts.length = 24;
    opts.useUpper = true;
    opts.useLower = true;
    opts.useDigits = true;
    opts.useSymbols = true;

    for (int i = 0; i < 20; ++i) {
        QString pw = PasswordGenerator::generate(opts);
        bool hasUpper = false, hasLower = false, hasDigit = false, hasSymbol = false;
        for (QChar c : pw) {
            if (c.isUpper())
                hasUpper = true;
            else if (c.isLower())
                hasLower = true;
            else if (c.isDigit())
                hasDigit = true;
            else
                hasSymbol = true;
        }
        QVERIFY(hasUpper);
        QVERIFY(hasLower);
        QVERIFY(hasDigit);
        QVERIFY(hasSymbol);
    }
}

void PasswordGeneratorTest::testSelectiveClasses() {
    PasswordGeneratorOptions opts;
    opts.length = 20;
    opts.useUpper = true;
    opts.useLower = false;
    opts.useDigits = true;
    opts.useSymbols = false;

    for (int i = 0; i < 20; ++i) {
        QString pw = PasswordGenerator::generate(opts);
        bool hasUpper = false, hasDigit = false;
        for (QChar c : pw) {
            if (c.isUpper())
                hasUpper = true;
            else if (c.isDigit())
                hasDigit = true;
            else
                QFAIL("Password contains unexpected character class!");
        }
        QVERIFY(hasUpper);
        QVERIFY(hasDigit);
    }
}

void PasswordGeneratorTest::testEmptyPoolFallback() {
    PasswordGeneratorOptions opts;
    opts.length = 16;
    opts.useUpper = false;
    opts.useLower = false;
    opts.useDigits = false;
    opts.useSymbols = false;

    QString pw = PasswordGenerator::generate(opts);
    QCOMPARE(pw.length(), 16);

    for (QChar c : pw) {
        QVERIFY(c.isLower() || c.isDigit());
    }
}

void PasswordGeneratorTest::testCustomSymbolsOnly() {
    PasswordGeneratorOptions opts;
    opts.length = 20;
    opts.useUpper = false;
    opts.useLower = false;
    opts.useDigits = false;
    opts.useSymbols = true;
    opts.customSymbols = "!@";

    QString pw = PasswordGenerator::generate(opts);
    QCOMPARE(pw.length(), 20);

    for (QChar c : pw) {
        QVERIFY(c == '!' || c == '@');
    }
}

void PasswordGeneratorTest::testDistributionSanity() {
    PasswordGeneratorOptions opts;
    opts.length = 32;

    QSet<QString> generated;
    for (int i = 0; i < 50; ++i) {
        generated.insert(PasswordGenerator::generate(opts));
    }

    // Every generated password should be unique across 50 iterations
    QCOMPARE(generated.size(), 50);
}

// C#: PasswordHelper.GeneratePassword - stored length and allowed characters (T15).
void PasswordGeneratorTest::testFromCharsetRespectsLengthAndChars() {
    const QString chars = QStringLiteral("abcXYZ789-");
    for (int i = 0; i < 20; ++i) {
        const QString pw = PasswordGenerator::generateFromCharset(12, chars);
        QCOMPARE(pw.length(), 12);
        for (QChar c : pw)
            QVERIFY2(chars.contains(c), qPrintable(pw));
    }
    // Empty character set -> default characters; length is at least 1.
    const QString def = PasswordGenerator::generateFromCharset(20, QString());
    QCOMPARE(def.length(), 20);
    for (QChar c : def)
        QVERIFY(AppSettings::Defaults::PasswordChars.contains(c));
    QCOMPARE(PasswordGenerator::generateFromCharset(0, chars).length(), 1);
}

void PasswordGeneratorTest::testFromCharsetSecureAndTerminates() {
    // Every class the characters offer is used and no common sequence appears.
    for (int i = 0; i < 20; ++i) {
        const QString pw = PasswordGenerator::generateFromCharset(15, AppSettings::Defaults::PasswordChars);
        QVERIFY2(PasswordStrength::isSecure(pw, 15), qPrintable(pw));
    }
    // Only lowercase + digits allowed: no uppercase/special required.
    const QString pw = PasswordGenerator::generateFromCharset(16, QStringLiteral("abcdefghijk0123456789"));
    QVERIFY(std::any_of(pw.begin(), pw.end(), [](QChar c) { return c.isDigit(); }));
    QVERIFY(std::any_of(pw.begin(), pw.end(), [](QChar c) { return c.isLower(); }));
    // Impossible (C# loops forever): gives up and still returns a password of the length.
    QCOMPARE(PasswordGenerator::generateFromCharset(8, QStringLiteral("a"), 50), QStringLiteral("aaaaaaaa"));
}
