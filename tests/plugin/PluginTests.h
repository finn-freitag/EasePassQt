#pragma once

#include <QObject>

// test_plugin: plugin host (steps.txt T26/T27) against the conformance plugins of
// easepassqt-plugin-spec in tests/fixtures/plugins.

/// PluginInspector, PluginInstance and the host functions (no threads, no manager).
class PluginRuntimeTest : public QObject {
    Q_OBJECT
private slots:
    void testInspectConformancePlugins_data();
    void testInspectConformancePlugins();
    void testInspectDoesNotNeedApproval();
    void testForbiddenImportsRejected_data();
    void testForbiddenImportsRejected();
    void testNotAComponentRejected();
    void testHostInterfaceOnlyWhenApproved_data();
    void testHostInterfaceOnlyWhenApproved();
    void testImportFixedItems();
    void testImportCsvThroughFilePicker();
    void testFilePickerCancelledAndTooLarge();
    void testPluginErrorIsReported();
    void testUnknownExportAndArguments();
    void testTrapDoesNotCrashHost();
    void testInfiniteLoopInterrupted();
    void testTimeoutDoesNotAffectOtherPlugin();
    void testMemoryCapHit();
    void testDialogTimeNotCounted();
    void testCancelInterruptsCall();
    void testLogSanitizedAndThrottled();
    void testRandomAndClock();
};

class PluginPackageTest : public QObject {
    Q_OBJECT
private slots:
    void testParseConformancePackage();
    void testStoredDeflateAndIcon();
    void testOtherEntriesIgnored();
    void testBrokenArchivesRejected_data();
    void testBrokenArchivesRejected();
    void testManifestValidation_data();
    void testManifestValidation();
    void testManifestFields();
    void testCompareVersions();
};

class PluginStorageTest : public QObject {
    Q_OBJECT
private slots:
    void testStrings();
    void testFiles();
    void testInvalidNamesRejected_data();
    void testInvalidNamesRejected();
    void testQuota();
    void testIsolationBetweenDirectories();
    void testFilesAreOwnerOnly();
    void testDamagedSettings();
};

class PluginNetworkTest : public QObject {
    Q_OBJECT
private slots:
    void testAllowListMatching_data();
    void testAllowListMatching();
    void testAllowListEntries();
    void testBlockedAddresses_data();
    void testBlockedAddresses();
    void testHttpGetAndPost();
    void testHttpBodyFramings();
    void testHttpStatusPassedThrough();
    void testRedirectOnSameHostFollowed();
    void testRedirectToOtherHostBlocked();
    void testRedirectLoopStops();
    void testHostAndPortNotOnAllowListBlocked();
    void testPrivateAddressBlocked();
    void testResponseTooLarge();
    void testHttpTimeout();
    void testInvalidRequestsRejected_data();
    void testInvalidRequestsRejected();
    void testTcpEcho();
    void testTcpLimitsAndErrors();
    void testNetworkPluginEndToEnd();
};

class PluginManagerTest : public QObject {
    Q_OBJECT
private slots:
    void testInstallFromStore();
    void testHashMismatchAborts();
    void testLocalInstallVerificationStates();
    void testInstallNeedsApproval();
    void testBadPackagesRejected_data();
    void testBadPackagesRejected();
    void testRecordsPersist();
    void testTamperedFileRefusedAtLoad();
    void testLoadChecksApprovedPermissions();
    void testRevokedAfterInstallRefusedAtLoad();
    void testEnableDisable();
    void testUninstallCleanup();
    void testUpdateKeepsStorage();
    void testStorageIsolationBetweenPlugins();
    void testNetworkPolicyFromManifest();
    void testInitializerRuns();
    void testLeakCheckWithService();
};

class PluginAdaptersTest : public QObject {
    Q_OBJECT
private slots:
    void testAbout();
    void testImporter();
    void testImporterFilePickerGetsPluginIdentity();
    void testTrapAndTimeoutAreErrorsAndPluginRecovers();
    void testPasswordGenerator();
    void testStoreFeed();
    void testDatabaseProviderConfig();
    void testDatabaseSource();
    void testDatabaseSourceReadOnly();
    void testDatabaseSourceWithDatabaseService();
    void testUntrustedValuesRejected();
};
