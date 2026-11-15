#include "AppSettingsTest.h"
#include "AppSettings.h"
#include <QTest>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QFile>

using namespace EasePass::Core;

namespace {
std::unique_ptr<QTemporaryDir> s_tempDir;
}

void AppSettingsTest::initTestCase() {
    QStandardPaths::setTestModeEnabled(true);
    s_tempDir = std::make_unique<QTemporaryDir>();
    QVERIFY(s_tempDir->isValid());
}

void AppSettingsTest::testDefaults() {
    auto settings = AppSettings::createInMemory();

    QCOMPARE(settings->language(), QStringLiteral("en-US"));
    QVERIFY(!settings->windowGeometry().isValid()); // never saved: the window manager places the window
    QCOMPARE(AppSettings::defaultWindowSize(), QSize(1100, 700));
    QCOMPARE(settings->windowMaximized(), false);
    QCOMPARE(settings->passwordLength(), 15);
    QCOMPARE(settings->passwordChars(),
             QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890!\"§$%&/()=?*+'#-_.:,;<>"));
    QCOMPARE(settings->inactivityLogoutMinutes(), 3);
    QCOMPARE(settings->doubleClickCopy(), true);
    QCOMPARE(settings->showIcons(), true);
    QCOMPARE(settings->verifyNotLeaked(), true);
    QCOMPARE(settings->splitterWidth(), 270);
    QCOMPARE(settings->clipboardClearSeconds(), 15);
    QCOMPARE(settings->databasePaths(), QStringList{AppSettings::defaultDatabasePath()});
    QCOMPARE(settings->lastDatabase(), QString());
    QCOMPARE(settings->removePluginSettingsOnUninstall(), true);
    QCOMPARE(settings->passwordGeneratorPlugin(), QString());
    QCOMPARE(settings->fetchFavicons(), false); // off by default: fetching icons reveals the user's accounts
    QCOMPARE(settings->databaseDeferredSaveSeconds(), 5);
}

namespace {
// Sets a value twice: the first call stores it and emits the signal once with the new value,
// the second call (same value) emits nothing.
template <typename T, typename Getter, typename Setter, typename Signal>
void checkProperty(AppSettings *settings, Getter getter, Setter setter, Signal signal, const T &value) {
    QVERIFY2((settings->*getter)() != value, "the test value must differ from the current one");
    QSignalSpy spy(settings, signal);
    (settings->*setter)(value);
    QCOMPARE((settings->*getter)(), value);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().at(0).template value<T>(), value);
    (settings->*setter)(value);
    QCOMPARE(spy.count(), 1);
}
} // namespace

void AppSettingsTest::testSetGetAndSignals() {
    auto settings = AppSettings::createInMemory();
    AppSettings *s = settings.get();
    using A = AppSettings;

    checkProperty<QString>(s, &A::language, &A::setLanguage, &A::languageChanged, "de-DE");
    checkProperty<bool>(s, &A::windowMaximized, &A::setWindowMaximized, &A::windowMaximizedChanged, true);
    checkProperty<int>(s, &A::passwordLength, &A::setPasswordLength, &A::passwordLengthChanged, 24);
    checkProperty<QString>(s, &A::passwordChars, &A::setPasswordChars, &A::passwordCharsChanged, "abc123!@#");
    checkProperty<int>(s, &A::inactivityLogoutMinutes, &A::setInactivityLogoutMinutes,
                       &A::inactivityLogoutMinutesChanged, 10);
    checkProperty<bool>(s, &A::doubleClickCopy, &A::setDoubleClickCopy, &A::doubleClickCopyChanged, false);
    checkProperty<bool>(s, &A::showIcons, &A::setShowIcons, &A::showIconsChanged, false);
    checkProperty<bool>(s, &A::verifyNotLeaked, &A::setVerifyNotLeaked, &A::verifyNotLeakedChanged, false);
    checkProperty<int>(s, &A::splitterWidth, &A::setSplitterWidth, &A::splitterWidthChanged, 320);
    checkProperty<int>(s, &A::clipboardClearSeconds, &A::setClipboardClearSeconds, &A::clipboardClearSecondsChanged, 30);
    checkProperty<QStringList>(s, &A::databasePaths, &A::setDatabasePaths, &A::databasePathsChanged,
                               QStringList{"/tmp/a.epdb", "/tmp/b.epdb"});
    checkProperty<QString>(s, &A::lastDatabase, &A::setLastDatabase, &A::lastDatabaseChanged, "/tmp/a.epdb");
    checkProperty<bool>(s, &A::removePluginSettingsOnUninstall, &A::setRemovePluginSettingsOnUninstall,
                        &A::removePluginSettingsOnUninstallChanged, false);
    checkProperty<QString>(s, &A::passwordGeneratorPlugin, &A::setPasswordGeneratorPlugin,
                           &A::passwordGeneratorPluginChanged, "org.example.generator");
    checkProperty<bool>(s, &A::fetchFavicons, &A::setFetchFavicons, &A::fetchFaviconsChanged, true);
    checkProperty<int>(s, &A::databaseDeferredSaveSeconds, &A::setDatabaseDeferredSaveSeconds,
                       &A::databaseDeferredSaveSecondsChanged, 10);

    // Text values are cleaned before they are stored.
    s->setPasswordGeneratorPlugin("  org.example.other ");
    QCOMPARE(s->passwordGeneratorPlugin(), QString("org.example.other"));
    s->setLanguage("   ");
    QCOMPARE(s->language(), QString("en-US")); // empty = default
    s->setPasswordChars(QString());
    QCOMPARE(s->passwordChars(), AppSettings::Defaults::PasswordChars); // a generator needs characters
}

