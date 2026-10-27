#pragma once

#include "AppSettings.h"
#include "CommonPasswordSequence.h"
#include "DatabaseFile.h"
#include "DatabaseService.h"

#include <QList>
#include <QString>
#include <QStringView>
#include <array>

namespace EasePass::Core {

/**
 * @brief Password strength evaluation, common sequence detection, character class checks,
 *        and database password reuse checks.
 *
 * C#: Helper/Security/Generator/PasswordHelper.cs, Extensions/CharExtension.cs,
 *     Controls/PasswordSafetyChart.xaml.cs, Helper/SortingHelper.cs (ByPasswordStrength).
 */
class PasswordStrength {
public:
    enum class Rating {
        VeryWeak = 0, ///< 0-1 basic criteria met
        Weak,         ///< 2-3 basic criteria met
        Fair,         ///< 4 basic criteria met
        Good,         ///< 5 basic criteria met
        Strong        ///< all 6 basic criteria met
    };

    struct AllowedIncludes {
        bool includeUpper = false;
        bool includeLower = false;
        bool includeSpecial = false;
        bool includeNumber = false;
    };

    /**
     * @brief Full password strength evaluation result.
     *
     * C#: PasswordHelper.EvaluatePassword (6 criteria: lower, upper, length, special, number,
     * no common sequences) + PasswordSafetyChart.CheckPasswordAlreadyUsed (reuse count).
     */
    struct Evaluation {
        bool hasLower = false;
        bool hasUpper = false;
        bool meetsMinLength = false;
        bool hasSpecial = false;
        bool hasNumber = false;
        bool noCommonSequences = false;

        int reuseCount = 0;    ///< Number of occurrences in the database (0 if no DB passed)
        bool notReused = true; ///< True if reuseCount < (existingSingleTime ? 2 : 1)

        int passedCriteria = 0; ///< Count of the 6 basic criteria met (0..6)
        int score = 0;          ///< C# SortingHelper.ByPasswordStrength sum (+1 for true, -1 for false): -6..+6
        Rating rating = Rating::VeryWeak;

        /// Returns the 6 basic criteria in the exact order of C# PasswordHelper.EvaluatePassword:
        /// [0] Lowercase, [1] Uppercase, [2] Length, [3] Special char, [4] Number, [5] No common sequence.
        std::array<bool, 6> toCriteriaArray() const {
            return {hasLower, hasUpper, meetsMinLength, hasSpecial, hasNumber, noCommonSequences};
        }
    };

    static constexpr int DefaultMinLength = AppSettings::Defaults::PasswordLength; // 15
    static constexpr int StringMinRepeat = 5;
    static constexpr int IntegerMinRepeat = 4;

    /// C#: Extensions/CharExtension.cs IsSpecialChar
    static bool isSpecialChar(QChar c);

    /// C#: PasswordHelper.GetAllowedIncludes
    static AllowedIncludes getAllowedIncludes(QStringView text);

    /// C#: PasswordHelper.CommonSequences (incl. Init() repeated chars/digits and optional username words)
    static QList<CommonPasswordSequence> defaultCommonSequences(const QString &systemUsername = QString());

    /// Checks if @p password contains any common sequence (keyboard walks, dictionary words,
    /// numeric runs, or repeated letters/digits).
    static bool containsCommonSequences(QStringView password, const QString &systemUsername = QString());

    /// Checks if @p password contains `minRepeat` or more consecutive identical characters.
    static bool hasRepeatedCharacters(QStringView password, int letterRepeat = StringMinRepeat,
                                      int digitRepeat = IntegerMinRepeat);

    /// C#: PasswordHelper.IsSecure
    static bool isSecure(QStringView password, int minLength = DefaultMinLength, bool includeUpper = true,
                         bool includeLower = true, bool includeSpecial = true, bool includeNumber = true);

    /// C#: PasswordHelper.EvaluatePassword (returns the 6 boolean criteria)
    static std::array<bool, 6> evaluateCriteria(QStringView password, int minLength = DefaultMinLength);

    /// Evaluates password strength without database context (or with an explicit reuseCount).
    static Evaluation evaluate(QStringView password, int minLength = DefaultMinLength, int reuseCount = 0,
                               bool existingSingleTime = false);

    /// Evaluates password strength with reuse count from @p db (via DatabaseFile::countOccurrences).
    static Evaluation evaluate(QStringView password, const DatabaseFile &db, bool existingSingleTime = true,
                               int minLength = DefaultMinLength);

    /// Evaluates password strength with reuse count from @p service (via DatabaseService::countOccurrences).
    static Evaluation evaluate(QStringView password, const DatabaseService &service, bool existingSingleTime = true,
                               int minLength = DefaultMinLength);

    /// C#: PasswordSafetyChart.CheckPasswordAlreadyUsed: true if occurrences < (existingSingleTime ? 2 : 1)
    static bool checkPasswordNotReused(const QString &password, const DatabaseFile &db,
                                       bool existingSingleTime = false);
    static bool checkPasswordNotReused(const QString &password, const DatabaseService &service,
                                       bool existingSingleTime = false);

    /// Computes the C# SortingHelper.ByPasswordStrength score (-6..+6) for @p password.
    static int strengthScore(QStringView password, int minLength = DefaultMinLength);
};

} // namespace EasePass::Core
