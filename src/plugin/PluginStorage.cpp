#include "PluginStorage.h"

#include "FilePermissions.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

namespace EasePass::Plugin {

namespace {

constexpr qsizetype MaxKeyBytes = 256;

HostError internalError(const QString &what) { return {QStringLiteral("internal"), what}; }

std::optional<HostError> checkKey(const QString &key) {
    if (key.isEmpty() || key.toUtf8().size() > MaxKeyBytes)
        return HostError{QStringLiteral("invalid-argument"), QStringLiteral("a key must have 1 to 256 bytes")};
    return std::nullopt;
}

std::optional<HostError> checkName(const QString &name) {
    if (!PluginStorage::isValidFileName(name))
        return HostError{QStringLiteral("invalid-argument"), QStringLiteral("not a plain file name")};
    return std::nullopt;
}

bool ensurePrivateDirectory(const QString &path) {
    QDir dir(path);
    if (!dir.exists() && !QDir().mkpath(path))
        return false;
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
                                    QFileDevice::ReadUser | QFileDevice::WriteUser | QFileDevice::ExeUser);
    return true;
}

bool writeAtomically(const QString &path, const QByteArray &data) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    Core::restrictToOwner(file);
    return file.write(data) == data.size() && file.commit();
}

} // namespace

PluginStorage::PluginStorage(const QString &directory) : PluginStorage(directory, Quota()) {}
PluginStorage::PluginStorage(const QString &directory, Quota quota) : m_directory(directory), m_quota(quota) {}

QString PluginStorage::settingsPath() const { return QDir(m_directory).filePath(QStringLiteral("settings.json")); }
QString PluginStorage::filesDirectory() const { return QDir(m_directory).filePath(QStringLiteral("files")); }

bool PluginStorage::isValidFileName(const QString &name) {
    if (name.isEmpty() || name.toUtf8().size() > 255 || name == QLatin1String(".") || name == QLatin1String(".."))
        return false;
    for (const QChar c : name) {
        if (c.unicode() < 0x20 || c.unicode() == 0x7f || QStringLiteral("/\\:*?\"<>|").contains(c))
            return false;
    }
    if (name.endsWith(QLatin1Char('.')) || name.endsWith(QLatin1Char(' ')))
        return false;
    // Reserved device names on Windows (with or without an extension).
    static const QStringList reserved = {
        QStringLiteral("CON"),  QStringLiteral("PRN"),  QStringLiteral("AUX"),  QStringLiteral("NUL"),
        QStringLiteral("COM1"), QStringLiteral("COM2"), QStringLiteral("COM3"), QStringLiteral("COM4"),
        QStringLiteral("COM5"), QStringLiteral("COM6"), QStringLiteral("COM7"), QStringLiteral("COM8"),
        QStringLiteral("COM9"), QStringLiteral("LPT1"), QStringLiteral("LPT2"), QStringLiteral("LPT3"),
        QStringLiteral("LPT4"), QStringLiteral("LPT5"), QStringLiteral("LPT6"), QStringLiteral("LPT7"),
        QStringLiteral("LPT8"), QStringLiteral("LPT9")};
    return !reserved.contains(name.section(QLatin1Char('.'), 0, 0).toUpper());
}

std::optional<HostError> PluginStorage::loadSettings(QJsonObject *settings) const {
    QFile file(settingsPath());
    if (!file.exists()) {
        *settings = {};
        return std::nullopt;
    }
    if (!file.open(QIODevice::ReadOnly))
        return internalError(QStringLiteral("cannot read the plugin settings"));
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject())
        return internalError(QStringLiteral("the plugin settings are damaged"));
    *settings = document.object();
    return std::nullopt;
}

std::optional<HostError> PluginStorage::saveSettings(const QJsonObject &settings) {
    if (!ensurePrivateDirectory(m_directory) ||
        !writeAtomically(settingsPath(), QJsonDocument(settings).toJson(QJsonDocument::Compact)))
        return internalError(QStringLiteral("cannot write the plugin settings"));
    return std::nullopt;
}

qint64 PluginStorage::usedBytes(const QString &exceptFile) const {
    qint64 total = QFileInfo(settingsPath()).size();
    const QFileInfoList files = QDir(filesDirectory()).entryInfoList(QDir::Files | QDir::Hidden | QDir::System);
    for (const QFileInfo &info : files) {
        if (info.fileName() != exceptFile)
            total += info.size();
    }
    return total;
}

