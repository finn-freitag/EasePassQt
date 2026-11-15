#include "AppSettings.h"
#include <QStandardPaths>
#include <QDir>
#include <algorithm>

namespace EasePass::Core {

const QString AppSettings::Defaults::Language = QStringLiteral("en-US");
const QString AppSettings::Defaults::PasswordChars =
    QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890!\"§$%&/()=?*+'#-_.:,;<>");

AppSettings::AppSettings(std::shared_ptr<ISettingsStore> store, QObject *parent)
    : QObject(parent), m_store(store ? std::move(store) : std::make_shared<QSettingsStore>()) {}

std::shared_ptr<AppSettings> AppSettings::createInMemory(QObject *parent) {
    return std::make_shared<AppSettings>(std::make_shared<InMemorySettingsStore>(), parent);
}

QString AppSettings::defaultDatabasePath() {
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dataDir.isEmpty()) {
        dataDir = QDir::homePath() + "/.local/share/EasePass";
    }
    return dataDir + "/easepass.epdb";
}

QStringList AppSettings::defaultDatabasePaths() {
    return QStringList{defaultDatabasePath()};
}

// --- Language ---
QString AppSettings::language() const {
    return m_store->value(Keys::Language, Defaults::Language).toString();
}

void AppSettings::setLanguage(const QString &language) {
    QString val = language.trimmed().isEmpty() ? Defaults::Language : language.trimmed();
    if (this->language() == val)
        return;
    m_store->setValue(Keys::Language, val);
    emit languageChanged(val);
}

// --- Window ---
QRect AppSettings::windowGeometry() const {
    return m_store->value(Keys::WindowGeometry).toRect();
}

void AppSettings::setWindowGeometry(const QRect &geometry) {
    if (!geometry.isValid())
        return;
    const QRect cleaned(geometry.x(), geometry.y(), std::max(200, geometry.width()), std::max(100, geometry.height()));
    if (windowGeometry() == cleaned)
        return;
    m_store->setValue(Keys::WindowGeometry, cleaned);
    emit windowGeometryChanged(cleaned);
}

bool AppSettings::windowMaximized() const {
    return m_store->value(Keys::WindowMaximized, false).toBool();
}

void AppSettings::setWindowMaximized(bool maximized) {
    if (windowMaximized() == maximized)
        return;
    m_store->setValue(Keys::WindowMaximized, maximized);
    emit windowMaximizedChanged(maximized);
}

// --- Password Generator Options ---
int AppSettings::passwordLength() const {
    return m_store->value(Keys::PasswordLength, Defaults::PasswordLength).toInt();
}

void AppSettings::setPasswordLength(int length) {
    int clamped = std::max(8, length);
    if (passwordLength() == clamped)
        return;
    m_store->setValue(Keys::PasswordLength, clamped);
    emit passwordLengthChanged(clamped);
}

QString AppSettings::passwordChars() const {
    return m_store->value(Keys::PasswordChars, Defaults::PasswordChars).toString();
}

void AppSettings::setPasswordChars(const QString &chars) {
    QString val = chars.isEmpty() ? Defaults::PasswordChars : chars;
    if (passwordChars() == val)
        return;
    m_store->setValue(Keys::PasswordChars, val);
    emit passwordCharsChanged(val);
}

// --- Security Options ---
int AppSettings::inactivityLogoutMinutes() const {
    return m_store->value(Keys::InactivityLogoutMinutes, Defaults::InactivityLogoutMinutes).toInt();
}

void AppSettings::setInactivityLogoutMinutes(int minutes) {
    int clamped = std::clamp(minutes, 1, 1000);
    if (inactivityLogoutMinutes() == clamped)
        return;
    m_store->setValue(Keys::InactivityLogoutMinutes, clamped);
    emit inactivityLogoutMinutesChanged(clamped);
}

bool AppSettings::doubleClickCopy() const {
    return m_store->value(Keys::DoubleClickCopy, Defaults::DoubleClickCopy).toBool();
}

void AppSettings::setDoubleClickCopy(bool enable) {
    if (doubleClickCopy() == enable)
        return;
    m_store->setValue(Keys::DoubleClickCopy, enable);
    emit doubleClickCopyChanged(enable);
}

bool AppSettings::showIcons() const {
    return m_store->value(Keys::ShowIcons, Defaults::ShowIcons).toBool();
}

void AppSettings::setShowIcons(bool show) {
    if (showIcons() == show)
        return;
    m_store->setValue(Keys::ShowIcons, show);
    emit showIconsChanged(show);
}

bool AppSettings::verifyNotLeaked() const {
    return m_store->value(Keys::VerifyNotLeaked, Defaults::VerifyNotLeaked).toBool();
}

void AppSettings::setVerifyNotLeaked(bool verify) {
    if (verifyNotLeaked() == verify)
        return;
    m_store->setValue(Keys::VerifyNotLeaked, verify);
    emit verifyNotLeakedChanged(verify);
}

