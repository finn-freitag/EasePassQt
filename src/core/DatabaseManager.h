#pragma once

#include <QObject>
#include <QStringList>
#include <memory>
#include "DatabaseFile.h"

namespace EasePass::Core {

class DatabaseManager : public QObject {
    Q_OBJECT
public:
    static DatabaseManager& instance();

    DatabaseFile* currentDatabase();
    const DatabaseFile* currentDatabase() const;
    bool isDatabaseLoaded() const;

    void setDatabase(std::unique_ptr<DatabaseFile> db);
    void closeDatabase();

    QStringList getKnownDatabasePaths() const;
    void addKnownDatabasePath(const QString& path);
    void removeKnownDatabasePath(const QString& path);

    QString getLastUsedDatabasePath() const;
    void setLastUsedDatabasePath(const QString& path);

    static QString getDefaultDatabasePath();

signals:
    void databaseLoaded();
    void databaseClosed();
    void itemsChanged();

private:
    DatabaseManager();
    ~DatabaseManager() override = default;

    std::unique_ptr<DatabaseFile> m_currentDb;
};

} // namespace EasePass::Core
