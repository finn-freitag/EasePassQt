// easepass_qml: Qt Quick front end (steps.txt T11, D14). The legacy Widgets app (`easepass`)
// keeps working until parity is reached in T24.
#include <QApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QtQml/qqmlextensionplugin.h>

#include "AppContext.h"
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
    EasePass::App::AppContext context;
    context.registerQml();

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("EasePass", "Main");

    return app.exec();
}
