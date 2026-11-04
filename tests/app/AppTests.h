#pragma once

#include <QObject>

class AppControllerTest : public QObject {
    Q_OBJECT
private slots:
    void testNavigationNeedsOpenDatabase();
    void testOpenAndLockResetNavigation();
    void testGoBackSkipsUnavailablePages();
};

class DatabaseControllerTest : public QObject {
    Q_OBJECT
private slots:
    void testOpenSuccessBusyFlags();
    void testOpenWrongPasswordError();
    void testOpenNeedsSecondFactor();
    void testOpenEmptyPathError();
    void testLockAndSaveErrors();
    void testCreateDatabaseValidationAndSuccess();
};

class PasswordModelsTest : public QObject {
    Q_OBJECT
private slots:
    void testListModelRolesAndNoSecrets();
    void testListModelResetsOnChanges();
    void testFilterFreeTextAndTags();
    void testTagSuggestions();
    void testSortModesAndOrder();
    void testSourceIndexMapping();
};

class ItemViewModelTest : public QObject {
    Q_OBJECT
private slots:
    void testLoadEditCancel();
    void testCommitUpdatesService();
    void testNewItemCommitAndValidation();
    void testTotpValidationErrors();
    void testDeleteItem();
    void testSafetyChecksAndReuse();
    void testLeakCheckStates();
    void testLeakResultForStalePasswordIgnored();
    void testTotpSharedTimerTicks();
    void testApplyOtpauthUri();
    void testCopyActionsUseClipboardAndRegisterClick();
    void testUnloadOnLock();
};

class AppServicesTest : public QObject {
    Q_OBJECT
private slots:
    void testTotpTimerRefCount();
    void testDialogServiceRequestResponse();
    void testDialogServiceCancelAll();
    void testClipboardService();
    void testSettingsViewModel();
    void testPrintViewModelExportPdf();
    void testScreenScanViewModelFound();
    void testScreenScanViewModelFailureAndClear();
    void testAppContextQmlRegistration();
};
