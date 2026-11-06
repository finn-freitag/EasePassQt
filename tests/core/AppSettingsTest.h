#pragma once

#include <QObject>

class AppSettingsTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();

    void testDefaults();
    void testSetGetAndSignals();
    void testRangeClamping();
    void testWindowGeometry();
    void testDatabasePathsAddRemoveDedupe();
    void testPersistenceAcrossInstances();
    void testLegacyKeysMigration();
    void testResetToDefaults();
};
