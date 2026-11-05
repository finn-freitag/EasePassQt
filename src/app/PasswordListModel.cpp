#include "PasswordListModel.h"

#include "DatabaseService.h"

namespace EasePass::App {

PasswordListModel::PasswordListModel(Core::DatabaseService *service, QObject *parent)
    : QAbstractListModel(parent), m_service(service) {
    connect(m_service, &Core::DatabaseService::itemsChanged, this, &PasswordListModel::onItemsChanged);
    connect(m_service, &Core::DatabaseService::opened, this, &PasswordListModel::reset);
    connect(m_service, &Core::DatabaseService::closed, this, &PasswordListModel::reset);
}

int PasswordListModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid() || !m_service->isOpen())
        return 0;
    return m_service->itemCount();
}

const Core::PasswordItem *PasswordListModel::itemAt(int row) const {
    if (row < 0 || row >= rowCount())
        return nullptr;
    return &m_service->items().at(row);
}

QVariant PasswordListModel::data(const QModelIndex &index, int role) const {
    const Core::PasswordItem *item = index.isValid() ? itemAt(index.row()) : nullptr;
    if (!item)
        return {};
    switch (role) {
    case Qt::DisplayRole:
    case DisplayNameRole: return item->displayName;
    case UsernameRole: return item->username;
    case EmailRole: return item->email;
    case WebsiteRole: return item->website;
    case NotesRole: return item->notes;
    case TagsRole: return item->tags;
    case Has2FARole: return item->has2FA();
    case AvatarLetterRole: return item->avatarLetter();
    case AvatarColorRole: return item->avatarColor();
    case AvatarTextColorRole: return item->avatarTextColor();
    case SourceIndexRole: return index.row();
    case HasPasswordRole: return !item->password.isEmpty();
    default: return {};
    }
}

QHash<int, QByteArray> PasswordListModel::roleNames() const {
    return {
        {DisplayNameRole, "displayName"}, {UsernameRole, "username"},   {EmailRole, "email"},
        {WebsiteRole, "website"},         {NotesRole, "notes"},         {TagsRole, "tags"},
        {Has2FARole, "has2FA"},           {AvatarLetterRole, "avatarLetter"},
        {AvatarColorRole, "avatarColor"}, {AvatarTextColorRole, "avatarTextColor"},
        {SourceIndexRole, "sourceIndex"},   {HasPasswordRole, "hasPassword"},
    };
}

void PasswordListModel::onItemsChanged() {
    // Same number of rows (edit, click registered): update in place so views keep their
    // selection and scroll position. Added/removed items: reset.
    const int rows = rowCount();
    if (rows == m_lastRowCount && rows > 0) {
        emit dataChanged(index(0, 0), index(rows - 1, 0));
        return;
    }
    reset();
}

void PasswordListModel::reset() {
    beginResetModel();
    m_lastRowCount = m_service->isOpen() ? m_service->itemCount() : 0;
    endResetModel();
    emit countChanged();
}

} // namespace EasePass::App
