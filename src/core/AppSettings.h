#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QRect>
#include <memory>
#include "ISettingsStore.h"

namespace EasePass::Core {

/**
 * Central typed settings object for EasePass (P9).
 * Mirrored from C# AppSettings.cs and DefaultSettingsValues.cs.
 */
class AppSettings : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(int windowWidth READ windowWidth WRITE setWindowWidth NOTIFY windowWidthChanged)
    Q_PROPERTY(int windowHeight READ windowHeight WRITE setWindowHeight NOTIFY windowHeightChanged)
    Q_PROPERTY(int windowLeft READ windowLeft WRITE setWindowLeft NOTIFY windowLeftChanged)
    Q_PROPERTY(int windowTop READ windowTop WRITE setWindowTop NOTIFY windowTopChanged)
    Q_PROPERTY(int windowState READ windowState WRITE setWindowState NOTIFY windowStateChanged)
    Q_PROPERTY(QRect windowGeometry READ windowGeometry WRITE setWindowGeometry NOTIFY windowGeometryChanged)
    Q_PROPERTY(int passwordLength READ passwordLength WRITE setPasswordLength NOTIFY passwordLengthChanged)
    Q_PROPERTY(QString passwordChars READ passwordChars WRITE setPasswordChars NOTIFY passwordCharsChanged)
    Q_PROPERTY(int inactivityLogoutMinutes READ inactivityLogoutMinutes WRITE setInactivityLogoutMinutes NOTIFY
                   inactivityLogoutMinutesChanged)
    Q_PROPERTY(bool doubleClickCopy READ doubleClickCopy WRITE setDoubleClickCopy NOTIFY doubleClickCopyChanged)
    Q_PROPERTY(bool showIcons READ showIcons WRITE setShowIcons NOTIFY showIconsChanged)
    Q_PROPERTY(bool verifyNotLeaked READ verifyNotLeaked WRITE setVerifyNotLeaked NOTIFY verifyNotLeakedChanged)
    Q_PROPERTY(int splitterWidth READ splitterWidth WRITE setSplitterWidth NOTIFY splitterWidthChanged)
    Q_PROPERTY(int clipboardClearSeconds READ clipboardClearSeconds WRITE setClipboardClearSeconds NOTIFY
                   clipboardClearSecondsChanged)
    Q_PROPERTY(QStringList databasePaths READ databasePaths WRITE setDatabasePaths NOTIFY databasePathsChanged)
    Q_PROPERTY(QString lastDatabase READ lastDatabase WRITE setLastDatabase NOTIFY lastDatabaseChanged)
    Q_PROPERTY(bool removePluginSettingsOnUninstall READ removePluginSettingsOnUninstall WRITE
                   setRemovePluginSettingsOnUninstall NOTIFY removePluginSettingsOnUninstallChanged)
    Q_PROPERTY(bool fetchFavicons READ fetchFavicons WRITE setFetchFavicons NOTIFY fetchFaviconsChanged)
    Q_PROPERTY(QString appVersion READ appVersion WRITE setAppVersion NOTIFY appVersionChanged)
    Q_PROPERTY(int databaseDeferredSaveSeconds READ databaseDeferredSaveSeconds WRITE setDatabaseDeferredSaveSeconds
                   NOTIFY databaseDeferredSaveSecondsChanged)

public:
    struct Defaults {
        static const QString Language; // "en-US"
        static constexpr int WindowWidth = 1100;
        static constexpr int WindowHeight = 700;
        static constexpr int WindowLeft = 0;
        static constexpr int WindowTop = 0;
        static constexpr int WindowState = 2;     // C#: OverlappedPresenterState.Restored = 2
        static constexpr int PasswordLength = 15; // Min 8
        static const QString
            PasswordChars; // "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890!\"§$%&/()=?*+'#-_.:,;<>"
        static constexpr int InactivityLogoutMinutes = 3; // 1..1000
        static constexpr bool DoubleClickCopy = true;
        static constexpr bool ShowIcons = true;
        static constexpr bool VerifyNotLeaked = true; // C#: disableLeakedPasswords = false
        static constexpr int SplitterWidth = 270;
        static constexpr int ClipboardClearSeconds = 15;
        static constexpr bool RemovePluginSettingsOnUninstall = true;
        static constexpr bool FetchFavicons = false; // Default OFF (fixes P8)
        static const QString AppVersion;             // ""
        static constexpr int DatabaseDeferredSaveSeconds = 5;
    };

    struct Keys {
        static inline const QString Language = QStringLiteral("language");
        static inline const QString WindowWidth = QStringLiteral("windowWidth");
        static inline const QString WindowHeight = QStringLiteral("windowHeight");
        static inline const QString WindowLeft = QStringLiteral("windowLeft");
        static inline const QString WindowTop = QStringLiteral("windowTop");
        static inline const QString WindowState = QStringLiteral("windowState");
        static inline const QString PasswordLength = QStringLiteral("passwordLength");
        static inline const QString PasswordChars = QStringLiteral("passwordChars");
        static inline const QString InactivityLogoutMinutes = QStringLiteral("inactivityLogoutMinutes");
        static inline const QString DoubleClickCopy = QStringLiteral("doubleClickCopy");
        static inline const QString ShowIcons = QStringLiteral("showIcons");
        static inline const QString VerifyNotLeaked = QStringLiteral("verifyNotLeaked");
        static inline const QString SplitterWidth = QStringLiteral("splitterWidth");
        static inline const QString ClipboardClearSeconds = QStringLiteral("clipboardClearSeconds");
        static inline const QString DatabasePaths = QStringLiteral("databasePaths");
        static inline const QString LastDatabase = QStringLiteral("lastDatabase");
        static inline const QString RemovePluginSettingsOnUninstall = QStringLiteral("removePluginSettingsOnUninstall");
        static inline const QString FetchFavicons = QStringLiteral("fetchFavicons");
        static inline const QString AppVersion = QStringLiteral("appVersion");
        static inline const QString DatabaseDeferredSaveSeconds = QStringLiteral("databaseDeferredSaveTime_Sec");
    };

