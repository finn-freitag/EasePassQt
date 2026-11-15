#include "AppContext.h"

#include "AppController.h"
#include "AppSettings.h"
#include "ClipboardManager.h"
#include "ClipboardService.h"
#include "DailyTipService.h"
#include "PasswordActions.h"
#include "PasswordGeneratorViewModel.h"
#include "QrImportService.h"
#include "WebcamScanViewModel.h"
#include "IWebcamScanner.h"
#include "WebsiteIconManager.h"
#include "WebsiteIconService.h"
#include "DatabaseController.h"
#include "DatabaseManagementViewModel.h"
#include "AboutViewModel.h"
#include "LanguageManager.h"
#include "AutoLogoutService.h"
#include "InactivityManager.h"
#include "DatabaseOperations.h"
#include "DatabaseService.h"
#include "DialogService.h"
#include "IClipboard.h"
#include "IClock.h"
#include "INetworkClient.h"
#include "IScreenCapture.h"
#include "ISettingsStore.h"
#include "ItemViewModel.h"
#include "LeakCheckService.h"
#include "PasswordFilterModel.h"
#include "PasswordListModel.h"
#include "PrintService.h"
#include "PrintViewModel.h"
#include "ScreenScanViewModel.h"
#include "SettingsViewModel.h"
#include "TotpTimerService.h"

#ifdef EASEPASS_HAVE_PLUGINS
#include "BuildInfo.h"
#include "PluginsViewModel.h"
#include <QDir>
#include <QPointer>
#include <QStandardPaths>
#endif

#include <QQmlEngine>