void AppSettingsTest::testRangeClamping() {
    auto settings = AppSettings::createInMemory();

    // passwordLength (min 8)
    settings->setPasswordLength(7);
    QCOMPARE(settings->passwordLength(), 8);
    settings->setPasswordLength(0);
    QCOMPARE(settings->passwordLength(), 8);
    settings->setPasswordLength(-10);
    QCOMPARE(settings->passwordLength(), 8);
    settings->setPasswordLength(20);
    QCOMPARE(settings->passwordLength(), 20);

    // inactivityLogoutMinutes (1..1000)
    settings->setInactivityLogoutMinutes(0);
    QCOMPARE(settings->inactivityLogoutMinutes(), 1);
    settings->setInactivityLogoutMinutes(-5);
    QCOMPARE(settings->inactivityLogoutMinutes(), 1);
    settings->setInactivityLogoutMinutes(1001);
    QCOMPARE(settings->inactivityLogoutMinutes(), 1000);
    settings->setInactivityLogoutMinutes(5000);
    QCOMPARE(settings->inactivityLogoutMinutes(), 1000);
    settings->setInactivityLogoutMinutes(30);
    QCOMPARE(settings->inactivityLogoutMinutes(), 30);

    // clipboardClearSeconds (0..100000)
    settings->setClipboardClearSeconds(-5);
    QCOMPARE(settings->clipboardClearSeconds(), 0);
    settings->setClipboardClearSeconds(0);
    QCOMPARE(settings->clipboardClearSeconds(), 0);
    settings->setClipboardClearSeconds(200000);
    QCOMPARE(settings->clipboardClearSeconds(), 100000);
    settings->setClipboardClearSeconds(60);
    QCOMPARE(settings->clipboardClearSeconds(), 60);

    // splitterWidth (min 0)
    settings->setSplitterWidth(-50);
    QCOMPARE(settings->splitterWidth(), 0);
    settings->setSplitterWidth(300);
    QCOMPARE(settings->splitterWidth(), 300);

    // databaseDeferredSaveSeconds (min 1)
    settings->setDatabaseDeferredSaveSeconds(0);
    QCOMPARE(settings->databaseDeferredSaveSeconds(), 1);
    settings->setDatabaseDeferredSaveSeconds(-5);
    QCOMPARE(settings->databaseDeferredSaveSeconds(), 1);
    settings->setDatabaseDeferredSaveSeconds(15);
    QCOMPARE(settings->databaseDeferredSaveSeconds(), 15);
}

void AppSettingsTest::testWindowGeometry() {
    auto settings = AppSettings::createInMemory();
    QSignalSpy spyGeom(settings.get(), &AppSettings::windowGeometryChanged);

    const QRect target(100, 150, 1400, 900);
    settings->setWindowGeometry(target);
    QCOMPARE(settings->windowGeometry(), target);
    QCOMPARE(spyGeom.count(), 1);
    settings->setWindowGeometry(target);
    QCOMPARE(spyGeom.count(), 1);

    // Negative positions are fine (a screen left of the primary one), tiny sizes are not.
    settings->setWindowGeometry(QRect(-800, -20, 50, 30));
    QCOMPARE(settings->windowGeometry(), QRect(-800, -20, 200, 100));
    // An invalid rectangle (nothing to remember) keeps what is stored.
    settings->setWindowGeometry(QRect());
    QCOMPARE(settings->windowGeometry(), QRect(-800, -20, 200, 100));
}

