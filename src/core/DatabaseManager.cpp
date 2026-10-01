#include "DatabaseManager.h"

#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>

namespace EasePass::Core {

DatabaseManager& DatabaseManager::instance() {
    static DatabaseManager mgr;
    return mgr;
}

DatabaseManager::DatabaseManager() {
    // Ensure default directory exists
    QFileInfo defInfo(getDefaultDatabasePath());
    QDir().mkpath(defInfo.absolutePath());
}

DatabaseFile* DatabaseManager::currentDatabase() {
    return m_currentDb.get();
}

const DatabaseFile* DatabaseManager::currentDatabase() const {
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
    QSettings settings("EasePass", "EasePassQt");
    QStringList list = settings.value("knownDatabases").toStringList();
    QString defPath = getDefaultDatabasePath();
    if (!list.contains(defPath) && QFile::exists(defPath)) {
        list.prepend(defPath);
    }
    return list;
}

void DatabaseManager::addKnownDatabasePath(const QString& path) {
    if (path.isEmpty()) return;
    QSettings settings("EasePass", "EasePassQt");
    QStringList list = settings.value("knownDatabases").toStringList();
    if (!list.contains(path)) {
        list.append(path);
        settings.setValue("knownDatabases", list);
    }
}

void DatabaseManager::removeKnownDatabasePath(const QString& path) {
    QSettings settings("EasePass", "EasePassQt");
    QStringList list = settings.value("knownDatabases").toStringList();
    list.removeAll(path);
    settings.setValue("knownDatabases", list);
}

QString DatabaseManager::getLastUsedDatabasePath() const {
    QSettings settings("EasePass", "EasePassQt");
    QString path = settings.value("lastUsedDatabase").toString();
    if (path.isEmpty() || !QFile::exists(path)) {
        QString defPath = getDefaultDatabasePath();
        if (QFile::exists(defPath)) {
            return defPath;
        }
    }
    return path;
}

void DatabaseManager::setLastUsedDatabasePath(const QString& path) {
    QSettings settings("EasePass", "EasePassQt");
    settings.setValue("lastUsedDatabase", path);
}

QString DatabaseManager::getDefaultDatabasePath() {
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dataDir.isEmpty()) {
        dataDir = QDir::homePath() + "/.local/share/EasePass";
    }
    return dataDir + "/easepass.epdb";
}

} // namespace EasePass::Core
