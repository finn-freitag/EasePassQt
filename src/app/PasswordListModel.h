#pragma once

#include "PasswordItem.h"

#include <QAbstractListModel>

namespace EasePass::Core { class DatabaseService; }

namespace EasePass::App {

/**
 * Items of the open database as a list model (steps.txt T12). Passwords and 2FA secrets are
 * deliberately NOT exposed as roles; QML gets them through ItemViewModel only.
 * Resets on every DatabaseService::itemsChanged/opened/closed.
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
    Core::DatabaseService *m_service;
};

} // namespace EasePass::App
