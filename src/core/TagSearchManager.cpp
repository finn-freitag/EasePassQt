#include "TagSearchManager.h"
#include "SortingHelper.h"

#include <QSet>
#include <algorithm>

namespace EasePass::Core {

void TagSearchManager::updateTagList(const QList<PasswordItem> &items) {
    // C#: TagSearchManager.IndexItems + UpdateTagList (seen HashSet + Sort())
    QSet<QString> seen;
    QStringList tags;
    for (const PasswordItem &item : items) {
        for (const QString &tag : item.tags) {
            if (!tag.isEmpty() && !seen.contains(tag)) {
                seen.insert(tag);
                tags.append(tag);
            }
        }
    }
    std::sort(tags.begin(), tags.end(),
              [](const QString &a, const QString &b) { return SortingHelper::compareStrings(a, b) < 0; });
    m_uniqueTagList = std::move(tags);
}

QStringList TagSearchManager::filter(const QString &searchKeyword, const QString &prefix) const {
    // C#: TagSearchManager.Filter(searchKW, prefix)
    QStringList result;
    if (searchKeyword.isEmpty()) {
        result.reserve(m_uniqueTagList.size());
        for (const QString &tag : m_uniqueTagList) {
            result.append(prefix + tag);
        }
        return result;
    }

    for (const QString &tag : m_uniqueTagList) {
        if (tag.contains(searchKeyword, Qt::CaseInsensitive)) {
            result.append(prefix + tag);
        }
    }
    return result;
}

} // namespace EasePass::Core
