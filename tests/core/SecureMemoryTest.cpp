#include "SecureMemoryTest.h"

#include "SecureBuffer.h"
#include "SecretString.h"
#include "ICoreDumpControl.h"
#include "CryptoHelper.h"
#include "DatabaseFile.h"

#include <QTest>
#include <cstring>
#include <vector>

using namespace EasePass::Core;

void SecureMemoryTest::testSecureBufferBasicAllocAndSize() {
    // Default constructor
    SecureBuffer emptyBuf;
    QVERIFY(emptyBuf.isEmpty());
    QVERIFY(emptyBuf.empty());
    QCOMPARE(emptyBuf.size(), 0);
    QCOMPARE(emptyBuf.length(), 0);
    QCOMPARE(emptyBuf.constData(), "");
    QVERIFY(emptyBuf.data() == nullptr);

    // Sized constructor with fill character
    SecureBuffer sizedBuf(32, 'A');
    QVERIFY(!sizedBuf.isEmpty());
    QCOMPARE(sizedBuf.size(), 32);
    QCOMPARE(sizedBuf.at(0), 'A');
    QCOMPARE(sizedBuf.at(31), 'A');
    QCOMPARE(sizedBuf[0], 'A');
    QCOMPARE(sizedBuf[31], 'A');

    // Null terminator guarantee
    QCOMPARE(sizedBuf.data()[32], '\0');

    // Mutation via operator[]
    sizedBuf[0] = 'Z';
    QCOMPARE(sizedBuf[0], 'Z');

    // Bounds checking
    bool caught = false;
    try {
        (void)sizedBuf.at(32);
    } catch (const std::out_of_range &) {
        caught = true;
    }
    QVERIFY(caught);
}

void SecureMemoryTest::testSecureBufferZeroOnDestructionWithHook() {
    bool hookCalled = false;
    bool allBytesZeroed = true;
    size_t inspectedSize = 0;

    SecureBuffer::setWipeHook([&](const void *ptr, size_t size) {
        hookCalled = true;
        inspectedSize = size;
        const uint8_t *bytes = static_cast<const uint8_t *>(ptr);
        for (size_t i = 0; i < size; ++i) {
            if (bytes[i] != 0) {
                allBytesZeroed = false;
                break;
            }
        }
    });

    {
        SecureBuffer buf("HighlyConfidentialPassword123!");
        QVERIFY(buf.size() > 0);
        QVERIFY(!hookCalled);
    } // buf destructor runs here

    SecureBuffer::setWipeHook(nullptr);

    QVERIFY(hookCalled);
    QVERIFY(inspectedSize > 0);
    QVERIFY(allBytesZeroed);
}

void SecureMemoryTest::testSecureBufferZeroWithCustomAllocator() {
    bool allocCalled = false;
    bool deallocCalled = false;
    bool wasZeroedOnDealloc = true;
    size_t deallocSize = 0;

    SecureBuffer::CustomAllocator alloc;
    alloc.allocate = [&](size_t size) -> void * {
        allocCalled = true;
        void *p = std::malloc(size);
        if (p) {
            // Poison memory with non-zero bytes to prove zeroing later
            std::memset(p, 0x5A, size);
        }
        return p;
    };
    alloc.deallocate = [&](void *ptr, size_t size) {
        deallocCalled = true;
        deallocSize = size;
        const uint8_t *bytes = static_cast<const uint8_t *>(ptr);
        for (size_t i = 0; i < size; ++i) {
            if (bytes[i] != 0) {
                wasZeroedOnDealloc = false;
                break;
            }
        }
        std::free(ptr);
    };

    SecureBuffer::setCustomAllocator(&alloc);

    {
        SecureBuffer secret("SuperSecretKeyFromAllocator");
        QVERIFY(allocCalled);
        QVERIFY(!deallocCalled);
        QCOMPARE(secret.size(), std::strlen("SuperSecretKeyFromAllocator"));
    } // Destructor runs here, cleanses memory, then invokes custom deallocate

    SecureBuffer::setCustomAllocator(nullptr);

    QVERIFY(deallocCalled);
    QVERIFY(deallocSize > 0);
    QVERIFY(wasZeroedOnDealloc);
}

