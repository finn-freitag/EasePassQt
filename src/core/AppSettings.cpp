#include "AppSettings.h"
#include <QStandardPaths>
#include <QDir>
#include <algorithm>

namespace EasePass::Core {

const QString AppSettings::Defaults::Language = QStringLiteral("en-US");
const QString AppSettings::Defaults::PasswordChars =
    QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890!\"§$%&/()=?*+'#-_.:,;<>");
const QString AppSettings::Defaults::AppVersion = QString();

AppSettings::AppSettings(std::shared_ptr<ISettingsStore> store, QObject *parent)
    : QObject(parent), m_store(store ? std::move(store) : std::make_shared<QSettingsStore>()) {
    migrateLegacyKeys();
}

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

void AppSettings::setStore(std::shared_ptr<ISettingsStore> store) {
    if (!store || m_store == store)
        return;
    m_store = std::move(store);
    migrateLegacyKeys();
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

// --- Window Geometry & State ---
int AppSettings::windowWidth() const {
    return m_store->value(Keys::WindowWidth, Defaults::WindowWidth).toInt();
}

void AppSettings::setWindowWidth(int width) {
    int clamped = std::max(200, width);
    if (windowWidth() == clamped)
        return;
    m_store->setValue(Keys::WindowWidth, clamped);
    emit windowWidthChanged(clamped);
    emit windowGeometryChanged(windowGeometry());
}

int AppSettings::windowHeight() const {
    return m_store->value(Keys::WindowHeight, Defaults::WindowHeight).toInt();
}

void AppSettings::setWindowHeight(int height) {
    int clamped = std::max(100, height);
    if (windowHeight() == clamped)
        return;
    m_store->setValue(Keys::WindowHeight, clamped);
    emit windowHeightChanged(clamped);
    emit windowGeometryChanged(windowGeometry());
}

int AppSettings::windowLeft() const {
    return m_store->value(Keys::WindowLeft, Defaults::WindowLeft).toInt();
}

void AppSettings::setWindowLeft(int left) {
    if (windowLeft() == left)
        return;
    m_store->setValue(Keys::WindowLeft, left);
    emit windowLeftChanged(left);
    emit windowGeometryChanged(windowGeometry());
}

int AppSettings::windowTop() const {
    return m_store->value(Keys::WindowTop, Defaults::WindowTop).toInt();
}

void AppSettings::setWindowTop(int top) {
    if (windowTop() == top)
        return;
    m_store->setValue(Keys::WindowTop, top);
    emit windowTopChanged(top);
    emit windowGeometryChanged(windowGeometry());
}

int AppSettings::windowState() const {
    return m_store->value(Keys::WindowState, Defaults::WindowState).toInt();
}

void AppSettings::setWindowState(int state) {
    if (windowState() == state)
        return;
    m_store->setValue(Keys::WindowState, state);
    emit windowStateChanged(state);
}

QRect AppSettings::windowGeometry() const {
    return QRect(windowLeft(), windowTop(), windowWidth(), windowHeight());
}

void AppSettings::setWindowGeometry(const QRect &geometry) {
    int l = geometry.x();
    int t = geometry.y();
    int w = std::max(200, geometry.width());
    int h = std::max(100, geometry.height());
    bool changed = (windowLeft() != l || windowTop() != t || windowWidth() != w || windowHeight() != h);
    if (!changed)
        return;
    m_store->setValue(Keys::WindowLeft, l);
    m_store->setValue(Keys::WindowTop, t);
    m_store->setValue(Keys::WindowWidth, w);
    m_store->setValue(Keys::WindowHeight, h);
    emit windowLeftChanged(l);
    emit windowTopChanged(t);
    emit windowWidthChanged(w);
    emit windowHeightChanged(h);
    emit windowGeometryChanged(QRect(l, t, w, h));
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
    QVariant val = m_store->value(Keys::DatabasePaths);
    if (!val.isValid()) {
        return defaultDatabasePaths();
    }
    if (val.typeId() == QMetaType::QString) {
        QString str = val.toString();
        if (str.contains('|')) {
            return str.split('|', Qt::SkipEmptyParts);
        } else if (str.contains(';')) {
            return str.split(';', Qt::SkipEmptyParts);
        } else if (!str.trimmed().isEmpty()) {
            return QStringList{str.trimmed()};
        }
        return defaultDatabasePaths();
    }
    if (val.canConvert<QStringList>()) {
        return val.toStringList();
    }
    return defaultDatabasePaths();
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

bool AppSettings::fetchFavicons() const {
    return m_store->value(Keys::FetchFavicons, Defaults::FetchFavicons).toBool();
}

void AppSettings::setFetchFavicons(bool fetch) {
    if (fetchFavicons() == fetch)
        return;
    m_store->setValue(Keys::FetchFavicons, fetch);
    emit fetchFaviconsChanged(fetch);
}

QString AppSettings::appVersion() const {
    return m_store->value(Keys::AppVersion, Defaults::AppVersion).toString();
}

void AppSettings::setAppVersion(const QString &version) {
    if (appVersion() == version)
        return;
    m_store->setValue(Keys::AppVersion, version);
    emit appVersionChanged(version);
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

void AppSettings::resetToDefaults() {
    setLanguage(Defaults::Language);
    setWindowGeometry(QRect(Defaults::WindowLeft, Defaults::WindowTop, Defaults::WindowWidth, Defaults::WindowHeight));
    setWindowState(Defaults::WindowState);
    setPasswordLength(Defaults::PasswordLength);
    setPasswordChars(Defaults::PasswordChars);
    setInactivityLogoutMinutes(Defaults::InactivityLogoutMinutes);
    setDoubleClickCopy(Defaults::DoubleClickCopy);
    setShowIcons(Defaults::ShowIcons);
    setVerifyNotLeaked(Defaults::VerifyNotLeaked);
    setSplitterWidth(Defaults::SplitterWidth);
    setClipboardClearSeconds(Defaults::ClipboardClearSeconds);
    setDatabasePaths(defaultDatabasePaths());
    setLastDatabase(QString());
    setRemovePluginSettingsOnUninstall(Defaults::RemovePluginSettingsOnUninstall);
    setFetchFavicons(Defaults::FetchFavicons);
    setAppVersion(Defaults::AppVersion);
    setDatabaseDeferredSaveSeconds(Defaults::DatabaseDeferredSaveSeconds);
    m_store->sync();
}

// --- Legacy Migration ---
void AppSettings::migrateLegacyKeys() {
    if (!m_store)
        return;

    // 1. databasePaths from legacy knownDatabases or delimited string
    if (!m_store->contains(Keys::DatabasePaths)) {
        if (m_store->contains(QStringLiteral("knownDatabases"))) {
            QVariant legacy = m_store->value(QStringLiteral("knownDatabases"));
            QStringList paths;
            if (legacy.typeId() == QMetaType::QString) {
                QString str = legacy.toString();
                paths = str.contains('|') ? str.split('|', Qt::SkipEmptyParts) : str.split(';', Qt::SkipEmptyParts);
            } else if (legacy.canConvert<QStringList>()) {
                paths = legacy.toStringList();
            }
            if (!paths.isEmpty()) {
                setDatabasePaths(paths);
            }
            m_store->remove(QStringLiteral("knownDatabases"));
        }
    } else {
        // If databasePaths exists as a delimited string (e.g. from C#), normalize to QStringList
        QVariant val = m_store->value(Keys::DatabasePaths);
        if (val.typeId() == QMetaType::QString) {
            QString str = val.toString();
            if (str.contains('|') || str.contains(';')) {
                QStringList paths =
                    str.contains('|') ? str.split('|', Qt::SkipEmptyParts) : str.split(';', Qt::SkipEmptyParts);
                setDatabasePaths(paths);
            }
        }
    }

    // 2. lastDatabase from legacy lastUsedDatabase or loadedDatabaseName
    if (!m_store->contains(Keys::LastDatabase)) {
        if (m_store->contains(QStringLiteral("lastUsedDatabase"))) {
            QString legacy = m_store->value(QStringLiteral("lastUsedDatabase")).toString();
            if (!legacy.isEmpty()) {
                setLastDatabase(legacy);
            }
            m_store->remove(QStringLiteral("lastUsedDatabase"));
        } else if (m_store->contains(QStringLiteral("loadedDatabaseName"))) {
            QString legacy = m_store->value(QStringLiteral("loadedDatabaseName")).toString();
            if (!legacy.isEmpty()) {
                setLastDatabase(legacy);
            }
            m_store->remove(QStringLiteral("loadedDatabaseName"));
        }
    }

    // 3. verifyNotLeaked from legacy disableLeakedPasswords
    if (!m_store->contains(Keys::VerifyNotLeaked)) {
        if (m_store->contains(QStringLiteral("disableLeakedPasswords"))) {
            bool disabled = m_store->value(QStringLiteral("disableLeakedPasswords")).toBool();
            setVerifyNotLeaked(!disabled);
            m_store->remove(QStringLiteral("disableLeakedPasswords"));
        }
    }

    // 4. inactivityLogoutMinutes from legacy inactivityLogoutTime
    if (!m_store->contains(Keys::InactivityLogoutMinutes)) {
        if (m_store->contains(QStringLiteral("inactivityLogoutTime"))) {
            int legacy = m_store->value(QStringLiteral("inactivityLogoutTime")).toInt();
            setInactivityLogoutMinutes(legacy);
            m_store->remove(QStringLiteral("inactivityLogoutTime"));
        }
    }

    // 5. doubleClickCopy from legacy doubleTapToCopy
    if (!m_store->contains(Keys::DoubleClickCopy)) {
        if (m_store->contains(QStringLiteral("doubleTapToCopy"))) {
            bool legacy = m_store->value(QStringLiteral("doubleTapToCopy")).toBool();
            setDoubleClickCopy(legacy);
            m_store->remove(QStringLiteral("doubleTapToCopy"));
        }
    }

    // 6. splitterWidth from legacy gridSplitterWidth
    if (!m_store->contains(Keys::SplitterWidth)) {
        if (m_store->contains(QStringLiteral("gridSplitterWidth"))) {
            int legacy = m_store->value(QStringLiteral("gridSplitterWidth")).toInt();
            setSplitterWidth(legacy);
            m_store->remove(QStringLiteral("gridSplitterWidth"));
        }
    }

    // 7. clipboardClearSeconds from legacy clipboardClearTimeoutSec
    if (!m_store->contains(Keys::ClipboardClearSeconds)) {
        if (m_store->contains(QStringLiteral("clipboardClearTimeoutSec"))) {
            int legacy = m_store->value(QStringLiteral("clipboardClearTimeoutSec")).toInt();
            setClipboardClearSeconds(legacy);
            m_store->remove(QStringLiteral("clipboardClearTimeoutSec"));
        }
    }

    m_store->sync();
}

} // namespace EasePass::Core
