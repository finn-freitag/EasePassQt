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

    // Values copied from C# DefaultSettingsValues.cs
    QCOMPARE(settings->language(), QStringLiteral("en-US"));
    QCOMPARE(settings->windowWidth(), 1100);
    QCOMPARE(settings->windowHeight(), 700);
    QCOMPARE(settings->windowLeft(), 0);
    QCOMPARE(settings->windowTop(), 0);
    QCOMPARE(settings->windowState(), 2);
    QCOMPARE(settings->windowGeometry(), QRect(0, 0, 1100, 700));
    QCOMPARE(settings->passwordLength(), 15);
    QCOMPARE(settings->passwordChars(),
             QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890!\"§$%&/()=?*+'#-_.:,;<>"));
    QCOMPARE(settings->inactivityLogoutMinutes(), 3);
    QCOMPARE(settings->inactivityLogoutTime(), 3);
    QCOMPARE(settings->doubleClickCopy(), true);
    QCOMPARE(settings->doubleTapToCopy(), true);
    QCOMPARE(settings->showIcons(), true);
    QCOMPARE(settings->verifyNotLeaked(), true);
    QCOMPARE(settings->disableLeakedPasswords(), false);
    QCOMPARE(settings->splitterWidth(), 270);
    QCOMPARE(settings->gridSplitterWidth(), 270);
    QCOMPARE(settings->clipboardClearSeconds(), 15);
    QCOMPARE(settings->clipboardClearTimeoutSec(), 15);
    QCOMPARE(settings->databasePaths(), QStringList{AppSettings::defaultDatabasePath()});
    QCOMPARE(settings->lastDatabase(), QString());
    QCOMPARE(settings->loadedDatabaseSource(), QString());
    QCOMPARE(settings->removePluginSettingsOnUninstall(), true);
    QCOMPARE(settings->fetchFavicons(), false); // NEW, default OFF/ask on first run (fixes P8)
    QCOMPARE(settings->appVersion(), QString());
    QCOMPARE(settings->databaseDeferredSaveSeconds(), 5);
}

