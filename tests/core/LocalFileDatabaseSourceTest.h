#pragma once

#include <QObject>

class LocalFileDatabaseSourceTest : public QObject {
    Q_OBJECT

private slots:
    void testNameDescriptionAndAvailability();
    void testGetBytesMissingAndExisting();
    void testSaveCreatesFileAndBackup();
    void testAtomicSaveKeepsOldFileWhenWriteFails();
    void testAtomicSaveFailsWhenDirectoryMissing();
    void testForcedReadOnlyRejectsSave();
    void testReadOnlyFileIsReadOnly();
    void testLastModifiedAndChangedSignal();
};
