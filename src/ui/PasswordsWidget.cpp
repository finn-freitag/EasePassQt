#include "PasswordsWidget.h"
#include "DatabaseManager.h"
#include "EditItemDialog.h"
#include "WebsiteIconManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenu>
#include <QMessageBox>
#include <QFileInfo>
#include <QPainter>
#include <algorithm>

namespace EasePass::UI {

namespace {

QWidget* createListItemWidget(const Core::PasswordItem& item) {
    auto* widget = new QWidget();
    auto* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(10);

    // Icon / Avatar
    auto* iconLabel = new QLabel(widget);
    iconLabel->setFixedSize(32, 32);

    QPixmap icon = Core::WebsiteIconManager::instance().getIcon(item.website, 28);
    if (!icon.isNull()) {
        iconLabel->setPixmap(icon);
    } else {
        QPixmap pix(32, 32);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(item.avatarColor());
        p.setPen(Qt::NoPen);
        p.drawEllipse(1, 1, 30, 30);
        p.setPen(item.avatarTextColor());
        QFont f("sans-serif", 13, QFont::Bold);
        p.setFont(f);
        p.drawText(pix.rect(), Qt::AlignCenter, item.avatarLetter());
        p.end();
        iconLabel->setPixmap(pix);
    }
    layout->addWidget(iconLabel);

    // Text column
    auto* col = new QVBoxLayout();
    col->setSpacing(2);

    auto* nameLabel = new QLabel(item.displayName.isEmpty() ? "(Unnamed)" : item.displayName, widget);
    nameLabel->setFont(QFont("sans-serif", 11, QFont::DemiBold));

    QString sub = item.username;
    if (sub.isEmpty()) sub = item.email;
    if (sub.isEmpty()) sub = item.website;
    auto* subLabel = new QLabel(sub, widget);
    subLabel->setStyleSheet("color: gray; font-size: 10px;");

    col->addWidget(nameLabel);
    col->addWidget(subLabel);
    layout->addLayout(col);

    layout->addStretch();

    // Badges: 2FA & Tag count
    if (item.has2FA()) {
        auto* badge = new QLabel("2FA", widget);
        badge->setStyleSheet("QLabel { background: #28a745; color: white; border-radius: 4px; padding: 1px 5px; font-size: 9px; font-weight: bold; }");
        layout->addWidget(badge);
    }

    if (!item.tags.isEmpty()) {
        auto* tagBadge = new QLabel(item.tags.first(), widget);
        tagBadge->setStyleSheet("QLabel { background: palette(alternate-base); color: gray; border: 1px solid gray; border-radius: 4px; padding: 1px 4px; font-size: 9px; }");
        layout->addWidget(tagBadge);
    }

    return widget;
}

} // namespace

PasswordsWidget::PasswordsWidget(QWidget* parent)
    : QWidget(parent) {
    setupUi();

    connect(&Core::WebsiteIconManager::instance(), &Core::WebsiteIconManager::iconLoaded,
            this, &PasswordsWidget::onIconLoaded);
}

void PasswordsWidget::setupUi() {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setChildrenCollapsible(false);

    // LEFT PANE
    auto* leftPane = new QWidget(m_splitter);
    leftPane->setMinimumWidth(260);
    leftPane->setMaximumWidth(450);
    auto* leftLayout = new QVBoxLayout(leftPane);
    leftLayout->setContentsMargins(12, 12, 12, 12);
    leftLayout->setSpacing(10);

    // Top action row: New Entry, Search, Sort
    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(6);

    m_newEntryBtn = new QPushButton("+ New", leftPane);
    m_newEntryBtn->setStyleSheet("QPushButton { font-weight: bold; padding: 6px 12px; }");
    connect(m_newEntryBtn, &QPushButton::clicked, this, &PasswordsWidget::onNewEntry);
    topRow->addWidget(m_newEntryBtn);

    m_searchEdit = new QLineEdit(leftPane);
    m_searchEdit->setPlaceholderText("Search entries...");
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &PasswordsWidget::onSearchTextChanged);
    topRow->addWidget(m_searchEdit);

    m_sortBtn = new QToolButton(leftPane);
    m_sortBtn->setText("⇅");
    m_sortBtn->setToolTip("Sort entries");
    m_sortBtn->setPopupMode(QToolButton::InstantPopup);

    auto* sortMenu = new QMenu(m_sortBtn);
    auto* a1 = sortMenu->addAction("Name (A-Z)");
    auto* a2 = sortMenu->addAction("Name (Z-A)");
    auto* a3 = sortMenu->addAction("Username");
    auto* a4 = sortMenu->addAction("Website");
    auto* a5 = sortMenu->addAction("Most Clicked");