void SecureMemoryTest::testSecureBufferCopySemantics() {
    SecureBuffer original("OriginalSecretKey123");
    SecureBuffer copy(original);

    // Deep copy: identical contents, different buffers
    QCOMPARE(copy.size(), original.size());
    QVERIFY(copy == original);
    QVERIFY(copy.data() != original.data());

    // Modifying the copy does not modify the original
    copy[0] = 'X';
    QVERIFY(copy != original);
    QCOMPARE(original[0], 'O');
    QCOMPARE(copy[0], 'X');

    // Self-assignment
    original = original;
    QCOMPARE(original.size(), std::strlen("OriginalSecretKey123"));
    QCOMPARE(original[0], 'O');

    // Copy assignment
    SecureBuffer assigned;
    assigned = original;
    QCOMPARE(assigned.size(), original.size());
    QVERIFY(assigned == original);
    QVERIFY(assigned.data() != original.data());
}

void SecureMemoryTest::testSecureBufferMoveSemantics() {
    SecureBuffer source("MoveMeSecretPassword!");
    const char *originalDataPtr = source.data();
    size_t originalSize = source.size();

    // Move construction
    SecureBuffer moved(std::move(source));
    QCOMPARE(moved.data(), originalDataPtr);
    QCOMPARE(moved.size(), originalSize);
    QVERIFY(source.isEmpty());
    QVERIFY(source.data() == nullptr);
    QCOMPARE(source.constData(), "");

    // Move assignment
    SecureBuffer target("OldValueToBeOverwrittenAndCleansed");
    target = std::move(moved);
    QCOMPARE(target.data(), originalDataPtr);
    QCOMPARE(target.size(), originalSize);
    QVERIFY(moved.isEmpty());
    QVERIFY(moved.data() == nullptr);
    QCOMPARE(moved.constData(), "");

    // Self move assignment
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
#endif
    target = std::move(target);
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
    QCOMPARE(target.data(), originalDataPtr);
    QCOMPARE(target.size(), originalSize);
}

void SecureMemoryTest::testSecureBufferResizeAndReserve() {
    SecureBuffer buf;
    buf.reserve(64);
    QVERIFY(buf.capacity() >= 64);
    QCOMPARE(buf.size(), 0);

    // Resize grow
    buf.resize(20, 'K');
    QCOMPARE(buf.size(), 20);
    for (size_t i = 0; i < 20; ++i) {
        QCOMPARE(buf[i], 'K');
    }

    // Resize shrink
    buf.resize(10);
    QCOMPARE(buf.size(), 10);
    for (size_t i = 0; i < 10; ++i) {
        QCOMPARE(buf[i], 'K');
    }

    // Wipe in place
    buf.wipe();
    for (size_t i = 0; i < 10; ++i) {
        QCOMPARE(buf[i], '\0');
    }

    // Clear
    buf.clear();
    QVERIFY(buf.isEmpty());
    QCOMPARE(buf.capacity(), 0);
}

void SecureMemoryTest::testSecureBufferConstantTimeComparison() {
    SecureBuffer buf1("MatchingPassword123");
    SecureBuffer buf2("MatchingPassword123");
    SecureBuffer buf3("DifferentPassword456");
    SecureBuffer bufShort("Matching");

    QVERIFY(buf1 == buf2);
    QVERIFY(!(buf1 != buf2));

    QVERIFY(buf1 != buf3);
    QVERIFY(!(buf1 == buf3));

    QVERIFY(buf1 != bufShort);
    QVERIFY(!(buf1 == bufShort));

    // QByteArray comparison
    QByteArray baMatch = "MatchingPassword123";
    QByteArray baDiff = "DifferentPassword456";
    QVERIFY(buf1 == baMatch);
    QVERIFY(buf1 != baDiff);

    // Empty buffers equal
    SecureBuffer empty1;
    SecureBuffer empty2;
    QVERIFY(empty1 == empty2);
}