void AppSettingsTest::testSetGetAndSignals() {
    auto settings = AppSettings::createInMemory();

    // 1. Language
    QSignalSpy spyLanguage(settings.get(), &AppSettings::languageChanged);
    settings->setLanguage("de-DE");
    QCOMPARE(settings->language(), QString("de-DE"));
    QCOMPARE(spyLanguage.count(), 1);
    QCOMPARE(spyLanguage.takeFirst().at(0).toString(), QString("de-DE"));
    settings->setLanguage("de-DE"); // Same value, no emission
    QCOMPARE(spyLanguage.count(), 0);

    // 2. WindowWidth
    QSignalSpy spyWidth(settings.get(), &AppSettings::windowWidthChanged);
    settings->setWindowWidth(1280);
    QCOMPARE(settings->windowWidth(), 1280);
    QCOMPARE(spyWidth.count(), 1);
    QCOMPARE(spyWidth.takeFirst().at(0).toInt(), 1280);
    settings->setWindowWidth(1280);
    QCOMPARE(spyWidth.count(), 0);

    // 3. WindowHeight
    QSignalSpy spyHeight(settings.get(), &AppSettings::windowHeightChanged);
    settings->setWindowHeight(800);
    QCOMPARE(settings->windowHeight(), 800);
    QCOMPARE(spyHeight.count(), 1);
    QCOMPARE(spyHeight.takeFirst().at(0).toInt(), 800);
    settings->setWindowHeight(800);
    QCOMPARE(spyHeight.count(), 0);

    // 4. WindowLeft
    QSignalSpy spyLeft(settings.get(), &AppSettings::windowLeftChanged);
    settings->setWindowLeft(50);
    QCOMPARE(settings->windowLeft(), 50);
    QCOMPARE(spyLeft.count(), 1);
    QCOMPARE(spyLeft.takeFirst().at(0).toInt(), 50);
    settings->setWindowLeft(50);
    QCOMPARE(spyLeft.count(), 0);

    // 5. WindowTop
    QSignalSpy spyTop(settings.get(), &AppSettings::windowTopChanged);
    settings->setWindowTop(60);
    QCOMPARE(settings->windowTop(), 60);
    QCOMPARE(spyTop.count(), 1);
    QCOMPARE(spyTop.takeFirst().at(0).toInt(), 60);
    settings->setWindowTop(60);
    QCOMPARE(spyTop.count(), 0);

    // 6. WindowState
    QSignalSpy spyState(settings.get(), &AppSettings::windowStateChanged);
    settings->setWindowState(0);
    QCOMPARE(settings->windowState(), 0);
    QCOMPARE(spyState.count(), 1);
    QCOMPARE(spyState.takeFirst().at(0).toInt(), 0);
    settings->setWindowState(0);
    QCOMPARE(spyState.count(), 0);

    // 7. PasswordLength
    QSignalSpy spyPassLen(settings.get(), &AppSettings::passwordLengthChanged);
    settings->setPasswordLength(24);
    QCOMPARE(settings->passwordLength(), 24);
    QCOMPARE(spyPassLen.count(), 1);
    QCOMPARE(spyPassLen.takeFirst().at(0).toInt(), 24);
    settings->setPasswordLength(24);
    QCOMPARE(spyPassLen.count(), 0);

    // 8. PasswordChars
    QSignalSpy spyPassChars(settings.get(), &AppSettings::passwordCharsChanged);
    settings->setPasswordChars("abc123!@#");
    QCOMPARE(settings->passwordChars(), QString("abc123!@#"));
    QCOMPARE(spyPassChars.count(), 1);
    QCOMPARE(spyPassChars.takeFirst().at(0).toString(), QString("abc123!@#"));
    settings->setPasswordChars("abc123!@#");
    QCOMPARE(spyPassChars.count(), 0);

    // 9. InactivityLogoutMinutes
    QSignalSpy spyInactivity(settings.get(), &AppSettings::inactivityLogoutMinutesChanged);
    settings->setInactivityLogoutMinutes(10);
    QCOMPARE(settings->inactivityLogoutMinutes(), 10);
    QCOMPARE(spyInactivity.count(), 1);
    QCOMPARE(spyInactivity.takeFirst().at(0).toInt(), 10);
    settings->setInactivityLogoutMinutes(10);
    QCOMPARE(spyInactivity.count(), 0);

    // 10. DoubleClickCopy
    QSignalSpy spyDouble(settings.get(), &AppSettings::doubleClickCopyChanged);
    settings->setDoubleClickCopy(false);
    QCOMPARE(settings->doubleClickCopy(), false);
    QCOMPARE(spyDouble.count(), 1);
    QCOMPARE(spyDouble.takeFirst().at(0).toBool(), false);
    settings->setDoubleClickCopy(false);
    QCOMPARE(spyDouble.count(), 0);

    // 11. ShowIcons
    QSignalSpy spyIcons(settings.get(), &AppSettings::showIconsChanged);
    settings->setShowIcons(false);
    QCOMPARE(settings->showIcons(), false);
    QCOMPARE(spyIcons.count(), 1);
    QCOMPARE(spyIcons.takeFirst().at(0).toBool(), false);
    settings->setShowIcons(false);
    QCOMPARE(spyIcons.count(), 0);

    // 12. VerifyNotLeaked
    QSignalSpy spyLeak(settings.get(), &AppSettings::verifyNotLeakedChanged);
    settings->setVerifyNotLeaked(false);
    QCOMPARE(settings->verifyNotLeaked(), false);
    QCOMPARE(settings->disableLeakedPasswords(), true);
    QCOMPARE(spyLeak.count(), 1);
    QCOMPARE(spyLeak.takeFirst().at(0).toBool(), false);
    settings->setVerifyNotLeaked(false);
    QCOMPARE(spyLeak.count(), 0);

    // 13. SplitterWidth
    QSignalSpy spySplitter(settings.get(), &AppSettings::splitterWidthChanged);
    settings->setSplitterWidth(320);
    QCOMPARE(settings->splitterWidth(), 320);
    QCOMPARE(spySplitter.count(), 1);
    QCOMPARE(spySplitter.takeFirst().at(0).toInt(), 320);
    settings->setSplitterWidth(320);
    QCOMPARE(spySplitter.count(), 0);

    // 14. ClipboardClearSeconds
    QSignalSpy spyClip(settings.get(), &AppSettings::clipboardClearSecondsChanged);
    settings->setClipboardClearSeconds(30);
    QCOMPARE(settings->clipboardClearSeconds(), 30);
    QCOMPARE(spyClip.count(), 1);
    QCOMPARE(spyClip.takeFirst().at(0).toInt(), 30);
    settings->setClipboardClearSeconds(30);
    QCOMPARE(spyClip.count(), 0);

    // 15. DatabasePaths
    QSignalSpy spyDbPaths(settings.get(), &AppSettings::databasePathsChanged);
    QStringList paths = {"/tmp/a.epdb", "/tmp/b.epdb"};
    settings->setDatabasePaths(paths);
    QCOMPARE(settings->databasePaths(), paths);
    QCOMPARE(spyDbPaths.count(), 1);
    settings->setDatabasePaths(paths);
    QCOMPARE(spyDbPaths.count(), 1);

    // 16. LastDatabase
    QSignalSpy spyLastDb(settings.get(), &AppSettings::lastDatabaseChanged);
    settings->setLastDatabase("/tmp/a.epdb");
    QCOMPARE(settings->lastDatabase(), QString("/tmp/a.epdb"));
    QCOMPARE(spyLastDb.count(), 1);
    QCOMPARE(spyLastDb.takeFirst().at(0).toString(), QString("/tmp/a.epdb"));
    settings->setLastDatabase("/tmp/a.epdb");
    QCOMPARE(spyLastDb.count(), 0);

    // 17. RemovePluginSettingsOnUninstall
    QSignalSpy spyPlugin(settings.get(), &AppSettings::removePluginSettingsOnUninstallChanged);
    settings->setRemovePluginSettingsOnUninstall(false);
    QCOMPARE(settings->removePluginSettingsOnUninstall(), false);
    QCOMPARE(spyPlugin.count(), 1);
    QCOMPARE(spyPlugin.takeFirst().at(0).toBool(), false);
    settings->setRemovePluginSettingsOnUninstall(false);
    QCOMPARE(spyPlugin.count(), 0);

    // 18. FetchFavicons
    QSignalSpy spyFavicon(settings.get(), &AppSettings::fetchFaviconsChanged);
    settings->setFetchFavicons(true);
    QCOMPARE(settings->fetchFavicons(), true);
    QCOMPARE(spyFavicon.count(), 1);
    QCOMPARE(spyFavicon.takeFirst().at(0).toBool(), true);
    settings->setFetchFavicons(true);
    QCOMPARE(spyFavicon.count(), 0);

    // 19. AppVersion
    QSignalSpy spyVersion(settings.get(), &AppSettings::appVersionChanged);
    settings->setAppVersion("1.4.0");
    QCOMPARE(settings->appVersion(), QString("1.4.0"));
    QCOMPARE(spyVersion.count(), 1);
    QCOMPARE(spyVersion.takeFirst().at(0).toString(), QString("1.4.0"));
    settings->setAppVersion("1.4.0");
    QCOMPARE(spyVersion.count(), 0);

    // 20. DatabaseDeferredSaveSeconds
    QSignalSpy spyDeferred(settings.get(), &AppSettings::databaseDeferredSaveSecondsChanged);
    settings->setDatabaseDeferredSaveSeconds(10);
    QCOMPARE(settings->databaseDeferredSaveSeconds(), 10);
    QCOMPARE(spyDeferred.count(), 1);
    QCOMPARE(spyDeferred.takeFirst().at(0).toInt(), 10);
    settings->setDatabaseDeferredSaveSeconds(10);
    QCOMPARE(spyDeferred.count(), 0);
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

    // windowWidth (min 200)
    settings->setWindowWidth(150);
    QCOMPARE(settings->windowWidth(), 200);
    settings->setWindowWidth(-10);
    QCOMPARE(settings->windowWidth(), 200);
    settings->setWindowWidth(1920);
    QCOMPARE(settings->windowWidth(), 1920);

    // windowHeight (min 100)
    settings->setWindowHeight(50);
    QCOMPARE(settings->windowHeight(), 100);
    settings->setWindowHeight(-10);
    QCOMPARE(settings->windowHeight(), 100);
    settings->setWindowHeight(1080);
    QCOMPARE(settings->windowHeight(), 1080);

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

    QRect target(100, 150, 1400, 900);
    settings->setWindowGeometry(target);

    QCOMPARE(settings->windowLeft(), 100);
    QCOMPARE(settings->windowTop(), 150);
    QCOMPARE(settings->windowWidth(), 1400);
    QCOMPARE(settings->windowHeight(), 900);
    QCOMPARE(settings->windowGeometry(), target);
    QCOMPARE(spyGeom.count(), 1);

    // Test modifying single dimension also updates windowGeometry and emits signal
    settings->setWindowWidth(1600);
    QCOMPARE(settings->windowGeometry(), QRect(100, 150, 1600, 900));
    QCOMPARE(spyGeom.count(), 2);
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
    }
    {
        auto qStore2 = std::make_shared<QSettingsStore>(iniFile, QSettings::IniFormat);
        AppSettings qs2(qStore2);
        QCOMPARE(qs2.language(), QString("de-DE"));
        QCOMPARE(qs2.splitterWidth(), 400);
        QCOMPARE(qs2.inactivityLogoutMinutes(), 12);
        QCOMPARE(qs2.verifyNotLeaked(), false);
    }
}

