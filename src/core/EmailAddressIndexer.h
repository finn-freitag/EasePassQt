#pragma once

#include "DatabaseFile.h"
#include "DatabaseService.h"
#include "PasswordItem.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace EasePass::Core {

/**
 * @brief Indexes unique email addresses in a database (case-insensitively deduplicated,
 *        preserving original casing) and filters suggestions for add/edit item dialogs.
 *
 * C#: Manager/EmailAddressIndexer.cs, Controls/TextWithSuggestionBox.xaml.cs.
 */
class EmailAddressIndexer {
public:
    EmailAddressIndexer() = default;

    const QStringList &uniqueEmailAddresses() const { return m_uniqueEmailAddresses; }

    /// C#: EmailAddressIndexer.IndexUniqueEmail
    void indexUniqueEmail(const QList<PasswordItem> &items);
    void indexUniqueEmail(const DatabaseFile &db) { indexUniqueEmail(db.items); }
    void indexUniqueEmail(const DatabaseService &service) { indexUniqueEmail(service.items()); }

    void clear() { m_uniqueEmailAddresses.clear(); }

    /// C#: TextWithSuggestionBox.UpdateSuggestions: filters indexed emails case-insensitively
    /// by `query.trimmed()`. Returns empty list if `query` is empty/whitespace.
    QStringList suggestions(const QString &query) const;

private:
    QStringList m_uniqueEmailAddresses;
};

} // namespace EasePass::Core
