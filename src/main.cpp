#include <QApplication>
#include <QIcon>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    app.setApplicationName("EasePass");
    app.setApplicationDisplayName("Ease Pass");
    app.setOrganizationName("EasePass");
    app.setApplicationVersion("1.4.0");
    app.setWindowIcon(QIcon(":/assets/appicon.svg"));

    EasePass::UI::MainWindow window;
    window.show();

    return app.exec();
}
