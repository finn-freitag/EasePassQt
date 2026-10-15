#include "DatabaseManagerTest.h"
#include "DatabaseManager.h"
#include <QTest>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QSettings>
#include <QTemporaryDir>
#include <QFile>

using namespace EasePass::Core;

namespace {
std::unique_ptr<QTemporaryDir> s_tempDir;
}

void DatabaseManagerTest::initTestCase() {
    QStandardPaths::setTestModeEnabled(true);
    s_tempDir = std::make_unique<QTemporaryDir>();
    QVERIFY(s_tempDir->isValid());
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, s_tempDir->path());
    QSettings::setDefaultFormat(QSettings::IniFormat);
}

void DatabaseManagerTest::cleanup() {
    QSettings settings("EasePass", "EasePassQt");
    settings.clear();
    DatabaseManager::instance().closeDatabase();
}

void DatabaseManagerTest::testKnownDatabasePathsAddRemoveDedupe() {
    DatabaseManager &mgr = DatabaseManager::instance();

    QString path1 = "/tmp/test1.epdb";
    QString path2 = "/tmp/test2.epdb";

    mgr.addKnownDatabasePath(path1);
    QStringList known = mgr.getKnownDatabasePaths();
    QVERIFY(known.contains(path1));

    // Dedupe: adding same path again must not duplicate it
    mgr.addKnownDatabasePath(path1);
    known = mgr.getKnownDatabasePaths();
    QCOMPARE(known.count(path1), 1);

    // Empty path should be ignored
    int countBefore = known.size();
    mgr.addKnownDatabasePath("");
    QCOMPARE(mgr.getKnownDatabasePaths().size(), countBefore);

    // Add path2
    mgr.addKnownDatabasePath(path2);
    QVERIFY(mgr.getKnownDatabasePaths().contains(path2));

    // Remove path1
    mgr.removeKnownDatabasePath(path1);
    known = mgr.getKnownDatabasePaths();
    QVERIFY(!known.contains(path1));
    QVERIFY(known.contains(path2));
}

void DatabaseManagerTest::testLastUsedDatabasePath() {
    DatabaseManager &mgr = DatabaseManager::instance();

    // Create an existing temporary file for last used path
    QString existingFile = s_tempDir->filePath("existing_db.epdb");
    QFile f(existingFile);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.close();

    mgr.setLastUsedDatabasePath(existingFile);
    QCOMPARE(mgr.getLastUsedDatabasePath(), existingFile);

    // Nonexistent file falls back to default database path if default exists, or returns stored
    mgr.setLastUsedDatabasePath("/nonexistent/file.epdb");
    QString res = mgr.getLastUsedDatabasePath();
    // Either default path or stored path if default doesn't exist
    QVERIFY(!res.isEmpty());
}

void DatabaseManagerTest::testSetAndCloseDatabaseSignals() {
    DatabaseManager &mgr = DatabaseManager::instance();
    QVERIFY(!mgr.isDatabaseLoaded());
    QVERIFY(mgr.currentDatabase() == nullptr);

    QSignalSpy spyLoaded(&mgr, &DatabaseManager::databaseLoaded);
    QSignalSpy spyClosed(&mgr, &DatabaseManager::databaseClosed);

    auto db = std::make_unique<DatabaseFile>();
    db->filePath = s_tempDir->filePath("loaded_db.epdb");
    db->masterPassword = "pwd";

    // Create file so it counts as valid path
    QFile f(db->filePath);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.close();

    mgr.setDatabase(std::move(db));
    QCOMPARE(spyLoaded.count(), 1);
    QVERIFY(mgr.isDatabaseLoaded());
    QVERIFY(mgr.currentDatabase() != nullptr);
    QCOMPARE(mgr.currentDatabase()->filePath, s_tempDir->filePath("loaded_db.epdb"));

    // Setting database also adds it to known paths and sets last used
    QVERIFY(mgr.getKnownDatabasePaths().contains(s_tempDir->filePath("loaded_db.epdb")));
    QCOMPARE(mgr.getLastUsedDatabasePath(), s_tempDir->filePath("loaded_db.epdb"));

    // Close database
    mgr.closeDatabase();
    QCOMPARE(spyClosed.count(), 1);
    QVERIFY(!mgr.isDatabaseLoaded());
    QVERIFY(mgr.currentDatabase() == nullptr);
}
