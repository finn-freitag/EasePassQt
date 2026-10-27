#include "EmailAddressIndexer.h"

#include <QSet>

namespace EasePass::Core {

void EmailAddressIndexer::indexUniqueEmail(const QList<PasswordItem> &items) {
    // C#: EmailAddressIndexer.IndexUniqueEmail (HashSet<string>(StringComparer.OrdinalIgnoreCase))
    QSet<QString> seenLower;
    for (const QString &existing : m_uniqueEmailAddresses) {
        seenLower.insert(existing.toCaseFolded());
    }

    for (const PasswordItem &item : items) {
        const QString trimmed = item.email.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        const QString key = trimmed.toCaseFolded();
        if (!seenLower.contains(key)) {
            seenLower.insert(key);
            m_uniqueEmailAddresses.append(trimmed);
        }
    }
}

QStringList EmailAddressIndexer::suggestions(const QString &query) const {
    const QString searchKw = query.trimmed();
    if (searchKw.isEmpty()) {
        return {};
    }

    QStringList matches;
    for (const QString &email : m_uniqueEmailAddresses) {
        if (email.contains(searchKw, Qt::CaseInsensitive)) {
            matches.append(email);
        }
    }
    return matches;
}

} // namespace EasePass::Core