void SecureMemoryTest::testSecureBufferAppend() {
    SecureBuffer buf;
    buf.append("Part1_", 6);
    buf.append(SecureBuffer("Part2_"));
    buf.append(QByteArray("Part3_"));
    buf.append('X');

    QCOMPARE(buf.size(), 6 + 6 + 6 + 1);
    QCOMPARE(buf.toQByteArray(), QByteArray("Part1_Part2_Part3_X"));

    // += operators
    SecureBuffer buf2;
    buf2 += 'A';
    buf2 += SecureBuffer("BC");
    buf2 += QByteArray("DE");
    QCOMPARE(buf2.toQByteArray(), QByteArray("ABCDE"));
}

void SecureMemoryTest::testSecretStringBasicAndZeroOnDestruction() {
    // Construct from const char*
    SecretString s1("MyMasterPassword");
    QCOMPARE(s1.size(), 16);
    QCOMPARE(s1.length(), 16);
    QVERIFY(!s1.isEmpty());
    QCOMPARE(s1.c_str(), "MyMasterPassword");
    QCOMPARE(s1.toQString(), QString("MyMasterPassword"));
    QCOMPARE(s1.toUtf8(), QByteArray("MyMasterPassword"));

    // Construct from QString
    QString qstr = "AnotherPassword456!";
    SecretString s2(qstr);
    QCOMPARE(s2.toQString(), qstr);

    // Construct from string_view
    std::string_view sv = "ViewSecret";
    SecretString s3(sv);
    QCOMPARE(s3.toQString(), QString("ViewSecret"));

    // Verify zeroing on destruction
    bool hookCalled = false;
    bool allZeroed = true;
    SecureBuffer::setWipeHook([&](const void *ptr, size_t size) {
        hookCalled = true;
        const uint8_t *b = static_cast<const uint8_t *>(ptr);
        for (size_t i = 0; i < size; ++i) {
            if (b[i] != 0) {
                allZeroed = false;
                break;
            }
        }
    });

    {
        SecretString scopedSecret("SecretDestructionCheck");
        QVERIFY(!hookCalled);
    }

    SecureBuffer::setWipeHook(nullptr);
    QVERIFY(hookCalled);
    QVERIFY(allZeroed);
}

void SecureMemoryTest::testSecretStringCopyAndMove() {
    SecretString original("OriginalSecret789");

    // Copy constructor
    SecretString copy(original);
    QVERIFY(copy == original);
    QCOMPARE(copy.c_str(), "OriginalSecret789");

    // Copy assignment
    SecretString assigned;
    assigned = original;
    QVERIFY(assigned == original);

    // Assignment from QString
    assigned = QString("AssignedFromQString");
    QCOMPARE(assigned.toQString(), QString("AssignedFromQString"));

    // Assignment from const char*
    assigned = "AssignedFromCString";
    QCOMPARE(assigned.toQString(), QString("AssignedFromCString"));

    // Move constructor
    SecretString moved(std::move(original));
    QCOMPARE(moved.toQString(), QString("OriginalSecret789"));
    QVERIFY(original.isEmpty());

    // Move assignment
    SecretString movedAssigned;
    movedAssigned = std::move(moved);
    QCOMPARE(movedAssigned.toQString(), QString("OriginalSecret789"));
    QVERIFY(moved.isEmpty());
}

void SecureMemoryTest::testSecretStringComparisonAndConversion() {
    SecretString s1("PasswordXYZ");
    SecretString s2("PasswordXYZ");
    SecretString s3("DifferentXYZ");

    // SecretString == SecretString
    QVERIFY(s1 == s2);
    QVERIFY(!(s1 != s2));
    QVERIFY(s1 != s3);
    QVERIFY(!(s1 == s3));

    // SecretString == QString
    QVERIFY(s1 == QString("PasswordXYZ"));
    QVERIFY(s1 != QString("Wrong"));

    // SecretString == const char*
    QVERIFY(s1 == "PasswordXYZ");
    QVERIFY(s1 != "Wrong");

    // Implicit conversion to QString
    QString implicitQStr = s1;
    QCOMPARE(implicitQStr, QString("PasswordXYZ"));
}

