#include "SearchPasswordsManager.h"

namespace EasePass::Core {

bool SearchPasswordsManager::matchesFreeText(const PasswordItem &item, const QString &query) {
    if (query.isEmpty()) {
        return true;
    }

    if (item.displayName.contains(query, Qt::CaseInsensitive) || item.username.contains(query, Qt::CaseInsensitive) ||
        item.email.contains(query, Qt::CaseInsensitive) || item.website.contains(query, Qt::CaseInsensitive) ||
        item.notes.contains(query, Qt::CaseInsensitive)) {
        return true;
    }

    for (const QString &tag : item.tags) {
        if (tag.contains(query, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

bool SearchPasswordsManager::matchesTag(const PasswordItem &item, const QString &tagQuery) {
    // C#: DatabaseItem.FindItemsByTag:
    // Items.Where(x => x.Tags != null && x.Tags.Any(t => t.Contains(tag, StringComparison.OrdinalIgnoreCase)))
    if (item.tags.isEmpty()) {
        return false;
    }
    if (tagQuery.isEmpty()) {
        return true; // Any item that has at least one tag matches "/" (because t.Contains("") is true)
    }
    for (const QString &tag : item.tags) {
        if (tag.contains(tagQuery, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

bool SearchPasswordsManager::matches(const PasswordItem &item, const QString &query) {
    if (query.isEmpty()) {
        return true;
    }
    if (query.startsWith(u'/')) {
        QString searchText = query;
        searchText.remove(u'/');
        return matchesTag(item, searchText);
    }
    return matchesFreeText(item, query);
}

QList<PasswordItem> SearchPasswordsManager::findItemsByName(const QList<PasswordItem> &items, const QString &name) {
    if (name.isEmpty()) {
        return items;
    }
    QList<PasswordItem> result;
    for (const PasswordItem &item : items) {
        if (item.displayName.contains(name, Qt::CaseInsensitive)) {
            result.append(item);
        }
    }
    return result;
}

QList<PasswordItem> SearchPasswordsManager::findItemsByTag(const QList<PasswordItem> &items, const QString &tag) {
    QList<PasswordItem> result;
    for (const PasswordItem &item : items) {
        if (matchesTag(item, tag)) {
            result.append(item);
        }
    }
    return result;
}

QList<PasswordItem> SearchPasswordsManager::findItemsByFreeText(const QList<PasswordItem> &items,
                                                                const QString &query) {
    if (query.isEmpty()) {
        return items;
    }
    QList<PasswordItem> result;
    for (const PasswordItem &item : items) {
        if (matchesFreeText(item, query)) {
            result.append(item);
        }
    }
    return result;
}

QList<PasswordItem> SearchPasswordsManager::filterItems(const QList<PasswordItem> &items, const QString &text) {
    if (text.isEmpty()) {
        return items;
    }
    if (text.startsWith(u'/')) {
        QString searchText = text;
        searchText.remove(u'/');
        return findItemsByTag(items, searchText);
    }
    return findItemsByFreeText(items, text);
}

SearchPasswordsManager::SearchResult
SearchPasswordsManager::searchPasswords(const QList<PasswordItem> &items, const QString &text, bool isUserTextChange) {
    // C#: Helper/SearchPasswordsManager.cs
    SearchResult res;

    if (!isUserTextChange) {
        if (text.startsWith(u'/')) {
            res.isTagSearch = true;
            QString searchText = text;
            searchText.remove(u'/');
            res.items = findItemsByTag(items, searchText);
            res.infoLabel = QString::number(res.items.size());
        } else {
            res.items = filterItems(items, text);
            res.infoLabel = QString::number(res.items.size());
        }
        return res;
    }

    if (text.isEmpty()) {
        res.items = items;
        res.infoLabel = QString::number(items.size());
        return res;
    }

    if (text.startsWith(u'/')) {
        res.isTagSearch = true;
        if (m_tagSearchManager.uniqueTagList().isEmpty()) {
            m_tagSearchManager.updateTagList(items);
        }

        QString searchText = text;
        searchText.remove(u'/');
        res.suggestions = m_tagSearchManager.filter(searchText, QStringLiteral("/"));
        if (res.suggestions.isEmpty()) {
            res.suggestions.append(QStringLiteral("No results found"));
        }

        res.items = findItemsByTag(items, searchText);
        res.infoLabel = QString::number(res.items.size());
        return res;
    }

    res.items = findItemsByFreeText(items, text);
    res.infoLabel = QString::number(res.items.size());
    return res;
}

} // namespace EasePass::Core