    connect(a1, &QAction::triggered, this, [this]() { onSortChanged(SortNameAsc); });
    connect(a2, &QAction::triggered, this, [this]() { onSortChanged(SortNameDesc); });
    connect(a3, &QAction::triggered, this, [this]() { onSortChanged(SortUsername); });
    connect(a4, &QAction::triggered, this, [this]() { onSortChanged(SortWebsite); });
    connect(a5, &QAction::triggered, this, [this]() { onSortChanged(SortMostUsed); });

    m_sortBtn->setMenu(sortMenu);
    topRow->addWidget(m_sortBtn);

    leftLayout->addLayout(topRow);

    // List of passwords
    m_listWidget = new QListWidget(leftPane);
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listWidget->setFrameShape(QFrame::StyledPanel);
    connect(m_listWidget, &QListWidget::itemSelectionChanged, this, &PasswordsWidget::onListSelectionChanged);
    leftLayout->addWidget(m_listWidget);

    // Bottom status bar
    auto* bottomRow = new QHBoxLayout();
    bottomRow->setContentsMargins(0, 4, 0, 0);

    m_dbStatusLabel = new QLabel(leftPane);
    m_dbStatusLabel->setStyleSheet("color: gray; font-size: 11px;");
    bottomRow->addWidget(m_dbStatusLabel);
    bottomRow->addStretch();

    m_aboutBtn = new QPushButton("About", leftPane);
    connect(m_aboutBtn, &QPushButton::clicked, this, &PasswordsWidget::aboutRequested);
    bottomRow->addWidget(m_aboutBtn);

    m_logoutBtn = new QPushButton("Lock", leftPane);
    m_logoutBtn->setToolTip("Lock database / Logout");
    connect(m_logoutBtn, &QPushButton::clicked, this, &PasswordsWidget::logoutRequested);
    bottomRow->addWidget(m_logoutBtn);

    leftLayout->addLayout(bottomRow);

    m_splitter->addWidget(leftPane);

    // RIGHT PANE (Viewer)
    m_viewer = new PasswordViewerWidget(m_splitter);
    connect(m_viewer, &PasswordViewerWidget::editRequested, this, &PasswordsWidget::onEditEntry);
    connect(m_viewer, &PasswordViewerWidget::deleteRequested, this, &PasswordsWidget::onDeleteEntry);
    connect(m_viewer, &PasswordViewerWidget::itemModified, this, &PasswordsWidget::onItemModified);
    m_splitter->addWidget(m_viewer);

    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({320, 680});

    rootLayout->addWidget(m_splitter);
}

void PasswordsWidget::refreshList() {
    auto* db = Core::DatabaseManager::instance().currentDatabase();
    if (!db) {
        m_listWidget->clear();
        m_viewer->clear();
        m_dbStatusLabel->setText("No database loaded");
        return;
    }

    QFileInfo fi(db->filePath);
    m_dbStatusLabel->setText(QString("%1 (%2 entries)").arg(fi.fileName()).arg(db->items.size()));

    struct ItemEntry {
        int originalIndex;
        Core::PasswordItem item;
    };

    QList<ItemEntry> visibleEntries;
    QString filter = m_filterText.trimmed();

    for (int i = 0; i < db->items.size(); ++i) {
        const auto& it = db->items[i];
        if (!filter.isEmpty()) {
            bool match = it.displayName.contains(filter, Qt::CaseInsensitive) ||
                         it.username.contains(filter, Qt::CaseInsensitive) ||
                         it.email.contains(filter, Qt::CaseInsensitive) ||
                         it.website.contains(filter, Qt::CaseInsensitive) ||
                         it.notes.contains(filter, Qt::CaseInsensitive);

            if (!match) {
                for (const auto& tag : it.tags) {
                    if (tag.contains(filter, Qt::CaseInsensitive)) {
                        match = true;
                        break;
                    }
                }
            }
            if (!match) continue;
        }

        visibleEntries.append({i, it});
    }

    // Sort
    std::sort(visibleEntries.begin(), visibleEntries.end(), [this](const ItemEntry& a, const ItemEntry& b) {
        switch (m_currentSort) {
            case SortNameDesc:
                return a.item.displayName.localeAwareCompare(b.item.displayName) > 0;
            case SortUsername:
                return a.item.username.localeAwareCompare(b.item.username) < 0;
            case SortWebsite:
                return a.item.website.localeAwareCompare(b.item.website) < 0;
            case SortMostUsed:
                return a.item.clicks.size() > b.item.clicks.size();
            case SortNameAsc:
            default:
                return a.item.displayName.localeAwareCompare(b.item.displayName) < 0;
        }
    });

    int prevSelectedOrigIdx = -1;
    if (m_listWidget->currentItem()) {
        prevSelectedOrigIdx = getOriginalIndexFromItem(m_listWidget->currentItem());
    }

    m_listWidget->clear();

    QListWidgetItem* itemToSelect = nullptr;
    for (const auto& entry : visibleEntries) {
        auto* listItem = new QListWidgetItem(m_listWidget);
        listItem->setData(Qt::UserRole, entry.originalIndex);
        listItem->setSizeHint(QSize(200, 48));

        auto* widget = createListItemWidget(entry.item);
        m_listWidget->setItemWidget(listItem, widget);

        if (entry.originalIndex == prevSelectedOrigIdx) {
            itemToSelect = listItem;
        }
    }

    if (itemToSelect) {
        m_listWidget->setCurrentItem(itemToSelect);
    } else if (m_listWidget->count() > 0) {
        m_listWidget->setCurrentRow(0);
    } else {
        m_viewer->clear();
    }
}

