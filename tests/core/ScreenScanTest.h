#pragma once

#include <QObject>

// T9: screen capture geometry, portal parsing, async QR scanning, webcam frame controller.
class ScreenScanTest : public QObject {
    Q_OBJECT

private slots:
    void testLogicalToDeviceDprScales();
    void testUnionGeometryNegativeOrigin();
    void testMapSelectionToImage_data();
    void testMapSelectionToImage();
    void testMapSelectionClipsAndRejects();
    void testComposeScreensMixedDpr();
    void testChooseBackend();
    void testPortalRequestPath();
    void testPortalResponseParsing();
    void testScanDecision();
    void testScanImageAllAsync();
    void testScreenQrScannerFindsCode();
    void testScreenQrScannerNoCode();
    void testScreenQrScannerRegionOnScaledMultiMonitor();
    void testScreenQrScannerCaptureFailureAndCancel();
    void testScreenQrScannerUnavailableBackend();
    void testWebcamControllerDetectsAndThrottles();
};
