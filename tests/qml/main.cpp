// test_qml: QtQuickTest runner for the EasePass QML module (steps.txt T11).
// Every tst_*.qml calls failOnWarning() in init(), so any QML warning fails the test.
#include <QtQuickTest>
#include <QQmlEngine>
#include <QtQml/qqmlextensionplugin.h>
#include <QApplication>
#include <QStandardPaths>

#include "QuickStyle.h"

Q_IMPORT_QML_PLUGIN(EasePassPlugin)

class Setup : public QObject {
    Q_OBJECT
public slots:
    void applicationAvailable() {
        QStandardPaths::setTestModeEnabled(true);
        EasePass::App::applyQuickStyle();
    }
};

int main(int argc, char **argv) {
    // QApplication like the real app (D7); quick_test_main creates none if one exists.
    QApplication app(argc, argv);
    Setup setup;
    return quick_test_main_with_setup(argc, argv, "test_qml", EASEPASS_QML_TEST_DIR, &setup);
}

#include "main.moc"
