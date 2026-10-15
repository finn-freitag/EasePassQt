#pragma once

#include <QObject>

class SecureMemoryTest : public QObject {
    Q_OBJECT

private slots:
    void testSecureBufferBasicAllocAndSize();
    void testSecureBufferZeroOnDestructionWithHook();
    void testSecureBufferZeroWithCustomAllocator();
    void testSecureBufferCopySemantics();
    void testSecureBufferMoveSemantics();
    void testSecureBufferResizeAndReserve();
    void testSecureBufferConstantTimeComparison();
    void testSecureBufferAppend();
    void testSecretStringBasicAndZeroOnDestruction();
    void testSecretStringCopyAndMove();
    void testSecretStringComparisonAndConversion();
    void testSecretStringToUtf16LE();
    void testCryptoHelperWithSecureBufferAndSecretString();
    void testDatabaseFileWipeSecrets();
    void testPlatformCoreDumpControl();
    void testFakeCoreDumpControlInjection();
};
