#pragma once

#include <QObject>

/// T26: proof-of-concept tests of the Wasmtime host against the conformance plugins of
/// easepassqt-plugin-spec (tests/fixtures/plugins, see the README there).
class PluginPocTest : public QObject {
    Q_OBJECT
private slots:
    void testInspectConformancePlugins_data();
    void testInspectConformancePlugins();
    void testInspectDoesNotNeedApproval();
    void testForbiddenImportsRejected_data();
    void testForbiddenImportsRejected();
    void testNotAComponentRejected();
    void testUnapprovedImportFailsToInstantiate();
    void testSources();
    void testImportFixedItems();
    void testImportCsvThroughFilePicker();
    void testFilePickerCancelled();
    void testPluginErrorIsReported();
    void testTrapDoesNotCrashHost();
    void testInfiniteLoopInterrupted();
    void testTimeoutDoesNotAffectOtherPlugin();
    void testMemoryCapHit();
    void testDialogTimeNotCounted();
};
