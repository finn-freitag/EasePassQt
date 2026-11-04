#pragma once

#include <QObject>

/// T6: database management operations (DatabaseOperations + DatabaseService management API).
class DatabaseOperationsTest : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    // Helpers / validation
    void testSecondFactorTypesAndTokenGenerator();
    void testSanitizeAndBuildDatabasePath();
    void testValidateNewPasswordAndCreate();

    // File operations
    void testCreateDatabase();
    void testCreateDatabaseWithSecondFactor();
    void testCreateDatabaseNeverOverwrites();
    void testRenameDatabase();
    void testRenameRejectsLoadedAndExisting();
    void testDeleteDatabaseRemovesFromKnownList();
    void testDeleteRejectsLastAndLoaded();
    void testImportDatabaseFile();
    void testReadItemsFromFileWrongPassword();

    // Operations on the open database
    void testChangeMasterPassword();
    void testChangeMasterPasswordValidation();
    void testChangeMasterPasswordSaveFailureRestoresOld();
    void testSetAndRemoveSecondFactor();
    void testSecondFactorUnsupportedAndInvalid();
    void testRotateOneTimeSecondFactor();
    void testImportPasswordsMergeRules();
    void testImportPasswordsFromFile();
    void testExportDatabaseCopy();
    void testExportWithNewPasswordOpensWithNewPasswordOnly();
    void testExportSelectedItemsAndBackup();
    void testExportRejectsOpenDatabaseFile();
    void testForceSave();
    void testTemporaryDatabaseIsReadOnly();
    void testOperationsWithoutOpenDatabase();
};
