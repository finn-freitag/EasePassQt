#include <QApplication>
#include <QIcon>
#include "MainWindow.h"
#include "ICoreDumpControl.h"
#include "DatabaseManager.h"
#include "WebsiteIconManager.h"

int main(int argc, char *argv[]) {
    // Disable core dumps early to prevent sensitive memory exposure in crash dumps (T3)
    EasePass::Core::PlatformCoreDumpControl coreDumpControl;
    coreDumpControl.disableCoreDumps();

    QApplication app(argc, argv);

    app.setApplicationName("EasePass");
    app.setApplicationDisplayName("Ease Pass");
    app.setOrganizationName("EasePass");
    app.setApplicationVersion("1.4.0");
    app.setWindowIcon(QIcon(":/assets/appicon.svg"));

    // Favicon fetching follows AppSettings::fetchFavicons (T7, default off).
    EasePass::Core::WebsiteIconManager::instance().setSettings(EasePass::Core::DatabaseManager::instance().settings());

    EasePass::UI::MainWindow window;
    window.show();

    return app.exec();
}
