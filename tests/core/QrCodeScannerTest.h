#pragma once

#include <QObject>

class QrCodeScannerTest : public QObject {
    Q_OBJECT

private slots:
    void testDecodeSampleQrNormal();
    void testDecodeSampleQrRotated();
    void testDecodeSampleQrInverted();
    void testDecodeSampleQrNoisy();
    void testNonQrImageReturnsEmpty();
    void testNullImageAndMissingFileReturnsEmpty();
    void testClipboardOtpauthText();
    void testClipboardImageScanning();
    void testGenerateRoundTrip();
    void testGenerateEmptyAndScale();
};
