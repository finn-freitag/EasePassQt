// test_smoke (steps.txt T24): the real main window (Main.qml through App::loadMainWindow, the
// same start-up path as src/main/main.cpp) with a real AppContext, offscreen. Unlocks a copy of
// the C# fixture test.epdb (production Argon2, hence label "slow"), checks that the items are
// listed and locks again. No network (stub client), no real user directories (test mode,
// in-memory settings).
#include "AppContext.h"
#include "AppController.h"
#include "AppSettings.h"
#include "DatabaseController.h"
#include "INetworkClient.h"
#include "ISettingsStore.h"
#include "MainWindowLoader.h"
#include "QuickStyle.h"

#include <QApplication>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(EasePassPlugin)

using namespace EasePass;

namespace {

/// Every request fails: the smoke test must not touch the network.
class OfflineNetworkClient : public Core::INetworkClient {
public:
    int requests = 0;
    void get(const QNetworkRequest &, ResponseCallback callback) override {
        ++requests;
        Core::NetworkResponse r;
        r.errorString = QStringLiteral("offline (test)");
        callback(r);
    }
};

QQuickItem *findItem(QQuickItem *root, const QString &objectName) {
    if (!root)
        return nullptr;
    if (root->objectName() == objectName && root->isVisible())
        return root;
    for (QQuickItem *child : root->childItems())
        if (QQuickItem *found = findItem(child, objectName))
            return found;
    return nullptr;
}

bool hasVisibleText(QQuickItem *root, const QString &text) {
    if (!root || !root->isVisible())
        return false;
    if (root->property("text").toString() == text)
        return true;
    for (QQuickItem *child : root->childItems())
        if (hasVisibleText(child, text))
            return true;
    return false;
}

void click(QQuickWindow *window, QQuickItem *item) {
    const QPointF center = item->mapToScene(QPointF(item->width() / 2, item->height() / 2));
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center.toPoint());
}

} // namespace

class SmokeTest : public QObject {
    Q_OBJECT
private slots:
    void testUnlockFixtureAndListItems();
};

void SmokeTest::testUnlockFixtureAndListItems() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString db = dir.filePath("test.epdb"); // a copy: the fixture is never modified
    QVERIFY(QFile::copy(QStringLiteral(EASEPASS_TEST_FIXTURES_DIR "/test.epdb"), db));
    QVERIFY(QFile::setPermissions(db, QFileDevice::ReadOwner | QFileDevice::WriteOwner));

    auto network = std::make_shared<OfflineNetworkClient>();
    App::AppContext::Deps deps;
    deps.settingsStore = std::make_shared<Core::InMemorySettingsStore>();
    deps.networkClient = network;
    deps.iconCacheDir = dir.filePath("icons");
    App::AppContext context(std::move(deps));
    context.settings()->setDatabasePaths({db});
    context.settings()->setLastDatabase(db);
    context.appController()->start(false);
    context.registerQml();

    QQmlApplicationEngine engine;
    QQuickWindow *window = App::loadMainWindow(engine, context);
    QVERIFY(window);
    window->resize(1100, 750);
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QCOMPARE(context.appController()->page(), App::AppController::Page::Login);

    // Login page: the known database is preselected; type the password and click "Login".
    QQuickItem *passwordBox = nullptr;
    QTRY_VERIFY((passwordBox = findItem(window->contentItem(), QStringLiteral("passwordBox"))));
    passwordBox->forceActiveFocus();
    for (char c : QByteArrayLiteral("123456"))
        QTest::keyClick(window, c); // keyClicks() has no QWindow overload
    QCOMPARE(passwordBox->property("text").toString(), QStringLiteral("123456"));
    QQuickItem *loginButton = findItem(window->contentItem(), QStringLiteral("loginButton"));
    QVERIFY(loginButton);
    QTRY_VERIFY(loginButton->isEnabled());
    click(window, loginButton);

    // Production Argon2 (250 MB x 10) runs on the worker thread.
    QTRY_VERIFY_WITH_TIMEOUT(context.databaseController()->isOpen(), 120000);
    QTRY_COMPARE(context.appController()->page(), App::AppController::Page::Passwords);

    // Passwords page lists the four fixture items.
    QQuickItem *list = nullptr;
    QTRY_VERIFY((list = findItem(window->contentItem(), QStringLiteral("passwordList"))));
    QTRY_COMPARE(list->property("count").toInt(), 4);
    QTRY_VERIFY(hasVisibleText(list, QStringLiteral("Test123")));
    QVERIFY(hasVisibleText(list, QStringLiteral("Nudel")));
    QVERIFY(hasVisibleText(list, QStringLiteral("Peters Futterecken")));

    // Lock: back on the login page with an empty password field.
    QQuickItem *logout = findItem(window->contentItem(), QStringLiteral("logoutButton"));
    QVERIFY(logout);
    click(window, logout);
    QTRY_VERIFY(!context.databaseController()->isOpen());
    QTRY_COMPARE(context.appController()->page(), App::AppController::Page::Login);
    QTRY_VERIFY((passwordBox = findItem(window->contentItem(), QStringLiteral("passwordBox"))));
    QVERIFY(passwordBox->property("text").toString().isEmpty());

    QCOMPARE(network->requests, 0);
}

int main(int argc, char *argv[]) {
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv); // like the real app (D7)
    App::applyQuickStyle();
    SmokeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "SmokeTest.moc"
