#pragma once

#include "CryptoHelper.h"

#include <QObject>
#include <memory>

class QQmlEngine;

namespace EasePass::Core {
class AppSettings;
class ClipboardManager;
class DatabaseOperations;
class DatabaseService;
class IClipboard;
class IClock;
class INetworkClient;
class IScreenCapture;
class ISettingsStore;
class LeakCheckService;
class WebsiteIconManager;
} // namespace EasePass::Core

namespace EasePass::App {

class AppController;
class ClipboardService;
class DailyTipService;
class PasswordActions;
class WebsiteIconService;
class DatabaseController;
class DialogService;
class ItemViewModel;
class PasswordFilterModel;
class PasswordListModel;
class PrintService;
class PrintViewModel;
class ScreenScanViewModel;
class SettingsViewModel;
class TotpTimerService;

/**
 * Composition root of the QML app (steps.txt T12): owns the Core services and all view
 * models and wires them together. Every platform dependency comes in through Deps, so tests
 * build the whole graph with fakes (no singletons, steps.txt 3.1).
 * registerQml() exposes the objects as singletons of the QML module "EasePass.App".
 */
class AppContext : public QObject {
    Q_OBJECT
public:
    struct Deps {
        std::shared_ptr<Core::ISettingsStore> settingsStore;  ///< default: QSettingsStore
        std::shared_ptr<Core::IClipboard> clipboard;          ///< default: QtClipboard
        std::shared_ptr<Core::INetworkClient> networkClient;  ///< default: QtNetworkClient
        std::shared_ptr<Core::IScreenCapture> screenCapture;  ///< default: createScreenCapture()
        QString iconCacheDir;                                 ///< default: WebsiteIconManager default
        const Core::IClock *clock = nullptr;                  ///< default: SystemClock
        Core::KdfParams kdf = Core::KdfParams::production();  ///< tests pass cheap params
    };

    explicit AppContext(QObject *parent = nullptr);
    explicit AppContext(Deps deps, QObject *parent = nullptr);
    ~AppContext() override;

    /// Registers the singletons + uncreatable enum types under "EasePass.App" 1.0.
    void registerQml();

    Core::AppSettings *settings() const { return m_settings.get(); }
    Core::DatabaseService *databaseService() const { return m_service; }
    Core::DatabaseOperations *databaseOperations() const { return m_operations; }
    TotpTimerService *totpTimer() const { return m_totpTimer; }
    AppController *appController() const { return m_appController; }
    DatabaseController *databaseController() const { return m_databaseController; }
    PasswordListModel *passwordListModel() const { return m_listModel; }
    PasswordFilterModel *passwordFilterModel() const { return m_filterModel; }
    ItemViewModel *itemViewModel() const { return m_itemViewModel; }
    SettingsViewModel *settingsViewModel() const { return m_settingsViewModel; }
    DialogService *dialogService() const { return m_dialogService; }
    ClipboardService *clipboardService() const { return m_clipboardService; }
    PrintViewModel *printViewModel() const { return m_printViewModel; }
    ScreenScanViewModel *screenScanViewModel() const { return m_screenScanViewModel; }
    DailyTipService *dailyTipService() const { return m_dailyTip; }
    PasswordActions *passwordActions() const { return m_actions; }
    WebsiteIconService *websiteIcons() const { return m_icons; }

private:
    const Core::IClock *m_clock;
    std::shared_ptr<Core::ISettingsStore> m_store;
    std::shared_ptr<Core::AppSettings> m_settings;
    Core::DatabaseService *m_service = nullptr;
    Core::DatabaseOperations *m_operations = nullptr;
    Core::ClipboardManager *m_clipboardManager = nullptr;
    Core::LeakCheckService *m_leakCheck = nullptr;
    PrintService *m_printService = nullptr;
    TotpTimerService *m_totpTimer = nullptr;
    AppController *m_appController = nullptr;
    DatabaseController *m_databaseController = nullptr;
    PasswordListModel *m_listModel = nullptr;
    PasswordFilterModel *m_filterModel = nullptr;
    ItemViewModel *m_itemViewModel = nullptr;
    SettingsViewModel *m_settingsViewModel = nullptr;
    DialogService *m_dialogService = nullptr;
    ClipboardService *m_clipboardService = nullptr;
    PrintViewModel *m_printViewModel = nullptr;
    ScreenScanViewModel *m_screenScanViewModel = nullptr;
    DailyTipService *m_dailyTip = nullptr;
    Core::WebsiteIconManager *m_iconManager = nullptr;
    PasswordActions *m_actions = nullptr;
    WebsiteIconService *m_icons = nullptr;
};

} // namespace EasePass::App
