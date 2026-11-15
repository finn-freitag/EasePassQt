#pragma once

#include "PasswordItem.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace EasePass::Core {

/**
 * Search over password items.
 *
 *  - Free text matches name, username, email, website, notes and tags (case-insensitive).
 *  - A query that starts with '/' is a tag search: "/work" matches only items with a tag
 *    containing "work"; "/" alone matches every item that has a tag.
 */
class SearchPasswordsManager {
public:
    static bool isTagQuery(const QString &query) { return query.startsWith(u'/'); }

    /// true if @p item matches @p query (an empty query matches everything).
    static bool matches(const PasswordItem &item, const QString &query);

    /// Suggestions for a tag query: the tags of @p items that contain the text after '/', each
    /// once, sorted, with the '/' prefix. Empty for a free-text query or when no tag matches.
    static QStringList tagSuggestions(const QList<PasswordItem> &items, const QString &query);

private:
    static bool matchesFreeText(const PasswordItem &item, const QString &query);
    static bool matchesTag(const PasswordItem &item, const QString &tagQuery);
};

} // namespace EasePass::Core