void SecureMemoryTest::testSecretStringToUtf16LE() {
    // ASCII string
    SecretString asciiSecret("Hello");
    SecureBuffer asciiU16 = asciiSecret.toUtf16LE();
    QByteArray expectedAscii = QByteArray::fromHex("480065006c006c006f00");
    QCOMPARE(asciiU16.toQByteArray(), expectedAscii);

    // Unicode string with German umlaut and non-BMP emoji
    // 'Über' + '🔑' (U+1F511: surrogate pair D83D DD11 -> 3D D8 11 DD)
    SecretString unicodeSecret("Über🔑");
    SecureBuffer unicodeU16 = unicodeSecret.toUtf16LE();

    // Verify against QString::utf16()
    QString qstr = "Über🔑";
    QByteArray expectedBytes;
    const char16_t *u16 = reinterpret_cast<const char16_t *>(qstr.utf16());
    for (qsizetype i = 0; i < qstr.size(); ++i) {
        uint16_t u = static_cast<uint16_t>(u16[i]);
        expectedBytes.append(static_cast<char>(u & 0xFF));
        expectedBytes.append(static_cast<char>((u >> 8) & 0xFF));
    }
    QCOMPARE(unicodeU16.toQByteArray(), expectedBytes);

    // Empty string
    SecretString emptySecret;
    QVERIFY(emptySecret.toUtf16LE().isEmpty());
}

void SecureMemoryTest::testCryptoHelperWithSecureBufferAndSecretString() {
    // Test argon2idHash with SecureBuffer directly using low test parameters
    SecureBuffer pw("TestSecretArgon2");
    QByteArray salt = "TestSaltValue123";
    QByteArray ad = "AssociatedData";

    SecureBuffer hash = CryptoHelper::argon2idHash(pw, salt, ad, 1, 1024, 1, 32);
    QCOMPARE(hash.size(), 32);
    QVERIFY(!hash.isEmpty());

    // Result should match QByteArray overload
    QByteArray hashBa = CryptoHelper::argon2idHash(pw.toQByteArray(), salt, ad, 1, 1024, 1, 32);
    QCOMPARE(hash.toQByteArray(), hashBa);

    // AES encryption and decryption roundtrip using SecureBuffer key
    SecureBuffer aesKey(32, 0);
    for (size_t i = 0; i < 32; ++i) {
        aesKey[i] = static_cast<char>(i * 13 + 5);
    }

    QByteArray plaintext = "ConfidentialDatabasePayload";
    QByteArray cipher = CryptoHelper::encryptAes(plaintext, aesKey);
    QVERIFY(!cipher.isEmpty());

    QByteArray decrypted;
    QVERIFY(CryptoHelper::decryptAes(cipher, aesKey, decrypted));
    QCOMPARE(decrypted, plaintext);

    // CustomToBase64 with SecureBuffer
    SecureBuffer input3("ABC");
    SecureBuffer out3 = CryptoHelper::customToBase64(input3);
    QCOMPARE(out3.toQByteArray(), QByteArray("QUJD"));
}

void SecureMemoryTest::testDatabaseFileWipeSecrets() {
    DatabaseFile db;
    db.masterPassword = "TopSecretMasterPassword123!";
    db.secondFactor = "SecondFactorAuthenticatorToken!";

    QVERIFY(!db.masterPassword.isEmpty());
    QVERIFY(!db.secondFactor.isEmpty());

    // Wipe secrets
    db.wipeSecrets();
    QVERIFY(db.masterPassword.isEmpty());
    QVERIFY(db.secondFactor.isEmpty());
}

void SecureMemoryTest::testPlatformCoreDumpControl() {
    PlatformCoreDumpControl ctrl;
    bool success = ctrl.disableCoreDumps();
    QVERIFY(success);

#if defined(__linux__)
    // On Linux, after disableCoreDumps(), the process must not be dumpable
    QVERIFY(!ctrl.isDumpable());
#endif
}

class FakeCoreDumpControl : public ICoreDumpControl {
public:
    bool disableCoreDumps() override {
        m_dumpable = false;
        return true;
    }
    bool isDumpable() const override { return m_dumpable; }
    bool m_dumpable = true;
};

void SecureMemoryTest::testFakeCoreDumpControlInjection() {
    FakeCoreDumpControl fake;
    QVERIFY(fake.isDumpable());
    QVERIFY(fake.disableCoreDumps());
    QVERIFY(!fake.isDumpable());
}