std::optional<HostError> PluginStorage::getString(const QString &key, std::optional<QString> *value) const {
    if (auto error = checkKey(key))
        return error;
    QJsonObject settings;
    if (auto error = loadSettings(&settings))
        return error;
    const QJsonValue stored = settings.value(key);
    *value = stored.isString() ? std::optional<QString>(stored.toString()) : std::nullopt;
    return std::nullopt;
}

std::optional<HostError> PluginStorage::setString(const QString &key, const QString &value) {
    if (auto error = checkKey(key))
        return error;
    const qsizetype valueBytes = value.toUtf8().size();
    if (valueBytes > m_quota.maxValueBytes)
        return HostError{QStringLiteral("too-large"), QStringLiteral("the value is too large")};
    QJsonObject settings;
    if (auto error = loadSettings(&settings))
        return error;
    if (!settings.contains(key) && settings.size() >= m_quota.maxKeys)
        return HostError{QStringLiteral("too-large"), QStringLiteral("too many keys")};
    settings.insert(key, value);
    const qint64 newSettingsSize = QJsonDocument(settings).toJson(QJsonDocument::Compact).size();
    if (usedBytes() - QFileInfo(settingsPath()).size() + newSettingsSize > m_quota.totalBytes)
        return HostError{QStringLiteral("too-large"), QStringLiteral("the storage quota of the plugin is used up")};
    return saveSettings(settings);
}

std::optional<HostError> PluginStorage::deleteString(const QString &key) {
    if (auto error = checkKey(key))
        return error;
    QJsonObject settings;
    if (auto error = loadSettings(&settings))
        return error;
    if (!settings.contains(key))
        return std::nullopt;
    settings.remove(key);
    return saveSettings(settings);
}

std::optional<HostError> PluginStorage::listKeys(QStringList *keys) const {
    QJsonObject settings;
    if (auto error = loadSettings(&settings))
        return error;
    *keys = settings.keys();
    return std::nullopt;
}

std::optional<HostError> PluginStorage::readFile(const QString &name, std::optional<QByteArray> *data) const {
    if (auto error = checkName(name))
        return error;
    QFile file(QDir(filesDirectory()).filePath(name));
    if (!file.exists()) {
        *data = std::nullopt;
        return std::nullopt;
    }
    if (file.size() > m_quota.maxValueBytes)
        return HostError{QStringLiteral("too-large"), QStringLiteral("the file is too large to be read in one call")};
    if (!file.open(QIODevice::ReadOnly))
        return internalError(QStringLiteral("cannot read the file"));
    *data = file.readAll();
    return std::nullopt;
}

std::optional<HostError> PluginStorage::writeFile(const QString &name, const QByteArray &data) {
    if (auto error = checkName(name))
        return error;
    if (data.size() > m_quota.maxValueBytes)
        return HostError{QStringLiteral("too-large"), QStringLiteral("the file is too large")};
    const QDir files(filesDirectory());
    const bool exists = files.exists(name);
    if (!exists && files.entryList(QDir::Files | QDir::Hidden | QDir::System).size() >= m_quota.maxFiles)
        return HostError{QStringLiteral("too-large"), QStringLiteral("too many files")};
    if (usedBytes(name) + data.size() > m_quota.totalBytes)
        return HostError{QStringLiteral("too-large"), QStringLiteral("the storage quota of the plugin is used up")};
    if (!ensurePrivateDirectory(m_directory) || !ensurePrivateDirectory(filesDirectory()) ||
        !writeAtomically(files.filePath(name), data))
        return internalError(QStringLiteral("cannot write the file"));
    return std::nullopt;
}

std::optional<HostError> PluginStorage::deleteFile(const QString &name) {
    if (auto error = checkName(name))
        return error;
    QFile file(QDir(filesDirectory()).filePath(name));
    if (file.exists() && !file.remove())
        return internalError(QStringLiteral("cannot delete the file"));
    return std::nullopt;
}

std::optional<HostError> PluginStorage::listFiles(QStringList *names) const {
    *names = QDir(filesDirectory()).entryList(QDir::Files | QDir::Hidden | QDir::System, QDir::Name);
    return std::nullopt;
}

bool PluginStorage::removeAll(const QString &directory) {
    QDir dir(directory);
    return !dir.exists() || dir.removeRecursively();
}

} // namespace EasePass::Plugin
