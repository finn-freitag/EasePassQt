#include "PasswordGenerator.h"

#include "AppSettings.h"
#include "PasswordStrength.h"

#include <QRandomGenerator>
#include <vector>
#include <algorithm>

namespace EasePass::Core {

QString PasswordGenerator::generateFromCharset(int length, const QString &allowedChars, int maxAttempts) {
    const QString chars = allowedChars.isEmpty() ? AppSettings::Defaults::PasswordChars : allowedChars;
    length = std::max(1, length);
    const auto includes = PasswordStrength::getAllowedIncludes(chars);
    auto *rng = QRandomGenerator::system();
    QString password;
    for (int attempt = 0; attempt < std::max(1, maxAttempts); ++attempt) {
        password.clear();
        password.reserve(length);
        for (int i = 0; i < length; ++i)
            password.append(chars[int(rng->bounded(quint32(chars.size())))]);
        if (PasswordStrength::isSecure(password, length, includes.includeUpper, includes.includeLower,
                                       includes.includeSpecial, includes.includeNumber))
            break;
    }
    return password;
}

} // namespace EasePass::Core
