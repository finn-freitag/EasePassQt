#pragma once

#include <QString>

namespace EasePass::Core {

struct PasswordGeneratorOptions {
    int length = 16;
    bool useUpper = true;
    bool useLower = true;
    bool useDigits = true;
    bool useSymbols = true;
    QString customSymbols = "!@#$%^&*()-_=+[]{}|;:,.<>?";
};

class PasswordGenerator {
public:
    static QString generate(const PasswordGeneratorOptions& options = PasswordGeneratorOptions());

    /**
     * C#: Helper/Security/Generator/PasswordHelper.GeneratePassword without the leak check:
     * draws @p length characters from @p allowedChars until the result passes
     * PasswordStrength::isSecure() for the character classes @p allowedChars can provide
     * (and contains no common sequence). Unlike C#, gives up after @p maxAttempts and returns
     * the last candidate (C# loops forever for e.g. allowedChars = "a").
     * Empty @p allowedChars falls back to AppSettings::Defaults::PasswordChars; length >= 1.
     */
    static QString generateFromCharset(int length, const QString &allowedChars, int maxAttempts = 10000);
};

} // namespace EasePass::Core
