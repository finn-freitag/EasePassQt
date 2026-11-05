#pragma once

#include <QObject>

class AppControllerTest : public QObject {
    Q_OBJECT
private slots:
    void testNavigationNeedsOpenDatabase();
    void testOpenAndLockResetNavigation();
    void testGoBackSkipsUnavailablePages();
    void testStartPageFirstRun();
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
    void testCreateAndOpen();
    void testImportDatabase();
    void testOneTimeSecondFactorRotatedAfterLogin();
    void testKnownDatabasesAndFirstRun();
    void testPathHelpers();
};

class DailyTipServiceTest : public QObject {
    Q_OBJECT
private slots:
    void testParseTips();
    void testTipStableForADayAndChanges();
    void testLanguageFallbackAndMissingFiles();
    void testBundledTipFiles();
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
    void testInPlaceEditKeepsRows();
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
    void testCopyActionsUseClipboardAndKeepClicks();
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

class PasswordActionsTest : public QObject {
    Q_OBJECT
private slots:
    void testCopyActions();
    void testTotpCode();
    void testRegisterClickAndReadOnly();
    void testDeleteItems();
    void testExportItems();
    void testExportWithNewPassword();
    void testWebsiteIconService();
};

class ItemDialogsTest : public QObject {
    Q_OBJECT
private slots:
    void testEditorTagsTextAndEmailSuggestions();
    void testPasswordReused();
    void testTwoFactorHelpers();
    void testDetailReloadsAfterEditorCommit();
    void testGeneratorRespectsSettings();
    void testGeneratorLeakRetries();
    void testGeneratorAsyncAndTypedPasswordCancels();
    void testQrImportFileAndClipboard();
    void testWebcamScanViewModel();
};

class DatabaseManagementTest : public QObject {
    Q_OBJECT
private slots:
    void testDatabaseListStates();
    void testDeleteNeedsTypedName();
    void testRenameAndCopyPath();
    void testChangePasswordAndSecondFactor();
    void testExportSelectionAndNewPassword();
    void testImportIntoSelectedItems();
    void testSwitchToAndMakePermanent();
};
