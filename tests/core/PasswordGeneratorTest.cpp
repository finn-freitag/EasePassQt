#include "PasswordGeneratorTest.h"
#include "PasswordGenerator.h"
#include "AppSettings.h"
#include "PasswordStrength.h"
#include <QTest>
#include <QSet>
#include <algorithm>

using namespace EasePass::Core;

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
