// easepass_qml: Qt Quick front end (steps.txt T11, D14). The legacy Widgets app (`easepass`)
// keeps working until parity is reached in T24.
#include <QApplication>
#include <QIcon>
#include <QCryptographicHash>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QStandardPaths>
#include <cstdio>
#include <QtQml/qqmlextensionplugin.h>

#include "AppContext.h"
#include "AppController.h"
#include "ClipboardService.h"
#include "DailyTipService.h"
#include "DatabaseController.h"
#include "DatabaseManagementViewModel.h"
#include "PrintViewModel.h"
#include "SettingsViewModel.h"
#include "AboutViewModel.h"
#include "ActivityEventFilter.h"
#include "AutoLogoutService.h"
#include "BuildInfo.h"
#include "CommandLine.h"
#include "FileLogger.h"
#include "LanguageManager.h"
#include "SingleInstance.h"
#include "WindowStateController.h"
#include "DialogService.h"
#include "ItemViewModel.h"
#include "PasswordActions.h"
#include "PasswordFilterModel.h"
#include "WebsiteIconService.h"
#include "PasswordGeneratorViewModel.h"
#include "QrImportService.h"
#include "ScreenScanViewModel.h"
#include "WebcamScanViewModel.h"
#ifdef EASEPASS_HAVE_MULTIMEDIA
#include "QtWebcamScanner.h"
#endif
#include "AppSettings.h"
#include "ICoreDumpControl.h"
#include "QuickStyle.h"

Q_IMPORT_QML_PLUGIN(EasePassPlugin)

