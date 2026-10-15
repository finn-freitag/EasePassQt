#pragma once

#include <QObject>

class DatabaseManagerTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();

    void testKnownDatabasePathsAddRemoveDedupe();
    void testLastUsedDatabasePath();
    void testSetAndCloseDatabaseSignals();
};