void AppSettingsTest::testLegacyKeysMigration() {
    auto store = std::make_shared<InMemorySettingsStore>();

    // Seed legacy keys
    store->setValue("knownDatabases", QStringList{"/legacy/1.epdb", "/legacy/2.epdb"});
    store->setValue("lastUsedDatabase", "/legacy/1.epdb");
    store->setValue("disableLeakedPasswords", true);
    store->setValue("inactivityLogoutTime", 45);
    store->setValue("doubleTapToCopy", false);
    store->setValue("gridSplitterWidth", 350);
    store->setValue("clipboardClearTimeoutSec", 20);

    // Initializing AppSettings triggers migrateLegacyKeys()
    AppSettings s(store);

    // Verify migrated values
    QCOMPARE(s.databasePaths(), (QStringList{"/legacy/1.epdb", "/legacy/2.epdb"}));
    QCOMPARE(s.lastDatabase(), QString("/legacy/1.epdb"));
    QCOMPARE(s.verifyNotLeaked(), false); // disableLeakedPasswords=true -> verifyNotLeaked=false
    QCOMPARE(s.inactivityLogoutMinutes(), 45);
    QCOMPARE(s.doubleClickCopy(), false);
    QCOMPARE(s.splitterWidth(), 350);
    QCOMPARE(s.clipboardClearSeconds(), 20);

    // Verify legacy keys were cleaned up
    QVERIFY(!store->contains("knownDatabases"));
    QVERIFY(!store->contains("lastUsedDatabase"));
    QVERIFY(!store->contains("disableLeakedPasswords"));
    QVERIFY(!store->contains("inactivityLogoutTime"));
    QVERIFY(!store->contains("doubleTapToCopy"));
    QVERIFY(!store->contains("gridSplitterWidth"));
    QVERIFY(!store->contains("clipboardClearTimeoutSec"));

    // Verify new typed keys are stored
    QVERIFY(store->contains("databasePaths"));
    QVERIFY(store->contains("lastDatabase"));
    QVERIFY(store->contains("verifyNotLeaked"));
    QVERIFY(store->contains("inactivityLogoutMinutes"));
    QVERIFY(store->contains("doubleClickCopy"));
    QVERIFY(store->contains("splitterWidth"));
    QVERIFY(store->contains("clipboardClearSeconds"));

    // Also test C# pipe-separated string and loadedDatabaseName
    auto storeCSharp = std::make_shared<InMemorySettingsStore>();
    storeCSharp->setValue("databasePaths", QString("/csharp/1.epdb|/csharp/2.epdb"));
    storeCSharp->setValue("loadedDatabaseName", QString("/csharp/loaded.epdb"));

    AppSettings sCSharp(storeCSharp);
    QCOMPARE(sCSharp.databasePaths(), (QStringList{"/csharp/1.epdb", "/csharp/2.epdb"}));
    QCOMPARE(sCSharp.lastDatabase(), QString("/csharp/loaded.epdb"));
    QVERIFY(!storeCSharp->contains("loadedDatabaseName"));
}