int main(int argc, char *argv[]) {
    // Disable core dumps early to prevent sensitive memory exposure in crash dumps (T3)
    EasePass::Core::PlatformCoreDumpControl coreDumpControl;
    coreDumpControl.disableCoreDumps();

    // QApplication (not QGuiApplication) so C++ services can open QPrintDialog (D7).
    QApplication app(argc, argv);
    app.setApplicationName("EasePass");
    app.setApplicationDisplayName("Ease Pass");
    app.setOrganizationName("EasePass");
    app.setApplicationVersion(QStringLiteral(EASEPASS_VERSION));
    app.setWindowIcon(QIcon(":/assets/appicon.svg"));
    app.setDesktopFileName(QStringLiteral("easepass"));

    using namespace EasePass;

    // --- Command line (T19): easepass [--db <path>] [<path>] ----------------------------
    const App::CommandLineOptions options = App::parseCommandLine(app.arguments());
    if (options.helpRequested) {
        std::fputs(qPrintable(App::commandLineHelp()), stdout);
        return 0;
    }
    if (options.versionRequested) {
        std::printf("%s %s\n", qPrintable(app.applicationName()), qPrintable(app.applicationVersion()));
        return 0;
    }
    if (!options.error.isEmpty()) {
        std::fprintf(stderr, "%s\n\n%s", qPrintable(options.error), qPrintable(App::commandLineHelp()));
        return 1;
    }

    // --- Single instance (T19): a second launch hands its arguments to the first one -------
    // Keyed by the settings location as well, so a separate profile (XDG_CONFIG_HOME, tests)
    // gets its own instance.
    const QByteArray profile = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toUtf8();
    App::SingleInstance instance(QStringLiteral("easepass-")
                                 + QString::fromLatin1(QCryptographicHash::hash(profile, QCryptographicHash::Sha256).toHex().left(12)));
    if (!instance.tryBecomePrimary()) {
        if (instance.sendToPrimary(app.arguments().mid(1)))
            return 0;
        std::fprintf(stderr, "Ease Pass is already running but does not respond.\n");
        return 1;
    }

    // --- Log file (T19; never secrets, messages are redacted as a safeguard) --------------
    Core::FileLogger::Options logOptions;
    logOptions.directory = Core::FileLogger::defaultDirectory();
    logOptions.appVersion = Core::BuildInfo::appVersion();
    Core::FileLogger logger(logOptions);
    if (logger.open())
        logger.install();

    EasePass::App::applyQuickStyle();

    // Composition root: Core services + view models, exposed as "EasePass.App" singletons (T12).
    EasePass::App::AppContext::Deps deps;
#ifdef EASEPASS_HAVE_MULTIMEDIA
    deps.webcamScanner = std::make_shared<EasePass::Multimedia::QtWebcamScanner>();
#endif
    EasePass::App::AppContext context(std::move(deps));
    context.registerQml();

    // Inactivity logout: every key/mouse/touch input restarts the period (T7/T19).
    App::ActivityEventFilter activityFilter([&context] { context.autoLogout()->reportActivity(); });
    app.installEventFilter(&activityFilter);

    if (!options.databasePath.isEmpty() && !context.databaseController()->setStartupDatabase(options.databasePath))
        qWarning("Database file not found: %s", qPrintable(options.databasePath));

    QQmlApplicationEngine engine;
    context.languageManager()->setEngine(&engine); // live language switching (T20)
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.addImageProvider(EasePass::App::WebsiteIconService::ProviderId, context.websiteIcons()->createImageProvider());
    engine.addImageProvider(EasePass::App::WebcamScanViewModel::ProviderId, context.webcamScan()->createImageProvider());
    engine.setInitialProperties({
        {QStringLiteral("appController"), QVariant::fromValue<QObject *>(context.appController())},
        {QStringLiteral("databaseController"), QVariant::fromValue<QObject *>(context.databaseController())},
        {QStringLiteral("dailyTip"), QVariant::fromValue<QObject *>(context.dailyTipService())},
        {QStringLiteral("dialogService"), QVariant::fromValue<QObject *>(context.dialogService())},
        {QStringLiteral("clipboard"), QVariant::fromValue<QObject *>(context.clipboardService())},
        {QStringLiteral("passwordList"), QVariant::fromValue<QObject *>(context.passwordFilterModel())},
        {QStringLiteral("itemViewModel"), QVariant::fromValue<QObject *>(context.itemViewModel())},
        {QStringLiteral("passwordActions"), QVariant::fromValue<QObject *>(context.passwordActions())},
        {QStringLiteral("websiteIcons"), QVariant::fromValue<QObject *>(context.websiteIcons())},
        {QStringLiteral("settings"), QVariant::fromValue<QObject *>(context.settings())},
        {QStringLiteral("editor"), QVariant::fromValue<QObject *>(context.editorViewModel())},
        {QStringLiteral("generator"), QVariant::fromValue<QObject *>(context.passwordGenerator())},
        {QStringLiteral("qrImport"), QVariant::fromValue<QObject *>(context.qrImport())},
        {QStringLiteral("screenScan"), QVariant::fromValue<QObject *>(context.screenScanViewModel())},
        {QStringLiteral("webcamScan"), QVariant::fromValue<QObject *>(context.webcamScan())},
        {QStringLiteral("databaseManagement"), QVariant::fromValue<QObject *>(context.databaseManagement())},
        {QStringLiteral("printer"), QVariant::fromValue<QObject *>(context.printViewModel())},
        {QStringLiteral("settingsViewModel"), QVariant::fromValue<QObject *>(context.settingsViewModel())},
        {QStringLiteral("about"), QVariant::fromValue<QObject *>(context.about())},
        {QStringLiteral("autoLogout"), QVariant::fromValue<QObject *>(context.autoLogout())},
    });
    engine.loadFromModule("EasePass", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;

    // --- Window geometry/state (T19; C#: WindowStateManager). Main.qml starts hidden. -------
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    App::WindowStateController windowState(context.settings());
    if (window) {
        windowState.attach(window);
        window->setVisible(true);
    }
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &windowState, &App::WindowStateController::save);

    QObject::connect(&instance, &App::SingleInstance::activationRequested, &app,
                     [&context, window](const QStringList &arguments) {
        const App::CommandLineOptions second = App::parseCommandLine(QStringList{QStringLiteral("easepass")} + arguments);
        if (!second.databasePath.isEmpty()) {
            if (!context.databaseController()->isOpen())
                context.databaseController()->setStartupDatabase(second.databasePath);
            else
                context.dialogService()->showInfo(App::DialogService::Severity::Info, QString(),
                                                  QCoreApplication::translate("main", "Log out to open %1.")
                                                      .arg(App::DatabaseController::displayNameForPath(second.databasePath)));
        }
        if (window) {
            if (window->windowStates() & Qt::WindowMinimized)
                window->setWindowStates(window->windowStates() & ~Qt::WindowMinimized);
            window->show();
            window->raise();
            window->requestActivate();
        }
    });

    const int result = app.exec();
    logger.uninstall();
    return result;
}