int PasswordsWidget::getOriginalIndexFromItem(QListWidgetItem* item) const {
    if (!item) return -1;
    return item->data(Qt::UserRole).toInt();
}

void PasswordsWidget::onListSelectionChanged() {
    auto* item = m_listWidget->currentItem();
    if (!item) {
        m_viewer->clear();
        return;
    }

    int origIdx = getOriginalIndexFromItem(item);
    auto* db = Core::DatabaseManager::instance().currentDatabase();
    if (db && origIdx >= 0 && origIdx < db->items.size()) {
        db->items[origIdx].registerClick();
        m_viewer->setItem(db->items[origIdx], origIdx);
    } else {
        m_viewer->clear();
    }
}

void PasswordsWidget::onSearchTextChanged(const QString& text) {
    m_filterText = text;
    refreshList();
}

void PasswordsWidget::onSortChanged(int sortType) {
    m_currentSort = static_cast<SortOption>(sortType);
    refreshList();
}

void PasswordsWidget::onNewEntry() {
    auto* db = Core::DatabaseManager::instance().currentDatabase();
    if (!db) return;

    EditItemDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        Core::PasswordItem newItem = dlg.getItem();
        db->addItem(newItem);
        saveDatabase();
        refreshList();

        // Select the newly added item
        int newIdx = db->items.size() - 1;
        for (int i = 0; i < m_listWidget->count(); ++i) {
            if (getOriginalIndexFromItem(m_listWidget->item(i)) == newIdx) {
                m_listWidget->setCurrentRow(i);
                break;
            }
        }
    }
}

void PasswordsWidget::onEditEntry(int originalIndex) {
    auto* db = Core::DatabaseManager::instance().currentDatabase();
    if (!db || originalIndex < 0 || originalIndex >= db->items.size()) return;

    EditItemDialog dlg(db->items[originalIndex], this);
    if (dlg.exec() == QDialog::Accepted) {
        db->updateItem(originalIndex, dlg.getItem());
        saveDatabase();
        refreshList();
    }
}

void PasswordsWidget::onDeleteEntry(int originalIndex) {
    auto* db = Core::DatabaseManager::instance().currentDatabase();
    if (!db || originalIndex < 0 || originalIndex >= db->items.size()) return;

    QString name = db->items[originalIndex].displayName;
    auto res = QMessageBox::question(this, "Delete Entry",
                                     QString("Are you sure you want to delete \"%1\"?").arg(name),
                                     QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (res == QMessageBox::Yes) {
        db->deleteItem(originalIndex);
        saveDatabase();
        refreshList();
    }
}

void PasswordsWidget::onItemModified(int originalIndex, const Core::PasswordItem& item) {
    auto* db = Core::DatabaseManager::instance().currentDatabase();
    if (!db || originalIndex < 0 || originalIndex >= db->items.size()) return;

    db->updateItem(originalIndex, item);
    saveDatabase();
    refreshList();
}

void PasswordsWidget::saveDatabase() {
    auto* db = Core::DatabaseManager::instance().currentDatabase();
    if (!db) return;

    if (!db->saveToFile()) {
        QMessageBox::critical(this, "Save Failed", "Could not save database to file!");
    }
}

void PasswordsWidget::onIconLoaded(const QString&) {
    refreshList();
}

} // namespace EasePass::UI
