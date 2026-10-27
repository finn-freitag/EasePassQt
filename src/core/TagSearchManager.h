#pragma once

#include "DatabaseFile.h"
#include "DatabaseService.h"
#include "PasswordItem.h"

#include <QStringList>

namespace EasePass::Core {

/**
 * @brief Indexes unique tags from a database and provides tag suggestions for `/` search syntax.
 *
 * C#: Helper/TagSearchManager.cs.
 */
class TagSearchManager {
public:
    TagSearchManager() = default;

    const QStringList &uniqueTagList() const { return m_uniqueTagList; }

    /// C#: TagSearchManager.UpdateTagList
    void updateTagList(const QList<PasswordItem> &items);
    void updateTagList(const DatabaseFile &db) { updateTagList(db.items); }
    void updateTagList(const DatabaseService &service) { updateTagList(service.items()); }

    void clear() { m_uniqueTagList.clear(); }

    /// C#: TagSearchManager.Filter(searchKW, prefix)
    /// Filters unique tags case-insensitively by `searchKeyword` and prepends `prefix` (typically `"/""`).
    QStringList filter(const QString &searchKeyword, const QString &prefix = QStringLiteral("/")) const;

private:
    QStringList m_uniqueTagList;
};

} // namespace EasePass::Core
