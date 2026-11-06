// easepass: Qt Quick application (steps.txt T11, D14; the Widgets UI was removed in T24).
#include <QApplication>
#include <QIcon>
#include <QCryptographicHash>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QStandardPaths>
#include <cstdio>
#include <QtQml/qqmlextensionplugin.h>

#include "AppContext.h"
#include "DatabaseController.h"
#include "ActivityEventFilter.h"
#include "AutoLogoutService.h"
#include "BuildInfo.h"
#include "CommandLine.h"
#include "FileLogger.h"
#include "MainWindowLoader.h"
#include "SingleInstance.h"
#include "WindowStateController.h"
#include "DialogService.h"
#ifdef EASEPASS_HAVE_MULTIMEDIA
#include "QtWebcamScanner.h"
#endif
#include "ICoreDumpControl.h"
#include "QuickStyle.h"

Q_IMPORT_QML_PLUGIN(EasePassPlugin)

int main(int argc, char *argv[]) {
    // Disable core dumps early to prevent sensitive memory exposure in crash dumps (T3)
    EasePass::Core::PlatformCoreDumpControl coreDumpControl;
    coreDumpControl.disableCoreDumps();

    using namespace EasePass;

    // --- Command line (T19): easepass [--db <path>] [<path>] ----------------------------
    // Parsed before the QApplication exists, so --help/--version also work without a display
    // (headless terminal, SSH, packaging checks; T24).
    QStringList rawArguments;
    for (int i = 0; i < argc; ++i)
        rawArguments << QString::fromLocal8Bit(argv[i]);
    const App::CommandLineOptions options = App::parseCommandLine(rawArguments);
    if (options.helpRequested) {
        std::fputs(qPrintable(App::commandLineHelp()), stdout);
        return 0;
    }
    if (options.versionRequested) {
        std::printf("EasePass %s\n", EASEPASS_VERSION);
        return 0;
    }
    if (!options.error.isEmpty()) {
        std::fprintf(stderr, "%s\n\n%s", qPrintable(options.error), qPrintable(App::commandLineHelp()));
        return 1;
    }

    // QApplication (not QGuiApplication) so C++ services can open QPrintDialog (D7).
    QApplication app(argc, argv);
    app.setApplicationName("EasePass");
    app.setApplicationDisplayName("Ease Pass");
    app.setOrganizationName("EasePass");
    app.setApplicationVersion(QStringLiteral(EASEPASS_VERSION));
    app.setWindowIcon(QIcon(":/assets/appicon.svg"));
    app.setDesktopFileName(QStringLiteral("easepass"));

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
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    QQuickWindow *window = App::loadMainWindow(engine, context);
    if (engine.rootObjects().isEmpty())
        return 1;

    // --- Window geometry/state (T19; C#: WindowStateManager). Main.qml starts hidden. -------
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
