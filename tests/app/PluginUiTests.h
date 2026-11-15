#pragma once
// Plugin UI tests of test_app (steps.txt T28); only built with the plugin host.

#include <QObject>

class PluginsViewModelTest : public QObject {
    Q_OBJECT
private slots:
    void testInstallFromFileVerified();
    void testInstallFromFileUnverified();
    void testInstallFromFileOffline();
    void testInstallFromFileRevoked();
    void testInstallFromFileProblems();
    void testInstallFromStore();
    void testStoreHashMismatchAborts();
    void testStoreFromCachedListIsNotVerified();
    void testOsFilteringAndToggle();
    void testOsWarningForLocalFile();
    void testDialogShowsComputedPermissions();
    void testRiskTexts();
    void testInstalledList();
    void testUninstallFlow();
    void testRevocationDisablesInstalledPlugin();
    void testUpdateFromStore();
    void testStoreFeedPluginAddsCatalog();
    void testImportThroughPlugin();
    void testImportWithFileDialog();
    void testProviderConfigForm();
    void testConfigFieldsFromSchema();
    void testAppContextOwnsPlugins();
};