void AppSettingsTest::testResetToDefaults() {
    auto settings = AppSettings::createInMemory();

    // Modify multiple properties
    settings->setLanguage("de-DE");
    settings->setWindowWidth(1920);
    settings->setWindowHeight(1080);
    settings->setPasswordLength(28);
    settings->setPasswordChars("1234567890");
    settings->setInactivityLogoutMinutes(15);
    settings->setDoubleClickCopy(false);
    settings->setShowIcons(false);
    settings->setVerifyNotLeaked(false);
    settings->setSplitterWidth(450);
    settings->setClipboardClearSeconds(60);
    settings->setFetchFavicons(true);
    settings->setLastDatabase("/tmp/custom.epdb");
    settings->setDatabasePaths(QStringList{"/tmp/custom.epdb"});

    QSignalSpy spyLanguage(settings.get(), &AppSettings::languageChanged);
    QSignalSpy spyWidth(settings.get(), &AppSettings::windowWidthChanged);
    QSignalSpy spyLen(settings.get(), &AppSettings::passwordLengthChanged);
    QSignalSpy spyChars(settings.get(), &AppSettings::passwordCharsChanged);
    QSignalSpy spyInactivity(settings.get(), &AppSettings::inactivityLogoutMinutesChanged);
    QSignalSpy spyDouble(settings.get(), &AppSettings::doubleClickCopyChanged);
    QSignalSpy spyIcons(settings.get(), &AppSettings::showIconsChanged);
    QSignalSpy spyLeak(settings.get(), &AppSettings::verifyNotLeakedChanged);
    QSignalSpy spySplitter(settings.get(), &AppSettings::splitterWidthChanged);
    QSignalSpy spyClip(settings.get(), &AppSettings::clipboardClearSecondsChanged);
    QSignalSpy spyFavicon(settings.get(), &AppSettings::fetchFaviconsChanged);
    QSignalSpy spyLastDb(settings.get(), &AppSettings::lastDatabaseChanged);

    settings->resetToDefaults();

    // Verify all properties returned to defaults
    QCOMPARE(settings->language(), QString("en-US"));
    QCOMPARE(settings->windowWidth(), 1100);
    QCOMPARE(settings->windowHeight(), 700);
    QCOMPARE(settings->passwordLength(), 15);
    QCOMPARE(settings->passwordChars(), AppSettings::Defaults::PasswordChars);
    QCOMPARE(settings->inactivityLogoutMinutes(), 3);
    QCOMPARE(settings->doubleClickCopy(), true);
    QCOMPARE(settings->showIcons(), true);
    QCOMPARE(settings->verifyNotLeaked(), true);
    QCOMPARE(settings->splitterWidth(), 270);
    QCOMPARE(settings->clipboardClearSeconds(), 15);
    QCOMPARE(settings->fetchFavicons(), false);
    QCOMPARE(settings->lastDatabase(), QString());

    // Verify change signals were emitted
    QCOMPARE(spyLanguage.count(), 1);
    QCOMPARE(spyWidth.count(), 1);
    QCOMPARE(spyLen.count(), 1);
    QCOMPARE(spyChars.count(), 1);
    QCOMPARE(spyInactivity.count(), 1);
    QCOMPARE(spyDouble.count(), 1);
    QCOMPARE(spyIcons.count(), 1);
    QCOMPARE(spyLeak.count(), 1);
    QCOMPARE(spySplitter.count(), 1);
    QCOMPARE(spyClip.count(), 1);
    QCOMPARE(spyFavicon.count(), 1);
    QCOMPARE(spyLastDb.count(), 1);

    // Calling resetToDefaults() when already at defaults emits no new signals
    settings->resetToDefaults();
    QCOMPARE(spyLanguage.count(), 1);
    QCOMPARE(spyWidth.count(), 1);

    // Test individual reset methods
    settings->setPasswordChars("test_chars");
    QCOMPARE(settings->passwordChars(), QString("test_chars"));
    settings->resetPasswordChars();
    QCOMPARE(settings->passwordChars(), AppSettings::Defaults::PasswordChars);

    settings->setPasswordLength(25);
    QCOMPARE(settings->passwordLength(), 25);
    settings->resetPasswordLength();
    QCOMPARE(settings->passwordLength(), 15);
}

