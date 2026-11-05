// easepass_qml: Qt Quick front end (steps.txt T11, D14). The legacy Widgets app (`easepass`)
// keeps working until parity is reached in T24.
#include <QApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QtQml/qqmlextensionplugin.h>

#include "AppContext.h"
#include "AppController.h"
#include "ClipboardService.h"
#include "DailyTipService.h"
#include "DatabaseController.h"
#include "DatabaseManagementViewModel.h"
#include "PrintViewModel.h"
#include "SettingsViewModel.h"
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
    app.setApplicationVersion("1.4.0");
    app.setWindowIcon(QIcon(":/assets/appicon.svg"));

    EasePass::App::applyQuickStyle();

    // Composition root: Core services + view models, exposed as "EasePass.App" singletons (T12).
    EasePass::App::AppContext::Deps deps;
#ifdef EASEPASS_HAVE_MULTIMEDIA
    deps.webcamScanner = std::make_shared<EasePass::Multimedia::QtWebcamScanner>();
#endif
    EasePass::App::AppContext context(std::move(deps));
    context.registerQml();

    QQmlApplicationEngine engine;
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
    });
    engine.loadFromModule("EasePass", "Main");

    return app.exec();
}
