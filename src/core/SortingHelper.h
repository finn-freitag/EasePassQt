#pragma once

#include "DatabaseFile.h"
#include "DatabaseService.h"
#include "IClock.h"
#include "PasswordItem.h"
#include "PasswordStrength.h"

#include <QDate>
#include <QList>
#include <QString>
#include <optional>

namespace EasePass::Core {

enum class SortMode {
    Name = 0,
    Username,
    Notes,
    FirstTag,
    Website,
    PopularLastYear,   ///< C#: ByPopularAllTime ("Popular last year" in UI; clicks > 365 days are pruned by
                       ///< clearOldClicksCache)
    PopularLast30Days, ///< C#: ByPopularLast30Days
    PasswordStrength
};

enum class SortOrder { Ascending = 0, Descending };

/**
 * @brief Sorting and popularity helpers for password items.
 *
 * C#: Helper/SortingHelper.cs, Views/PasswordsPage.xaml.cs (SortClickAction, SwitchOrder_Click),
 *     Core/Database/DatabaseItem.cs (ClearOldClicksCache), Views/SettingsPage.xaml.cs (ResetPopularity_Click).
 */
class SortingHelper {
public:
    /// Parses a click date string formatted as "d.M.yyyy" (e.g. "14.10.2026").
    static std::optional<QDate> parseClickDate(const QString &clickStr);

    /// C#: SortingHelper.CountPopularityLast30Days
    /// Counts clicks where `0 <= (now - date) < daysDeadline` (or in the past within `daysDeadline` days).
    /// Note: C# uses `now - date < TimeSpan.FromDays(daysDeadline)` where `now` has current time of day
    /// and `date` is midnight, meaning `date.daysTo(nowDate) < daysDeadline` (for `now` at midnight) or
    /// `nowSecs - dateMidnightSecs < daysDeadline * 86400`.
    static int countPopularityInWindow(const PasswordItem &item, int daysDeadline = 30, const IClock *clock = nullptr);

    static int countPopularityLast30Days(const PasswordItem &item, const IClock *clock = nullptr) {
        return countPopularityInWindow(item, 30, clock);
    }

    static int countPopularityLastYear(const PasswordItem &item, const IClock *clock = nullptr) {
        return countPopularityInWindow(item, 365, clock);
    }

    // --- Three-way comparators (< 0 if item1 comes before item2, 0 if equal, > 0 if after) ---
    // C#: SortingHelper.By*
    static int compareStrings(const QString &s1, const QString &s2);

    static int byDisplayName(const PasswordItem &item1, const PasswordItem &item2);
    static int byUsername(const PasswordItem &item1, const PasswordItem &item2);
    static int byNotes(const PasswordItem &item1, const PasswordItem &item2);
    static int byWebsite(const PasswordItem &item1, const PasswordItem &item2);
    static int byFirstTag(const PasswordItem &item1, const PasswordItem &item2);

    /// C#: SortingHelper.ByPopularAllTime (and PopularLastYear when clicks are within 365 days).
    /// If @p clock is provided, only clicks within the last 365 days are counted; if @p clock is
    /// nullptr, all clicks in `item.clicks` are counted (matching C# ByPopularAllTime after ClearOldClicksCache).
    static int byPopularAllTime(const PasswordItem &item1, const PasswordItem &item2);
    static int byPopularLastYear(const PasswordItem &item1, const PasswordItem &item2, const IClock *clock = nullptr);
    static int byPopularLast30Days(const PasswordItem &item1, const PasswordItem &item2, const IClock *clock = nullptr);
    static int byPasswordStrength(const PasswordItem &item1, const PasswordItem &item2,
                                  int minLength = PasswordStrength::DefaultMinLength);

    static int compare(const PasswordItem &item1, const PasswordItem &item2, SortMode mode,
                       SortOrder order = SortOrder::Ascending, const IClock *clock = nullptr,
                       int minPasswordLength = PasswordStrength::DefaultMinLength);

    /// Stable in-place sort of @p items according to @p mode and @p order.
    static void sort(QList<PasswordItem> &items, SortMode mode, SortOrder order = SortOrder::Ascending,
                     const IClock *clock = nullptr, int minPasswordLength = PasswordStrength::DefaultMinLength);

    /// Returns a sorted copy of @p items.
    static QList<PasswordItem> sorted(const QList<PasswordItem> &items, SortMode mode,
                                      SortOrder order = SortOrder::Ascending, const IClock *clock = nullptr,
                                      int minPasswordLength = PasswordStrength::DefaultMinLength);

    /// C#: DatabaseItem.ClearOldClicksCache (removes clicks older than 365 days)
    static void clearOldClicksCache(QList<PasswordItem> &items, const IClock *clock = nullptr);
    static void clearOldClicksCache(DatabaseFile &db, const IClock *clock = nullptr);

    /// C#: SettingsPage.ResetPopularity_Click (clears all clicks on all items)
    static void resetPopularity(QList<PasswordItem> &items);
    static void resetPopularity(DatabaseFile &db);
    static bool resetPopularity(DatabaseService &service);
};

} // namespace EasePass::Core
