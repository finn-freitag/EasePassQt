#include "SearchPasswordsManager.h"

#include "SortingHelper.h"

#include <QSet>
#include <algorithm>

namespace EasePass::Core {

bool SearchPasswordsManager::matchesFreeText(const PasswordItem &item, const QString &query) {
    if (item.displayName.contains(query, Qt::CaseInsensitive) || item.username.contains(query, Qt::CaseInsensitive) ||
        item.email.contains(query, Qt::CaseInsensitive) || item.website.contains(query, Qt::CaseInsensitive) ||
        item.notes.contains(query, Qt::CaseInsensitive)) {
        return true;
    }
    return std::any_of(item.tags.cbegin(), item.tags.cend(),
                       [&](const QString &tag) { return tag.contains(query, Qt::CaseInsensitive); });
}

bool SearchPasswordsManager::matchesTag(const PasswordItem &item, const QString &tagQuery) {
    // An empty tag query ("/") matches every item that has a tag.
    return std::any_of(item.tags.cbegin(), item.tags.cend(),
                       [&](const QString &tag) { return tag.contains(tagQuery, Qt::CaseInsensitive); });
}

bool SearchPasswordsManager::matches(const PasswordItem &item, const QString &query) {
    if (query.isEmpty()) {
        return true;
    }
    return isTagQuery(query) ? matchesTag(item, query.mid(1)) : matchesFreeText(item, query);
}

QStringList SearchPasswordsManager::tagSuggestions(const QList<PasswordItem> &items, const QString &query) {
    if (!isTagQuery(query)) {
        return {};
    }
    const QString keyword = query.mid(1);
    QSet<QString> seen;
    QStringList tags;
    for (const PasswordItem &item : items) {
        for (const QString &tag : item.tags) {
            if (!tag.isEmpty() && !seen.contains(tag) && tag.contains(keyword, Qt::CaseInsensitive)) {
                seen.insert(tag);
                tags.append(tag);
            }
        }
    }
    std::sort(tags.begin(), tags.end(),
              [](const QString &a, const QString &b) { return SortingHelper::compareStrings(a, b) < 0; });
    for (QString &tag : tags) {
        tag.prepend(u'/');
    }
    return tags;
}

} // namespace EasePass::Core
