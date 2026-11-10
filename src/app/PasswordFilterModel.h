#pragma once

#include "SearchPasswordsManager.h"

#include <QSortFilterProxyModel>
#include <QStringList>

namespace EasePass::Core { class IClock; }

namespace EasePass::App {

class PasswordListModel;

/**
 * Search + sort proxy over PasswordListModel using the Core (T5) logic:
 * SearchPasswordsManager::matches for filtering ("/tag" = tag search, as in C#) and
 * SortingHelper::compare for sorting. sortMode None keeps the database order.
 * C#: Helper/SearchPasswordsManager.cs, Helper/SortingHelper.cs, Views/PasswordsPage.
 */
class PasswordFilterModel : public QSortFilterProxyModel {
    Q_OBJECT
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(SortMode sortMode READ sortMode WRITE setSortMode NOTIFY sortChanged)
    Q_PROPERTY(bool descending READ descending WRITE setDescending NOTIFY sortChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString infoLabel READ infoLabel NOTIFY countChanged)
    Q_PROPERTY(bool isTagSearch READ isTagSearch NOTIFY filterTextChanged)
    Q_PROPERTY(QStringList suggestions READ suggestions NOTIFY suggestionsChanged)
public:
    // Mirrors Core::SortMode, plus None (database order).
    enum class SortMode {
        None = -1,
        Name = 0,
        Username,
        Notes,
        FirstTag,
        Website,
        PopularLastYear,
        PopularLast30Days,
        PasswordStrength
    };
    Q_ENUM(SortMode)

    explicit PasswordFilterModel(PasswordListModel *source, const Core::IClock *clock = nullptr,
                                 QObject *parent = nullptr);

    QString filterText() const { return m_filterText; }
    void setFilterText(const QString &text);
    /// Sets the text without computing tag suggestions (C#: isUserTextChange == false).
    Q_INVOKABLE void setFilterTextProgrammatically(const QString &text);
    SortMode sortMode() const { return m_sortMode; }
    void setSortMode(SortMode mode);
    bool descending() const { return m_descending; }
    void setDescending(bool descending);
    int count() const { return rowCount(); }
    QString infoLabel() const { return QString::number(rowCount()); }
    bool isTagSearch() const { return m_filterText.startsWith(u'/'); }
    QStringList suggestions() const { return m_suggestions; }

    /// Database index of the item in proxy row @p row (-1 if out of range).
    Q_INVOKABLE int sourceIndex(int row) const;
    /// Proxy row of database index @p sourceIndex (-1 if filtered out).
    Q_INVOKABLE int proxyRow(int sourceIndex) const;

signals:
    void filterTextChanged();
    void sortChanged();
    void countChanged();
    void suggestionsChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    void applyFilterText(const QString &text, bool isUserChange);
    void applySort();
    void setSuggestions(const QStringList &s);
    void scheduleCountChanged();

    PasswordListModel *m_list;
    const Core::IClock *m_clock;
    Core::SearchPasswordsManager m_search;
    QString m_filterText;
    QStringList m_suggestions;
    SortMode m_sortMode = SortMode::None;
    bool m_descending = false;
    bool m_countChangePending = false;
};

} // namespace EasePass::App
