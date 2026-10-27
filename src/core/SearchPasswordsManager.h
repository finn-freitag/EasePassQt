#pragma once

#include "DatabaseFile.h"
#include "DatabaseService.h"
#include "PasswordItem.h"
#include "TagSearchManager.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace EasePass::Core {

/**
 * @brief Searches password items by free text or tag syntax (`/tag`), and builds tag suggestions.
 *
 * C#: Helper/SearchPasswordsManager.cs, Core/Database/DatabaseItem.cs (FindItemsByName, FindItemsByTag).
 *
 * Per T5 specification:
 *  - Free text searches over name (`displayName`), username, email, website, notes, and tags
 *    (case-insensitive). Also exposes `findItemsByName` for strict C# `DatabaseItem.FindItemsByName`
 *    compatibility.
 *  - Tag filter syntax: queries starting with `/` (e.g. `"/work"`) strip `/` and match only items
 *    whose tags contain the query substring case-insensitively, while populating `suggestions` via
 *    `TagSearchManager::filter` (or `"No results found"` when no tag matches, mirroring C#).
 */
class SearchPasswordsManager {
public:
    struct SearchResult {
        QList<PasswordItem> items;
        QStringList suggestions; ///< Tag suggestions when `isTagSearch` is true (with `/` prefix)
        QString infoLabel;       ///< String representation of matched item count (C#: searchbox.InfoLabel)
        bool isTagSearch = false;
    };

    SearchPasswordsManager() = default;

    TagSearchManager &tagSearchManager() { return m_tagSearchManager; }
    const TagSearchManager &tagSearchManager() const { return m_tagSearchManager; }

    /// Updates the internal `TagSearchManager` unique tag index from @p items.
    void updateTagList(const QList<PasswordItem> &items) { m_tagSearchManager.updateTagList(items); }
    void updateTagList(const DatabaseFile &db) { m_tagSearchManager.updateTagList(db); }
    void updateTagList(const DatabaseService &service) { m_tagSearchManager.updateTagList(service); }

    /// Returns true if @p item matches free-text @p query across name/username/email/website/notes/tags.
    static bool matchesFreeText(const PasswordItem &item, const QString &query);

    /// Returns true if @p item has at least one tag containing @p tagQuery case-insensitively.
    /// C#: DatabaseItem.FindItemsByTag
    static bool matchesTag(const PasswordItem &item, const QString &tagQuery);

    /// Returns true if @p item matches @p query (uses tag filter if `query.startsWith('/')`, else free text).
    static bool matches(const PasswordItem &item, const QString &query);

    /// C#: DatabaseItem.FindItemsByName (displayName only, case-insensitive)
    static QList<PasswordItem> findItemsByName(const QList<PasswordItem> &items, const QString &name);

    /// C#: DatabaseItem.FindItemsByTag (tags only, case-insensitive)
    static QList<PasswordItem> findItemsByTag(const QList<PasswordItem> &items, const QString &tag);

    /// Free-text search over name, username, email, website, notes, and tags.
    static QList<PasswordItem> findItemsByFreeText(const QList<PasswordItem> &items, const QString &query);

    /// Filters @p items using `/` tag syntax if `text.startsWith('/')`, otherwise free-text search.
    static QList<PasswordItem> filterItems(const QList<PasswordItem> &items, const QString &text);

    /// Full C# SearchPasswordsManager.SearchPasswords mirror: returns matched items, tag suggestions,
    /// and infoLabel count. Automatically indexes tags from @p items if not already indexed.
    SearchResult searchPasswords(const QList<PasswordItem> &items, const QString &text, bool isUserTextChange = true);
    SearchResult searchPasswords(const DatabaseFile &db, const QString &text, bool isUserTextChange = true) {
        return searchPasswords(db.items, text, isUserTextChange);
    }
    SearchResult searchPasswords(const DatabaseService &service, const QString &text, bool isUserTextChange = true) {
        return searchPasswords(service.items(), text, isUserTextChange);
    }

private:
    TagSearchManager m_tagSearchManager;
};

} // namespace EasePass::Core