namespace EasePass::App {

using namespace EasePass::Core;

AppContext::AppContext(QObject *parent) : AppContext(Deps{}, parent) {}

AppContext::AppContext(Deps deps, QObject *parent)
    : QObject(parent), m_clock(deps.clock ? deps.clock : &SystemClock::instance()) {
    m_store = deps.settingsStore ? deps.settingsStore : std::make_shared<QSettingsStore>();
    m_settings = std::make_shared<AppSettings>(m_store);
    m_language = new LanguageManager(m_settings.get(), this); // translators before any tr() (T20)
    auto clipboard = deps.clipboard ? deps.clipboard : std::make_shared<QtClipboard>();
    auto network = deps.networkClient ? deps.networkClient : std::make_shared<QtNetworkClient>();
    auto capture = deps.screenCapture ? deps.screenCapture : std::shared_ptr<IScreenCapture>(createScreenCapture());

    m_service = new DatabaseService(deps.kdf, this);
    m_service->setSaveDelayMs(m_settings->databaseDeferredSaveSeconds() * 1000);
    m_operations = new DatabaseOperations(m_settings, m_service, deps.kdf, this);
    m_clipboardManager = new ClipboardManager(clipboard, m_settings.get(), m_clock, this);
    m_leakCheck = new LeakCheckService(network, m_settings.get(), m_clock, this);
    m_printService = new PrintService(this);

    m_totpTimer = new TotpTimerService(m_clock, this);
    m_appController = new AppController(m_service, this);
    m_databaseController = new DatabaseController(m_service, m_operations, m_settings, this);
    m_listModel = new PasswordListModel(m_service, this);
    m_filterModel = new PasswordFilterModel(m_listModel, m_clock, this);
    m_clipboardService = new ClipboardService(m_clipboardManager, this);
    m_itemViewModel = new ItemViewModel(m_service, m_totpTimer, this);
    m_itemViewModel->setLeakCheckService(m_leakCheck);
    m_itemViewModel->setClipboardService(m_clipboardService);
    m_itemViewModel->setSettings(m_settings.get());
    m_editorViewModel = new ItemViewModel(m_service, m_totpTimer, this);
    m_editorViewModel->setLeakCheckService(m_leakCheck);
    m_editorViewModel->setClipboardService(m_clipboardService);
    m_editorViewModel->setSettings(m_settings.get());
    m_generator = new PasswordGeneratorViewModel(m_settings.get(), m_service, m_leakCheck, this);
    m_qrImport = new QrImportService(clipboard, this);
    m_webcam = new WebcamScanViewModel(deps.webcamScanner, this);
    m_settingsViewModel = new SettingsViewModel(m_settings.get(), this);
    m_settingsViewModel->setDatabaseService(m_service);
    m_dialogService = new DialogService(this);
    m_printViewModel = new PrintViewModel(m_service, m_printService, this);
    m_screenScanViewModel = new ScreenScanViewModel(capture, this);
    m_about = new AboutViewModel(this);
    m_inactivity = new InactivityManager(m_settings.get(), m_clock, this);
    m_autoLogout = new AutoLogoutService(m_service, m_inactivity, this);
    m_databaseManagement = new DatabaseManagementViewModel(m_service, m_operations, m_settings, m_clipboardService, this);

    m_dailyTip = new DailyTipService(m_clock, this);
    m_dailyTip->setLanguage(m_settings->language());
    connect(m_settings.get(), &AppSettings::languageChanged, m_dailyTip, &DailyTipService::setLanguage);

    m_actions = new PasswordActions(m_service, m_operations, m_clipboardService, m_totpTimer, this);
    m_iconManager = new WebsiteIconManager(network);
    m_iconManager->setParent(this);
    m_iconManager->setSettings(m_settings.get());
    if (!deps.iconCacheDir.isEmpty())
        m_iconManager->setCacheDir(deps.iconCacheDir);
    m_icons = new WebsiteIconService(m_iconManager, m_settings.get(), this);

#ifdef EASEPASS_HAVE_PLUGINS
    PluginsViewModel::Config plugins;
    plugins.pluginsDirectory = !deps.pluginsDirectory.isEmpty()
                                   ? deps.pluginsDirectory
                                   : QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                                         .filePath(QStringLiteral("plugins"));
    plugins.catalogUrl = deps.pluginCatalogUrl;
    plugins.network = network;
    plugins.settings = m_settings.get();
    plugins.service = m_service;
    plugins.leakCheck = m_leakCheck;
    plugins.dialogs = m_dialogService;
    plugins.clock = m_clock;
    plugins.appVersion = BuildInfo::appVersion();
    m_plugins = new PluginsViewModel(plugins, this);
    // Databases of database-source plugins join the local ones on the login and database pages.
    const auto syncRemote = [this] {
        m_databaseController->setRemoteDatabases(m_plugins->remoteDatabases());
        m_databaseManagement->setRemoteDatabases(m_plugins->remoteDatabases());
        std::function<void()> refresh;
        if (m_plugins->providerCount() > 0)
            refresh = [plugins = QPointer<PluginsViewModel>(m_plugins)] {
                if (plugins)
                    plugins->refreshRemoteDatabases();
            };
        m_databaseController->setRemoteProviders(refresh, m_plugins->remoteLoading());
    };
    connect(m_plugins, &PluginsViewModel::remoteDatabasesChanged, this, syncRemote);
    connect(m_plugins, &PluginsViewModel::installedChanged, this, syncRemote);
    // password-generator plugins are offered in the generator dialog.
    const auto syncGenerators = [this] { m_generator->setExternalGenerators(m_plugins->passwordGenerators()); };
    connect(m_plugins, &PluginsViewModel::installedChanged, this, syncGenerators);
    syncRemote();
    syncGenerators();
#endif

    // First run (no database file known yet) starts with the Register/OOBE page.
    m_appController->start(m_databaseController->isFirstRun());

    // Locking drops open questions; their answers would act on a closed database.
    connect(m_service, &DatabaseService::closed, m_dialogService, &DialogService::cancelAll);
    // T23: locking (by hand or by inactivity) also removes a copied password or TOTP code from
    // the clipboard right away, if it is still ours, instead of waiting for the timer.
    connect(m_service, &DatabaseService::closed, m_clipboardManager, &ClipboardManager::clearNow);
    connect(m_settings.get(), &AppSettings::databaseDeferredSaveSecondsChanged, m_service,
            [this](int s) { m_service->setSaveDelayMs(s * 1000); });
}

AppContext::~AppContext() {
#ifdef EASEPASS_HAVE_PLUGINS
    // Stops the plugin threads first: plugins may still call into services owned by this object.
    delete m_plugins;
    m_plugins = nullptr;
#endif
    // Finish background saves before the members are destroyed.
    m_operations->waitForBackgroundWork();
    m_service->shutdown();
}

void AppContext::registerQml() {
    const char *uri = "EasePass.App";
    auto single = [&](const char *name, QObject *obj) {
        QQmlEngine::setObjectOwnership(obj, QQmlEngine::CppOwnership);
        qmlRegisterSingletonInstance(uri, 1, 0, name, obj);
    };
    single("AppController", m_appController);
    single("DatabaseController", m_databaseController);
    single("PasswordList", m_filterModel);
    single("ItemViewModel", m_itemViewModel);
    single("EditorViewModel", m_editorViewModel);
    single("PasswordGenerator", m_generator);
    single("QrImport", m_qrImport);
    single("WebcamScan", m_webcam);
    single("DatabaseManagement", m_databaseManagement);
    single("About", m_about);
    single("AutoLogout", m_autoLogout);
    single("SettingsViewModel", m_settingsViewModel);
    single("DialogService", m_dialogService);
    single("ClipboardService", m_clipboardService);
    single("PrintViewModel", m_printViewModel);
    single("ScreenScanViewModel", m_screenScanViewModel);
    single("TotpTimer", m_totpTimer);
    single("DailyTip", m_dailyTip);
    single("PasswordActions", m_actions);
    single("WebsiteIcons", m_icons);
#ifdef EASEPASS_HAVE_PLUGINS
    single("Plugins", m_plugins);
#endif
    qmlRegisterUncreatableType<AppSettings>(uri, 1, 0, "AppSettings", QStringLiteral("use SettingsViewModel.settings"));
    qmlRegisterUncreatableType<PasswordListModel>(uri, 1, 0, "PasswordListModel", QStringLiteral("singleton only"));
}

} // namespace EasePass::App
