#pragma once

#include <QString>

namespace EasePass::Core {

class PasswordGenerator {
public:
    /**
     * Draws @p length characters from @p allowedChars until the result passes
     * PasswordStrength::isSecure() for the character classes @p allowedChars can provide
     * (and contains no common sequence). Gives up after @p maxAttempts and returns the last
     * candidate (e.g. allowedChars = "a" can never be secure).
     * Empty @p allowedChars falls back to AppSettings::Defaults::PasswordChars; length >= 1.
     */
    static QString generateFromCharset(int length, const QString &allowedChars, int maxAttempts = 10000);
};

} // namespace EasePass::Core
