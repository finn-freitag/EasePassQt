#pragma once

#include <QObject>

class DatabaseServiceTest : public QObject {
    Q_OBJECT

private slots:
    // Fast tests (test-only KDF parameters)
    void testSerializeLoadFromBytesRoundtrip();
    void testOpenSuccessIsAsync();
    void testOpenWrongPasswordAsync();
    void testOpenNeedsSecondFactor();
    void testOpenAvailabilityHandling();
    void testOpenWhileOpeningRejected();
    void testOpenNullSourceRejected();
    void testMutationSchedulesDeferredSave();
    void testConcurrentEditsCoalesceIntoOneSave();
    void testEditsDuringRunningSaveAreSavedAfterwards();
    void testFlushOnCloseThenLogout();
    void testReadOnlyRejectsMutation();
    void testSourceBecomingReadOnly();
    void testSaveFailureReportedAndRetried();
    void testLockWipesSecrets();
    void testCloseWhileOpeningDiscardsResult();
    void testDestructorFlushesPendingSave();
    void testRegisterClickPersistedOnClose();
    void testLocalFileSourceEndToEnd();

    // Slow test (production Argon2 on the C# fixture)
    void testOpenCSharpFixtureThroughService();
};
