#include "MainWindowLoader.h"

#include "AboutViewModel.h"
#include "AppContext.h"
#include "AppController.h"
#include "AutoLogoutService.h"
#include "ClipboardService.h"
#include "DailyTipService.h"
#include "DatabaseController.h"
#include "DatabaseManagementViewModel.h"
#include "DialogService.h"
#include "ItemViewModel.h"
#include "LanguageManager.h"
#include "PasswordActions.h"
#include "PasswordFilterModel.h"
#include "PasswordGeneratorViewModel.h"
#include "PrintViewModel.h"
#include "QrImportService.h"
#include "ScreenScanViewModel.h"
#include "SettingsViewModel.h"
#include "WebcamScanViewModel.h"
#include "WebsiteIconService.h"
#include "AppSettings.h"

#include <QQmlApplicationEngine>
#include <QQuickWindow>

namespace EasePass::App {

QQuickWindow *loadMainWindow(QQmlApplicationEngine &engine, AppContext &context) {
    context.languageManager()->setEngine(&engine); // live language switching (T20)
    engine.addImageProvider(WebsiteIconService::ProviderId, context.websiteIcons()->createImageProvider());
    engine.addImageProvider(WebcamScanViewModel::ProviderId, context.webcamScan()->createImageProvider());
    auto obj = [](QObject *o) { return QVariant::fromValue<QObject *>(o); };
    engine.setInitialProperties({
        {QStringLiteral("appController"), obj(context.appController())},
        {QStringLiteral("databaseController"), obj(context.databaseController())},
        {QStringLiteral("dailyTip"), obj(context.dailyTipService())},
        {QStringLiteral("dialogService"), obj(context.dialogService())},
        {QStringLiteral("clipboard"), obj(context.clipboardService())},
        {QStringLiteral("passwordList"), obj(context.passwordFilterModel())},
        {QStringLiteral("itemViewModel"), obj(context.itemViewModel())},
        {QStringLiteral("passwordActions"), obj(context.passwordActions())},
        {QStringLiteral("websiteIcons"), obj(context.websiteIcons())},
        {QStringLiteral("settings"), obj(context.settings())},
        {QStringLiteral("editor"), obj(context.editorViewModel())},
        {QStringLiteral("generator"), obj(context.passwordGenerator())},
        {QStringLiteral("qrImport"), obj(context.qrImport())},
        {QStringLiteral("screenScan"), obj(context.screenScanViewModel())},
        {QStringLiteral("webcamScan"), obj(context.webcamScan())},
        {QStringLiteral("databaseManagement"), obj(context.databaseManagement())},
        {QStringLiteral("printer"), obj(context.printViewModel())},
        {QStringLiteral("settingsViewModel"), obj(context.settingsViewModel())},
        {QStringLiteral("about"), obj(context.about())},
        {QStringLiteral("autoLogout"), obj(context.autoLogout())},
    });
    engine.loadFromModule("EasePass", "Main");
    if (engine.rootObjects().isEmpty())
        return nullptr;
    return qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
}

} // namespace EasePass::App
