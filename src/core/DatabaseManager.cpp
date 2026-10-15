#include "DatabaseManager.h"

#include <QDir>
#include <QFileInfo>
#include <QFile>

namespace EasePass::Core {

DatabaseManager &DatabaseManager::instance() {
    static DatabaseManager mgr;
    return mgr;
}

DatabaseManager::DatabaseManager() : m_settings(std::make_shared<AppSettings>()) {
    // Ensure default directory exists
    QFileInfo defInfo(getDefaultDatabasePath());
    QDir().mkpath(defInfo.absolutePath());
}

DatabaseFile *DatabaseManager::currentDatabase() {
    return m_currentDb.get();
}

const DatabaseFile *DatabaseManager::currentDatabase() const {
    return m_currentDb.get();
}

bool DatabaseManager::isDatabaseLoaded() const {
    return m_currentDb != nullptr;
}

void DatabaseManager::setDatabase(std::unique_ptr<DatabaseFile> db) {
    m_currentDb = std::move(db);
    if (m_currentDb) {
        addKnownDatabasePath(m_currentDb->filePath);
        setLastUsedDatabasePath(m_currentDb->filePath);
        emit databaseLoaded();
    } else {
        emit databaseClosed();
    }
}

void DatabaseManager::closeDatabase() {
    if (m_currentDb) {
        m_currentDb.reset();
        emit databaseClosed();
    }
}

QStringList DatabaseManager::getKnownDatabasePaths() const {
    QStringList list = m_settings->databasePaths();
    QString defPath = getDefaultDatabasePath();
    if (!list.contains(defPath) && QFile::exists(defPath)) {
        list.prepend(defPath);
    }
    return list;
}

void DatabaseManager::addKnownDatabasePath(const QString &path) {
    if (path.isEmpty())
        return;
    m_settings->addDatabasePath(path);
}

void DatabaseManager::removeKnownDatabasePath(const QString &path) {
    if (path.isEmpty())
        return;
    m_settings->removeDatabasePath(path);
}

QString DatabaseManager::getLastUsedDatabasePath() const {
    QString path = m_settings->lastDatabase();
    if (path.isEmpty() || !QFile::exists(path)) {
        QString defPath = getDefaultDatabasePath();
        if (QFile::exists(defPath)) {
            return defPath;
        }
    }
    return path;
}

void DatabaseManager::setLastUsedDatabasePath(const QString &path) {
    m_settings->setLastDatabase(path);
}

QString DatabaseManager::getDefaultDatabasePath() {
    return AppSettings::defaultDatabasePath();
}

AppSettings *DatabaseManager::settings() const {
    return m_settings.get();
}

void DatabaseManager::setSettings(std::shared_ptr<AppSettings> settings) {
    if (settings) {
        m_settings = std::move(settings);
    }
}

} // namespace EasePass::Core