int AppSettings::splitterWidth() const {
    return m_store->value(Keys::SplitterWidth, Defaults::SplitterWidth).toInt();
}

void AppSettings::setSplitterWidth(int width) {
    int clamped = std::max(0, width);
    if (splitterWidth() == clamped)
        return;
    m_store->setValue(Keys::SplitterWidth, clamped);
    emit splitterWidthChanged(clamped);
}

int AppSettings::clipboardClearSeconds() const {
    return m_store->value(Keys::ClipboardClearSeconds, Defaults::ClipboardClearSeconds).toInt();
}

void AppSettings::setClipboardClearSeconds(int seconds) {
    int clamped = std::clamp(seconds, 0, 100000);
    if (clipboardClearSeconds() == clamped)
        return;
    m_store->setValue(Keys::ClipboardClearSeconds, clamped);
    emit clipboardClearSecondsChanged(clamped);
}

// --- Databases ---
QStringList AppSettings::databasePaths() const {
    const QVariant val = m_store->value(Keys::DatabasePaths);
    return val.isValid() ? val.toStringList() : defaultDatabasePaths();
}

void AppSettings::setDatabasePaths(const QStringList &paths) {
    QStringList cleaned;
    for (const QString &p : paths) {
        QString trimmed = p.trimmed();
        if (!trimmed.isEmpty() && !cleaned.contains(trimmed)) {
            cleaned.append(trimmed);
        }
    }
    if (m_store->contains(Keys::DatabasePaths) && databasePaths() == cleaned)
        return;
    m_store->setValue(Keys::DatabasePaths, cleaned);
    emit databasePathsChanged(cleaned);
}

void AppSettings::addDatabasePath(const QString &path) {
    QString trimmed = path.trimmed();
    if (trimmed.isEmpty())
        return;

    QStringList paths = databasePaths();
    if (!paths.contains(trimmed)) {
        paths.append(trimmed);
        setDatabasePaths(paths);
    }
}

void AppSettings::removeDatabasePath(const QString &path) {
    QString trimmed = path.trimmed();
    if (trimmed.isEmpty())
        return;

    QStringList paths = databasePaths();
    if (paths.contains(trimmed)) {
        paths.removeAll(trimmed);
        setDatabasePaths(paths);
    }
}

QString AppSettings::lastDatabase() const {
    return m_store->value(Keys::LastDatabase, QString()).toString();
}

void AppSettings::setLastDatabase(const QString &path) {
    QString trimmed = path.trimmed();
    if (lastDatabase() == trimmed)
        return;
    m_store->setValue(Keys::LastDatabase, trimmed);
    emit lastDatabaseChanged(trimmed);
}

// --- Plugins & Misc ---
bool AppSettings::removePluginSettingsOnUninstall() const {
    return m_store->value(Keys::RemovePluginSettingsOnUninstall, Defaults::RemovePluginSettingsOnUninstall).toBool();
}

void AppSettings::setRemovePluginSettingsOnUninstall(bool remove) {
    if (removePluginSettingsOnUninstall() == remove)
        return;
    m_store->setValue(Keys::RemovePluginSettingsOnUninstall, remove);
    emit removePluginSettingsOnUninstallChanged(remove);
}

QString AppSettings::passwordGeneratorPlugin() const {
    return m_store->value(Keys::PasswordGeneratorPlugin, QString()).toString();
}

void AppSettings::setPasswordGeneratorPlugin(const QString &pluginId) {
    const QString trimmed = pluginId.trimmed();
    if (passwordGeneratorPlugin() == trimmed)
        return;
    m_store->setValue(Keys::PasswordGeneratorPlugin, trimmed);
    emit passwordGeneratorPluginChanged(trimmed);
}

bool AppSettings::fetchFavicons() const {
    return m_store->value(Keys::FetchFavicons, Defaults::FetchFavicons).toBool();
}

void AppSettings::setFetchFavicons(bool fetch) {
    if (fetchFavicons() == fetch)
        return;
    m_store->setValue(Keys::FetchFavicons, fetch);
    emit fetchFaviconsChanged(fetch);
}

int AppSettings::databaseDeferredSaveSeconds() const {
    return m_store->value(Keys::DatabaseDeferredSaveSeconds, Defaults::DatabaseDeferredSaveSeconds).toInt();
}

void AppSettings::setDatabaseDeferredSaveSeconds(int seconds) {
    int clamped = std::max(1, seconds);
    if (databaseDeferredSaveSeconds() == clamped)
        return;
    m_store->setValue(Keys::DatabaseDeferredSaveSeconds, clamped);
    emit databaseDeferredSaveSecondsChanged(clamped);
}

// --- Resets ---
void AppSettings::resetPasswordChars() {
    setPasswordChars(Defaults::PasswordChars);
}

void AppSettings::resetPasswordLength() {
    setPasswordLength(Defaults::PasswordLength);
}

} // namespace EasePass::Core
