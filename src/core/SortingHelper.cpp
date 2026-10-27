#include "SortingHelper.h"

#include <QCollator>
#include <QDateTime>
#include <QTimeZone>
#include <algorithm>

namespace EasePass::Core {

namespace {

QDateTime currentDateTime(const IClock *clock) {
    if (clock) {
        return QDateTime::fromSecsSinceEpoch(clock->currentSecsSinceEpoch(), QTimeZone::UTC);
    }
    return QDateTime::currentDateTimeUtc();
}

} // namespace

std::optional<QDate> SortingHelper::parseClickDate(const QString &clickStr) {
    // C#: string[] splitted = pmi.Clicks[i].Split('.');
    // DateTime date = new DateTime(Convert.ToInt32(splitted[2]), Convert.ToInt32(splitted[1]),
    // Convert.ToInt32(splitted[0]));
    const QStringList parts = clickStr.trimmed().split(u'.');
    if (parts.size() != 3) {
        return std::nullopt;
    }
    bool okDay = false, okMonth = false, okYear = false;
    const int day = parts[0].toInt(&okDay);
    const int month = parts[1].toInt(&okMonth);
    const int year = parts[2].toInt(&okYear);
    if (!okDay || !okMonth || !okYear) {
        return std::nullopt;
    }
    const QDate date(year, month, day);
    if (!date.isValid()) {
        return std::nullopt;
    }
    return date;
}

int SortingHelper::countPopularityInWindow(const PasswordItem &item, int daysDeadline, const IClock *clock) {
    // C#: SortingHelper.CountPopularityLast30Days:
    // if (now - date < TimeSpan.FromDays(daysDeadline)) pmi1Count++;
    const QDateTime now = currentDateTime(clock);
    const qint64 deadlineSecs = static_cast<qint64>(daysDeadline) * 86400LL;

    int count = 0;
    for (const QString &clickStr : item.clicks) {
        const std::optional<QDate> parsed = parseClickDate(clickStr);
        if (!parsed.has_value()) {
            continue;
        }
        const QDateTime clickMidnight(*parsed, QTime(0, 0, 0), QTimeZone::UTC);
        const qint64 diffSecs = clickMidnight.secsTo(now);
        if (diffSecs < deadlineSecs) {
            ++count;
        }
    }
    return count;
}

int SortingHelper::compareStrings(const QString &s1, const QString &s2) {
    // C#: (item1 ?? "").CompareTo(item2 ?? "") which is culture-sensitive case-insensitive
    // with case tie-breaking. Use QCollator on default locale with case-insensitive primary comparison
    // and deterministic case/codepoint tie-breaking.
    if (s1 == s2) {
        return 0;
    }
    const int ci = QString::localeAwareCompare(s1.toCaseFolded(), s2.toCaseFolded());
    if (ci != 0) {
        return ci < 0 ? -1 : 1;
    }
    const int cs = QString::localeAwareCompare(s1, s2);
    if (cs != 0) {
        return cs < 0 ? -1 : 1;
    }
    const int ord = s1.compare(s2, Qt::CaseSensitive);
    return ord < 0 ? -1 : (ord > 0 ? 1 : 0);
}

int SortingHelper::byDisplayName(const PasswordItem &item1, const PasswordItem &item2) {
    return compareStrings(item1.displayName, item2.displayName);
}

int SortingHelper::byUsername(const PasswordItem &item1, const PasswordItem &item2) {
    return compareStrings(item1.username, item2.username);
}

int SortingHelper::byNotes(const PasswordItem &item1, const PasswordItem &item2) {
    return compareStrings(item1.notes, item2.notes);
}

int SortingHelper::byWebsite(const PasswordItem &item1, const PasswordItem &item2) {
    return compareStrings(item1.website, item2.website);
}

int SortingHelper::byFirstTag(const PasswordItem &item1, const PasswordItem &item2) {
    // C#: SortingHelper.ByFirstTag: items without tags sort after items with tags.
    const bool has1 = !item1.tags.isEmpty();
    const bool has2 = !item2.tags.isEmpty();

    if (!has1 && !has2) {
        return 0;
    }
    if (!has1) {
        return 1;
    }
    if (!has2) {
        return -1;
    }
    return compareStrings(item1.tags.first(), item2.tags.first());
}

int SortingHelper::byPopularAllTime(const PasswordItem &item1, const PasswordItem &item2) {
    // C#: SortingHelper.ByPopularAllTime:
    // int comp = -pmi1.Clicks.Count.CompareTo(pmi2.Clicks.Count);
    // if (comp == 0) return (pmi1.DisplayName ?? "").CompareTo(pmi2.DisplayName ?? "");
    const qsizetype c1 = item1.clicks.size();
    const qsizetype c2 = item2.clicks.size();
    if (c1 != c2) {
        return (c1 > c2) ? -1 : 1; // Higher click count comes first
    }
    return byDisplayName(item1, item2);
}

int SortingHelper::byPopularLastYear(const PasswordItem &item1, const PasswordItem &item2, const IClock *clock) {
    // In C# PasswordsPage.xaml.cs: "popular all time" is equal to "popular last year" because
    // ClearOldClicksCache prunes clicks > 365 days on load. If an explicit clock is supplied
    // (or unpruned items are passed), we filter by the 365-day window when clock != nullptr,
    // or use countPopularityLastYear when clicks contain dates and clock is provided.
    const int c1 = clock ? countPopularityLastYear(item1, clock) : static_cast<int>(item1.clicks.size());
    const int c2 = clock ? countPopularityLastYear(item2, clock) : static_cast<int>(item2.clicks.size());
    if (c1 != c2) {
        return (c1 > c2) ? -1 : 1;
    }
    return byDisplayName(item1, item2);
}

int SortingHelper::byPopularLast30Days(const PasswordItem &item1, const PasswordItem &item2, const IClock *clock) {
    // C#: SortingHelper.ByPopularLast30Days
    const int c1 = countPopularityLast30Days(item1, clock);
    const int c2 = countPopularityLast30Days(item2, clock);
    if (c1 != c2) {
        return (c1 > c2) ? -1 : 1;
    }
    return byDisplayName(item1, item2);
}

int SortingHelper::byPasswordStrength(const PasswordItem &item1, const PasswordItem &item2, int minLength) {
    // C#: SortingHelper.ByPasswordStrength:
    // int pmi1_res = 0, pmi2_res = 0;
    // for (int i = 0; i < strength1.Length; i++) { pmi1_res += strength1[i] ? 1 : -1; ... }
    // return -pmi1_res.CompareTo(pmi2_res);
    const int s1 = PasswordStrength::strengthScore(item1.password, minLength);
    const int s2 = PasswordStrength::strengthScore(item2.password, minLength);
    if (s1 != s2) {
        return (s1 > s2) ? -1 : 1; // Stronger password comes first
    }
    return 0;
}

int SortingHelper::compare(const PasswordItem &item1, const PasswordItem &item2, SortMode mode, SortOrder order,
                           const IClock *clock, int minPasswordLength) {
    int cmp = 0;
    switch (mode) {
    case SortMode::Name:
        cmp = byDisplayName(item1, item2);
        break;
    case SortMode::Username:
        cmp = byUsername(item1, item2);
        break;
    case SortMode::Notes:
        cmp = byNotes(item1, item2);
        break;
    case SortMode::FirstTag:
        cmp = byFirstTag(item1, item2);
        break;
    case SortMode::Website:
        cmp = byWebsite(item1, item2);
        break;
    case SortMode::PopularLastYear:
        cmp = byPopularLastYear(item1, item2, clock);
        break;
    case SortMode::PopularLast30Days:
        cmp = byPopularLast30Days(item1, item2, clock);
        break;
    case SortMode::PasswordStrength:
        cmp = byPasswordStrength(item1, item2, minPasswordLength);
        break;
    }

    if (order == SortOrder::Descending) {
        cmp = -cmp;
    }
    return cmp;
}

void SortingHelper::sort(QList<PasswordItem> &items, SortMode mode, SortOrder order, const IClock *clock,
                         int minPasswordLength) {
    std::stable_sort(items.begin(), items.end(), [&](const PasswordItem &a, const PasswordItem &b) {
        return compare(a, b, mode, order, clock, minPasswordLength) < 0;
    });
}

QList<PasswordItem> SortingHelper::sorted(const QList<PasswordItem> &items, SortMode mode, SortOrder order,
                                          const IClock *clock, int minPasswordLength) {
    QList<PasswordItem> copy = items;
    sort(copy, mode, order, clock, minPasswordLength);
    return copy;
}

void SortingHelper::clearOldClicksCache(QList<PasswordItem> &items, const IClock *clock) {
    // C#: DatabaseItem.ClearOldClicksCache:
    // if (DateTime.Now - date > TimeSpan.FromDays(365)) Items[i].Clicks.RemoveAt(j);
    const QDateTime now = currentDateTime(clock);
    const qint64 maxAgeSecs = 365LL * 86400LL;

    for (PasswordItem &item : items) {
        for (qsizetype j = 0; j < item.clicks.size(); ++j) {
            const std::optional<QDate> parsed = parseClickDate(item.clicks[j]);
            if (!parsed.has_value()) {
                continue;
            }
            const QDateTime clickMidnight(*parsed, QTime(0, 0, 0), QTimeZone::UTC);
            if (clickMidnight.secsTo(now) > maxAgeSecs) {
                item.clicks.removeAt(j);
                --j;
            }
        }
    }
}

void SortingHelper::clearOldClicksCache(DatabaseFile &db, const IClock *clock) {
    if (db.isReadOnly) {
        return;
    }
    clearOldClicksCache(db.items, clock);
}

void SortingHelper::resetPopularity(QList<PasswordItem> &items) {
    // C#: SettingsPage.ResetPopularity_Click
    for (PasswordItem &item : items) {
        item.clicks.clear();
    }
}

void SortingHelper::resetPopularity(DatabaseFile &db) {
    if (db.isReadOnly) {
        return;
    }
    resetPopularity(db.items);
}

bool SortingHelper::resetPopularity(DatabaseService &service) {
    if (!service.isOpen() || service.isReadOnly()) {
        return false;
    }
    QList<PasswordItem> updated = service.items();
    resetPopularity(updated);
    return service.setItems(updated);
}

} // namespace EasePass::Core
