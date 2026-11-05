// test_qml: QtQuickTest runner for the EasePass QML module (steps.txt T11).
// Every tst_*.qml calls failOnWarning() in init(), so any QML warning fails the test.
#include <QtQuickTest>
#include <QQmlEngine>
#include <QtQml/qqmlextensionplugin.h>
#include <QApplication>
#include <QStandardPaths>

#include "QuickStyle.h"
#include "QmlTestContext.h"

#include <QQmlContext>

Q_IMPORT_QML_PLUGIN(EasePassPlugin)

class Setup : public QObject {
    Q_OBJECT
public slots:
    void qmlEngineAvailable(QQmlEngine *engine) {
        // Real view models with fakes for the page tests (T14+).
        m_context = std::make_unique<QmlTestContext>();
        engine->rootContext()->setContextProperty(QStringLiteral("testContext"), m_context.get());
    }
    void cleanupTestCase() { m_context.reset(); }

private:
    std::unique_ptr<QmlTestContext> m_context;
};

int main(int argc, char **argv) {
    // QApplication like the real app (D7); quick_test_main creates none if one exists.
    // (applicationAvailable() is not called when the application already exists, so the
    // style and test mode are set up here, exactly like src/qmlapp/main.cpp.)
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    EasePass::App::applyQuickStyle();
    Setup setup;
    return quick_test_main_with_setup(argc, argv, "test_qml", EASEPASS_QML_TEST_DIR, &setup);
}

#include "main.moc"
