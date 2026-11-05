#pragma once

#include "PasswordItem.h"

#include <QAbstractListModel>

namespace EasePass::Core { class DatabaseService; }

namespace EasePass::App {

/**
 * Items of the open database as a list model (steps.txt T12). Passwords and 2FA secrets are
 * deliberately NOT exposed as roles; QML gets them through ItemViewModel only.
 * Resets on open/close and when the item count changes; in-place edits emit dataChanged.
 */
class PasswordListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    enum Role {
        DisplayNameRole = Qt::UserRole + 1,
        UsernameRole,
        EmailRole,
        WebsiteRole,
        NotesRole,
        TagsRole,
        Has2FARole,
        AvatarLetterRole,
        AvatarColorRole,
        AvatarTextColorRole,
        SourceIndexRole,
        HasPasswordRole, ///< only whether one is set, never the password itself
    };
    Q_ENUM(Role)

    explicit PasswordListModel(Core::DatabaseService *service, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return rowCount(); }
    const Core::PasswordItem *itemAt(int row) const;
    Core::DatabaseService *service() const { return m_service; }

signals:
    void countChanged();

private:
    void reset();
    void onItemsChanged();
    Core::DatabaseService *m_service;
    int m_lastRowCount = 0;
};

} // namespace EasePass::App
