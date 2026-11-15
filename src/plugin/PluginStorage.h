#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <optional>

namespace EasePass::Plugin {

/// Error of a host function, mapped to the WIT `error` record.
struct HostError {
    QString code; ///< WIT error-code, e.g. "invalid-argument", "too-large", "not-allowed"
    QString message;
};

/**
 * Private key/value settings and files of ONE plugin (permission `storage`,
 * C#: Helper/FileSystem/PluginStorageHelper.cs).
 *
 * Layout: <directory>/settings.json (all string values) and <directory>/files/<name>.
 * The directory is chosen by the host from the plugin id, never by the plugin; file names
 * are plain names, so a plugin cannot reach anything outside its directory.
 * Files are owner-only and written atomically.
 */
class PluginStorage {
public:
    struct Quota {
        qint64 totalBytes = 64ll * 1024 * 1024;
        int maxKeys = 1024;
        int maxFiles = 1024;
        qsizetype maxValueBytes = 2 * 1024 * 1024; ///< one string value or one file (PluginLimits::MaxByteListSize)
    };

    explicit PluginStorage(const QString &directory);
    PluginStorage(const QString &directory, Quota quota);

    QString directory() const { return m_directory; }

    std::optional<HostError> getString(const QString &key, std::optional<QString> *value) const;
    std::optional<HostError> setString(const QString &key, const QString &value);
    std::optional<HostError> deleteString(const QString &key);
    std::optional<HostError> listKeys(QStringList *keys) const;

    std::optional<HostError> readFile(const QString &name, std::optional<QByteArray> *data) const;
    std::optional<HostError> writeFile(const QString &name, const QByteArray &data);
    std::optional<HostError> deleteFile(const QString &name);
    std::optional<HostError> listFiles(QStringList *names) const;

    /// Plain file name: not empty, at most 255 bytes, no separators or control characters,
    /// not "." or "..", no Windows device names or trailing dot/space.
    static bool isValidFileName(const QString &name);

    /// Deletes everything the plugin stored (uninstall).
    static bool removeAll(const QString &directory);

private:
    QString settingsPath() const;
    QString filesDirectory() const;
    std::optional<HostError> loadSettings(QJsonObject *settings) const;
    std::optional<HostError> saveSettings(const QJsonObject &settings);
    qint64 usedBytes(const QString &exceptFile = {}) const;

    QString m_directory;
    Quota m_quota;
};

} // namespace EasePass::Plugin
