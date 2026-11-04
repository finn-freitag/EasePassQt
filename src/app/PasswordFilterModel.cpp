#include "PasswordFilterModel.h"

#include "DatabaseService.h"
#include "PasswordListModel.h"
#include "SortingHelper.h"

namespace EasePass::App {

PasswordFilterModel::PasswordFilterModel(PasswordListModel *source, const Core::IClock *clock, QObject *parent)
    : QSortFilterProxyModel(parent), m_list(source), m_clock(clock) {
    setSourceModel(source);
    setDynamicSortFilter(true);
    connect(this, &QAbstractItemModel::modelReset, this, &PasswordFilterModel::countChanged);
    connect(this, &QAbstractItemModel::layoutChanged, this, &PasswordFilterModel::countChanged);
    connect(this, &QAbstractItemModel::rowsInserted, this, &PasswordFilterModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &PasswordFilterModel::countChanged);
    // New items -> refresh the tag list used for "/tag" suggestions.
    connect(source, &QAbstractItemModel::modelReset, this, [this] {
        m_search.updateTagList(m_list->service()->isOpen() ? m_list->service()->items() : QList<Core::PasswordItem>());
    });
}

void PasswordFilterModel::setFilterText(const QString &text) { applyFilterText(text, true); }

void PasswordFilterModel::setFilterTextProgrammatically(const QString &text) { applyFilterText(text, false); }

void PasswordFilterModel::applyFilterText(const QString &text, bool isUserChange) {
    if (text == m_filterText)
        return;
    beginFilterChange();
    m_filterText = text;
    if (isUserChange && isTagSearch() && m_list->service()->isOpen()) {
        setSuggestions(m_search.searchPasswords(m_list->service()->items(), text, true).suggestions);
    } else {
        setSuggestions({});
    }
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    emit filterTextChanged();
    emit countChanged();
}

void PasswordFilterModel::setSuggestions(const QStringList &s) {
    if (s == m_suggestions)
        return;
    m_suggestions = s;
    emit suggestionsChanged();
}

void PasswordFilterModel::setSortMode(SortMode mode) {
    if (mode == m_sortMode)
        return;
    m_sortMode = mode;
    applySort();
    emit sortChanged();
}

void PasswordFilterModel::setDescending(bool descending) {
    if (descending == m_descending)
        return;
    m_descending = descending;
    applySort();
    emit sortChanged();
}

void PasswordFilterModel::applySort() {
    sort(-1); // back to source order, also forces a re-sort below
    if (m_sortMode != SortMode::None)
        sort(0, m_descending ? Qt::DescendingOrder : Qt::AscendingOrder);
}

bool PasswordFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &) const {
    const Core::PasswordItem *item = m_list->itemAt(sourceRow);
    return item && Core::SearchPasswordsManager::matches(*item, m_filterText);
}

bool PasswordFilterModel::lessThan(const QModelIndex &left, const QModelIndex &right) const {
    const Core::PasswordItem *a = m_list->itemAt(left.row());
    const Core::PasswordItem *b = m_list->itemAt(right.row());
    if (!a || !b || m_sortMode == SortMode::None)
        return left.row() < right.row();
    // Qt applies the order itself, so always compare ascending here.
    const int c = Core::SortingHelper::compare(*a, *b, static_cast<Core::SortMode>(m_sortMode),
                                               Core::SortOrder::Ascending, m_clock);
    return c != 0 ? c < 0 : left.row() < right.row();
}

int PasswordFilterModel::sourceIndex(int row) const {
    const QModelIndex src = mapToSource(index(row, 0));
    return src.isValid() ? src.row() : -1;
}

int PasswordFilterModel::proxyRow(int sourceIndex) const {
    const QModelIndex p = mapFromSource(m_list->index(sourceIndex, 0));
    return p.isValid() ? p.row() : -1;
}

} // namespace EasePass::App