void AppSettingsTest::testDatabasePathsAddRemoveDedupe() {
    auto settings = AppSettings::createInMemory();

    QString p1 = "/tmp/db1.epdb";
    QString p2 = "/tmp/db2.epdb";

    settings->addDatabasePath(p1);
    QVERIFY(settings->databasePaths().contains(p1));

    // Deduplication
    int countBefore = settings->databasePaths().size();
    settings->addDatabasePath(p1);
    QCOMPARE(settings->databasePaths().size(), countBefore);

    // Empty and whitespace paths ignored
    settings->addDatabasePath("");
    settings->addDatabasePath("   ");
    QCOMPARE(settings->databasePaths().size(), countBefore);

    // Add second path
    settings->addDatabasePath(p2);
    QVERIFY(settings->databasePaths().contains(p2));

    // Remove first path
    settings->removeDatabasePath(p1);
    QVERIFY(!settings->databasePaths().contains(p1));
    QVERIFY(settings->databasePaths().contains(p2));

    // Remove nonexistent path does not fail
    countBefore = settings->databasePaths().size();
    settings->removeDatabasePath("/tmp/nonexistent.epdb");
    QCOMPARE(settings->databasePaths().size(), countBefore);
}

void AppSettingsTest::testPersistenceAcrossInstances() {
    // 1. InMemorySettingsStore
    auto memStore = std::make_shared<InMemorySettingsStore>();
    {
        AppSettings s1(memStore);
        s1.setLanguage("pt-BR");
        s1.setPasswordLength(32);
        s1.setShowIcons(false);
        s1.setFetchFavicons(true);
        s1.setLastDatabase("/tmp/active.epdb");
        s1.setDatabasePaths(QStringList{"/tmp/active.epdb", "/tmp/archive.epdb"});
    }
    {
        AppSettings s2(memStore);
        QCOMPARE(s2.language(), QString("pt-BR"));
        QCOMPARE(s2.passwordLength(), 32);
        QCOMPARE(s2.showIcons(), false);
        QCOMPARE(s2.fetchFavicons(), true);
        QCOMPARE(s2.lastDatabase(), QString("/tmp/active.epdb"));
        QCOMPARE(s2.databasePaths(), (QStringList{"/tmp/active.epdb", "/tmp/archive.epdb"}));
    }

    // 2. QSettingsStore with file format
    QString iniFile = s_tempDir->filePath("test_persistence.ini");
    {
        auto qStore1 = std::make_shared<QSettingsStore>(iniFile, QSettings::IniFormat);
        AppSettings qs1(qStore1);
        qs1.setLanguage("de-DE");
        qs1.setSplitterWidth(400);
        qs1.setInactivityLogoutMinutes(12);
        qs1.setVerifyNotLeaked(false);
        qs1.setWindowGeometry(QRect(-10, 20, 640, 480));
        qs1.setWindowMaximized(true);
        qs1.setDatabasePaths({"/tmp/one.epdb"}); // a single entry must stay a list in the file
    }
    {
        auto qStore2 = std::make_shared<QSettingsStore>(iniFile, QSettings::IniFormat);
        AppSettings qs2(qStore2);
        QCOMPARE(qs2.language(), QString("de-DE"));
        QCOMPARE(qs2.splitterWidth(), 400);
        QCOMPARE(qs2.inactivityLogoutMinutes(), 12);
        QCOMPARE(qs2.verifyNotLeaked(), false);
        QCOMPARE(qs2.windowGeometry(), QRect(-10, 20, 640, 480));
        QCOMPARE(qs2.windowMaximized(), true);
        QCOMPARE(qs2.databasePaths(), QStringList{"/tmp/one.epdb"});
    }
}

void AppSettingsTest::testResets() {
    auto settings = AppSettings::createInMemory();
    settings->setPasswordChars("test_chars");
    settings->setPasswordLength(25);
    QSignalSpy chars(settings.get(), &AppSettings::passwordCharsChanged);
    QSignalSpy length(settings.get(), &AppSettings::passwordLengthChanged);

    settings->resetPasswordChars();
    QCOMPARE(settings->passwordChars(), AppSettings::Defaults::PasswordChars);
    QCOMPARE(chars.count(), 1);
    QCOMPARE(settings->passwordLength(), 25); // one reset does not touch the other value

    settings->resetPasswordLength();
    QCOMPARE(settings->passwordLength(), 15);
    QCOMPARE(length.count(), 1);
}