    explicit AppSettings(std::shared_ptr<ISettingsStore> store = nullptr, QObject *parent = nullptr);
    ~AppSettings() override = default;

    static std::shared_ptr<AppSettings> createInMemory(QObject *parent = nullptr);
    static QString defaultDatabasePath();
    static QStringList defaultDatabasePaths();

    ISettingsStore *store() const { return m_store.get(); }
    void setStore(std::shared_ptr<ISettingsStore> store);

    // Getters & Setters
    QString language() const;
    void setLanguage(const QString &language);

    int windowWidth() const;
    void setWindowWidth(int width);

    int windowHeight() const;
    void setWindowHeight(int height);

    int windowLeft() const;
    void setWindowLeft(int left);

    int windowTop() const;
    void setWindowTop(int top);

    int windowState() const;
    void setWindowState(int state);

    QRect windowGeometry() const;
    void setWindowGeometry(const QRect &geometry);

    int passwordLength() const;
    void setPasswordLength(int length);

    QString passwordChars() const;
    void setPasswordChars(const QString &chars);

    int inactivityLogoutMinutes() const;
    void setInactivityLogoutMinutes(int minutes);

    bool doubleClickCopy() const;
    void setDoubleClickCopy(bool enable);

    bool showIcons() const;
    void setShowIcons(bool show);

    bool verifyNotLeaked() const;
    void setVerifyNotLeaked(bool verify);

    int splitterWidth() const;
    void setSplitterWidth(int width);

    int clipboardClearSeconds() const;
    void setClipboardClearSeconds(int seconds);

    QStringList databasePaths() const;
    void setDatabasePaths(const QStringList &paths);
    void addDatabasePath(const QString &path);
    void removeDatabasePath(const QString &path);

    QString lastDatabase() const;
    void setLastDatabase(const QString &path);

    bool removePluginSettingsOnUninstall() const;
    void setRemovePluginSettingsOnUninstall(bool remove);

    bool fetchFavicons() const;
    void setFetchFavicons(bool fetch);

    QString appVersion() const;
    void setAppVersion(const QString &version);

    int databaseDeferredSaveSeconds() const;
    void setDatabaseDeferredSaveSeconds(int seconds);

    // C# naming compatibility aliases
    bool doubleTapToCopy() const { return doubleClickCopy(); }
    void setDoubleTapToCopy(bool enable) { setDoubleClickCopy(enable); }
    int inactivityLogoutTime() const { return inactivityLogoutMinutes(); }
    void setInactivityLogoutTime(int time) { setInactivityLogoutMinutes(time); }
    bool disableLeakedPasswords() const { return !verifyNotLeaked(); }
    void setDisableLeakedPasswords(bool disable) { setVerifyNotLeaked(!disable); }
    int gridSplitterWidth() const { return splitterWidth(); }
    void setGridSplitterWidth(int width) { setSplitterWidth(width); }
    int clipboardClearTimeoutSec() const { return clipboardClearSeconds(); }
    void setClipboardClearTimeoutSec(int sec) { setClipboardClearSeconds(sec); }
    QString loadedDatabaseSource() const { return lastDatabase(); }
    void setLoadedDatabaseSource(const QString &src) { setLastDatabase(src); }

    // Reset & Migration
    void resetToDefaults();
    void resetPasswordChars();
    void resetPasswordLength();
    void migrateLegacyKeys();

signals:
    void languageChanged(const QString &language);
    void windowWidthChanged(int width);
    void windowHeightChanged(int height);
    void windowLeftChanged(int left);
    void windowTopChanged(int top);
    void windowStateChanged(int state);
    void windowGeometryChanged(const QRect &geometry);
    void passwordLengthChanged(int length);
    void passwordCharsChanged(const QString &chars);
    void inactivityLogoutMinutesChanged(int minutes);
    void doubleClickCopyChanged(bool enable);
    void showIconsChanged(bool show);
    void verifyNotLeakedChanged(bool verify);
    void splitterWidthChanged(int width);
    void clipboardClearSecondsChanged(int seconds);
    void databasePathsChanged(const QStringList &paths);
    void lastDatabaseChanged(const QString &path);
    void removePluginSettingsOnUninstallChanged(bool remove);
    void fetchFaviconsChanged(bool fetch);
    void appVersionChanged(const QString &version);
    void databaseDeferredSaveSecondsChanged(int seconds);

private:
    std::shared_ptr<ISettingsStore> m_store;
};

} // namespace EasePass::Core
