#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QRect>
#include <QSize>
#include <memory>
#include "ISettingsStore.h"

namespace EasePass::Core {

/**
 * The application's settings, typed, with defaults and value ranges. Every setter clamps or
 * cleans its value, stores it and emits the change signal only when the value really changed.
 */
class AppSettings : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(QRect windowGeometry READ windowGeometry WRITE setWindowGeometry NOTIFY windowGeometryChanged)
    Q_PROPERTY(bool windowMaximized READ windowMaximized WRITE setWindowMaximized NOTIFY windowMaximizedChanged)
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
    Q_PROPERTY(QString passwordGeneratorPlugin READ passwordGeneratorPlugin WRITE setPasswordGeneratorPlugin NOTIFY
                   passwordGeneratorPluginChanged)
    Q_PROPERTY(bool fetchFavicons READ fetchFavicons WRITE setFetchFavicons NOTIFY fetchFaviconsChanged)
    Q_PROPERTY(int databaseDeferredSaveSeconds READ databaseDeferredSaveSeconds WRITE setDatabaseDeferredSaveSeconds
                   NOTIFY databaseDeferredSaveSecondsChanged)

public:
    struct Defaults {
        static const QString Language; // "en-US"
        static constexpr int WindowWidth = 1100;
        static constexpr int WindowHeight = 700;
        static constexpr int PasswordLength = 15; // Min 8
        static const QString
            PasswordChars; // "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890!\"§$%&/()=?*+'#-_.:,;<>"
        static constexpr int InactivityLogoutMinutes = 3; // 1..1000
        static constexpr bool DoubleClickCopy = true;
        static constexpr bool ShowIcons = true;
        static constexpr bool VerifyNotLeaked = true;
        static constexpr int SplitterWidth = 270;
        static constexpr int ClipboardClearSeconds = 15;
        static constexpr bool RemovePluginSettingsOnUninstall = true;
        static constexpr bool FetchFavicons = false; // off: fetching icons tells web sites which accounts exist
        static constexpr int DatabaseDeferredSaveSeconds = 5;
    };

    struct Keys {
        static inline const QString Language = QStringLiteral("language");
        static inline const QString WindowGeometry = QStringLiteral("windowGeometry");
        static inline const QString WindowMaximized = QStringLiteral("windowMaximized");
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
        static inline const QString PasswordGeneratorPlugin = QStringLiteral("passwordGeneratorPlugin");
        static inline const QString FetchFavicons = QStringLiteral("fetchFavicons");
        static inline const QString DatabaseDeferredSaveSeconds = QStringLiteral("databaseDeferredSaveSeconds");
    };

    explicit AppSettings(std::shared_ptr<ISettingsStore> store = nullptr, QObject *parent = nullptr);
    ~AppSettings() override = default;

    static std::shared_ptr<AppSettings> createInMemory(QObject *parent = nullptr);
    static QString defaultDatabasePath();
    static QStringList defaultDatabasePaths();

    ISettingsStore *store() const { return m_store.get(); }

    // Getters & Setters
    QString language() const;
    void setLanguage(const QString &language);

    /// Geometry of the normal (not maximized) main window. Invalid until the window was
    /// closed once: the first start uses defaultWindowSize() and the window manager's position.
    QRect windowGeometry() const;
    void setWindowGeometry(const QRect &geometry);
    static QSize defaultWindowSize() { return QSize(Defaults::WindowWidth, Defaults::WindowHeight); }

    bool windowMaximized() const;
    void setWindowMaximized(bool maximized);

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

    /// Id of the password-generator plugin used by the generator; empty = built-in generator.
    QString passwordGeneratorPlugin() const;
    void setPasswordGeneratorPlugin(const QString &pluginId);

    bool fetchFavicons() const;
    void setFetchFavicons(bool fetch);

    int databaseDeferredSaveSeconds() const;
    void setDatabaseDeferredSaveSeconds(int seconds);

    // Reset
    void resetPasswordChars();
    void resetPasswordLength();

signals:
    void languageChanged(const QString &language);
    void windowGeometryChanged(const QRect &geometry);
    void windowMaximizedChanged(bool maximized);
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
    void passwordGeneratorPluginChanged(const QString &pluginId);
    void fetchFaviconsChanged(bool fetch);
    void databaseDeferredSaveSecondsChanged(int seconds);

private:
    std::shared_ptr<ISettingsStore> m_store;
};

} // namespace EasePass::Core
