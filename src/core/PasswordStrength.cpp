#include "PasswordStrength.h"

namespace EasePass::Core {

bool PasswordStrength::isSpecialChar(QChar c) {
    // C#: Extensions/CharExtension.cs IsSpecialChar
    switch (c.unicode()) {
    case u'!':
    case u'"':
    case u'\u00A7': // '§'
    case u'$':
    case u'%':
    case u'&':
    case u'/':
    case u'{':
    case u'(':
    case u'[':
    case u')':
    case u']':
    case u'=':
    case u'}':
    case u'?':
    case u'\\':
    case u'`':
    case u'\u00B4': // '´'
    case u'*':
    case u'+':
    case u'~':
    case u'\u00B3': // '³'
    case u'\'':
    case u'#':
    case u'-':
    case u'_':
    case u'.':
    case u':':
    case u',':
    case u';':
    case u'<':
    case u'>':
    case u'|':
    case u'\u00B2': // '²'
        return true;
    default:
        // Also treat other standard ASCII punctuation symbols (e.g. '@', '^') as special
        // if they are non-alphanumeric printable ASCII symbols, while keeping exact parity
        // with every C# special char above.
        if (c == u'@' || c == u'^') {
            return true;
        }
        return false;
    }
}

PasswordStrength::AllowedIncludes PasswordStrength::getAllowedIncludes(QStringView text) {
    // C#: PasswordHelper.GetAllowedIncludes
    AllowedIncludes res;
    for (QChar c : text) {
        if (!res.includeUpper && c.isUpper()) {
            res.includeUpper = true;
        } else if (!res.includeLower && c.isLower()) {
            res.includeLower = true;
        } else if (!res.includeSpecial && isSpecialChar(c)) {
            res.includeSpecial = true;
        } else if (!res.includeNumber && c.isNumber()) {
            res.includeNumber = true;
        }
        if (res.includeUpper && res.includeLower && res.includeSpecial && res.includeNumber) {
            break;
        }
    }
    return res;
}

QList<CommonPasswordSequence> PasswordStrength::defaultCommonSequences(const QString &systemUsername) {
    // C#: PasswordHelper.CommonSequences + PasswordHelper.Init()
    static const QString kAbc = QStringLiteral("abcdefghijklmnopqrstuvwxyz");

    static const QList<CommonPasswordSequence> kBaseSequences = []() {
        QList<CommonPasswordSequence> list{
            CommonPasswordSequence(QStringLiteral("012345678909876543210"), IntegerMinRepeat), // forward and backward
            CommonPasswordSequence(kAbc + kAbc, StringMinRepeat),                              // double abc
            CommonPasswordSequence(QStringLiteral("qwertyuiopasdfghjklzxcvbnm"), StringMinRepeat), // English keyboard
            CommonPasswordSequence(QStringLiteral("qwertzuiopasdfghjklyxcvbnm"), StringMinRepeat), // German keyboard
            CommonPasswordSequence(QStringLiteral("hello")),
            CommonPasswordSequence(QStringLiteral("test")),
            CommonPasswordSequence(QStringLiteral("password")),
            CommonPasswordSequence(QStringLiteral("service")),
            CommonPasswordSequence(QStringLiteral("monkey")),
            CommonPasswordSequence(QStringLiteral("letmein")),
            CommonPasswordSequence(QStringLiteral("football")),
            CommonPasswordSequence(QStringLiteral("baseball")),
            CommonPasswordSequence(QStringLiteral("princess")),
            CommonPasswordSequence(QStringLiteral("sunshine")),
            CommonPasswordSequence(QStringLiteral("iloveyou")),
            CommonPasswordSequence(QStringLiteral("admin")),
            CommonPasswordSequence(QStringLiteral("starwars")),
            CommonPasswordSequence(QStringLiteral("master")),
            CommonPasswordSequence(QStringLiteral("lovely")),
            CommonPasswordSequence(QStringLiteral("welcome")),
            CommonPasswordSequence(QStringLiteral("dragon")),
            CommonPasswordSequence(QStringLiteral("superman")),
        };

        // Repeating letters, e.g. "aaaaa"
        for (QChar ch : kAbc) {
            list.append(CommonPasswordSequence(QString(StringMinRepeat, ch), StringMinRepeat));
        }

        // Repeating numbers, e.g. "55555" with IntegerMinRepeat (4)
        for (int i = 0; i < 10; ++i) {
            list.append(CommonPasswordSequence(QString(StringMinRepeat, QChar(u'0' + i)), IntegerMinRepeat));
        }

        return list;
    }();

    if (systemUsername.trimmed().isEmpty()) {
        return kBaseSequences;
    }

    QList<CommonPasswordSequence> result = kBaseSequences;
    const QStringList parts = systemUsername.split(u' ', Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        bool isNumber = false;
        part.toInt(&isNumber);
        if (!isNumber && !part.isEmpty()) {
            result.append(CommonPasswordSequence(part.toLower()));
        }
    }
    return result;
}

bool PasswordStrength::hasRepeatedCharacters(QStringView password, int letterRepeat, int digitRepeat) {
    if (password.isEmpty()) {
        return false;
    }

    int runLength = 1;
    QChar prev = password[0].toLower();
    for (qsizetype i = 1; i < password.size(); ++i) {
        const QChar curr = password[i].toLower();
        if (curr == prev) {
            ++runLength;
            const int threshold = curr.isDigit() ? digitRepeat : letterRepeat;
            if (threshold > 0 && runLength >= threshold) {
                return true;
            }
        } else {
            prev = curr;
            runLength = 1;
        }
    }
    return false;
}

bool PasswordStrength::containsCommonSequences(QStringView password, const QString &systemUsername) {
    if (password.isEmpty()) {
        return false;
    }

    const QList<CommonPasswordSequence> sequences = defaultCommonSequences(systemUsername);
    for (const CommonPasswordSequence &seq : sequences) {
        if (seq.containsSequence(password)) {
            return true;
        }
    }

    // Also catch repeated non-ASCII letters or repeated symbols (>= StringMinRepeat)
    if (hasRepeatedCharacters(password, StringMinRepeat, IntegerMinRepeat)) {
        return true;
    }

    return false;
}

bool PasswordStrength::isSecure(QStringView password, int minLength, bool includeUpper, bool includeLower,
                                bool includeSpecial, bool includeNumber) {
    // C#: PasswordHelper.IsSecure
    if (password.length() < minLength) {
        return false;
    }
    if (containsCommonSequences(password)) {
        return false;
    }

    bool upper = !includeUpper;
    bool lower = !includeLower;
    bool special = !includeSpecial;
    bool number = !includeNumber;

    if (upper && lower && special && number) {
        return true;
    }

    for (QChar c : password) {
        if (!upper && c.isUpper()) {
            upper = true;
        } else if (!lower && c.isLower()) {
            lower = true;
        } else if (!special && isSpecialChar(c)) {
            special = true;
        } else if (!number && c.isNumber()) {
            number = true;
        }
        if (upper && lower && special && number) {
            return true;
        }
    }
    return upper && lower && special && number;
}

std::array<bool, 6> PasswordStrength::evaluateCriteria(QStringView password, int minLength) {
    // C#: PasswordHelper.EvaluatePassword
    const AllowedIncludes inc = getAllowedIncludes(password);
    return {
        inc.includeLower,   inc.includeUpper,  password.length() >= minLength,
        inc.includeSpecial, inc.includeNumber, !containsCommonSequences(password),
    };
}

PasswordStrength::Evaluation PasswordStrength::evaluate(QStringView password, int minLength, int reuseCount,
                                                        bool existingSingleTime) {
    Evaluation eval;
    const std::array<bool, 6> checks = evaluateCriteria(password, minLength);
    eval.hasLower = checks[0];
    eval.hasUpper = checks[1];
    eval.meetsMinLength = checks[2];
    eval.hasSpecial = checks[3];
    eval.hasNumber = checks[4];
    eval.noCommonSequences = checks[5];

    eval.reuseCount = std::max(0, reuseCount);
    eval.notReused = eval.reuseCount < (existingSingleTime ? 2 : 1);

    int passed = 0;
    int score = 0;
    for (bool ok : checks) {
        if (ok) {
            ++passed;
            score += 1;
        } else {
            score -= 1;
        }
    }
    eval.passedCriteria = passed;
    eval.score = score;

    if (passed <= 1) {
        eval.rating = Rating::VeryWeak;
    } else if (passed <= 3) {
        eval.rating = Rating::Weak;
    } else if (passed == 4) {
        eval.rating = Rating::Fair;
    } else if (passed == 5) {
        eval.rating = Rating::Good;
    } else {
        eval.rating = Rating::Strong;
    }

    return eval;
}

PasswordStrength::Evaluation PasswordStrength::evaluate(QStringView password, const DatabaseFile &db,
                                                        bool existingSingleTime, int minLength) {
    const int occurrences = db.countOccurrences(password.toString());
    return evaluate(password, minLength, occurrences, existingSingleTime);
}

PasswordStrength::Evaluation PasswordStrength::evaluate(QStringView password, const DatabaseService &service,
                                                        bool existingSingleTime, int minLength) {
    const int occurrences = service.countOccurrences(password.toString());
    return evaluate(password, minLength, occurrences, existingSingleTime);
}

bool PasswordStrength::checkPasswordNotReused(const QString &password, const DatabaseFile &db,
                                              bool existingSingleTime) {
    return db.countOccurrences(password) < (existingSingleTime ? 2 : 1);
}

bool PasswordStrength::checkPasswordNotReused(const QString &password, const DatabaseService &service,
                                              bool existingSingleTime) {
    return service.countOccurrences(password) < (existingSingleTime ? 2 : 1);
}

int PasswordStrength::strengthScore(QStringView password, int minLength) {
    const std::array<bool, 6> checks = evaluateCriteria(password, minLength);
    int score = 0;
    for (bool ok : checks) {
        score += ok ? 1 : -1;
    }
    return score;
}

} // namespace EasePass::Core
