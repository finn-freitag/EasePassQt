#pragma once

#include <QWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QLabel>
#include <QSplitter>
#include "PasswordViewerWidget.h"
#include "PasswordItem.h"

namespace EasePass::UI {

class PasswordsWidget : public QWidget {
    Q_OBJECT
public:
    explicit PasswordsWidget(QWidget* parent = nullptr);

    void refreshList();

signals:
    void logoutRequested();
    void aboutRequested();

private slots:
    void onNewEntry();
    void onEditEntry(int originalIndex);
    void onDeleteEntry(int originalIndex);
    void onItemModified(int originalIndex, const Core::PasswordItem& item);
    void onSearchTextChanged(const QString& text);
    void onListSelectionChanged();
    void onSortChanged(int sortType);
    void onIconLoaded(const QString& domain);

private:
    void setupUi();
    void saveDatabase();
    int getOriginalIndexFromItem(QListWidgetItem* item) const;

    enum SortOption {
        SortNameAsc,
        SortNameDesc,
        SortUsername,
        SortWebsite,
        SortMostUsed
    };

    SortOption m_currentSort = SortNameAsc;
    QString m_filterText;

    QSplitter* m_splitter = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QPushButton* m_newEntryBtn = nullptr;
    QToolButton* m_sortBtn = nullptr;
    QListWidget* m_listWidget = nullptr;
    QLabel* m_dbStatusLabel = nullptr;
    QPushButton* m_logoutBtn = nullptr;
    QPushButton* m_aboutBtn = nullptr;

    PasswordViewerWidget* m_viewer = nullptr;
};

} // namespace EasePass::UI
