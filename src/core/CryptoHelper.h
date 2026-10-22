#pragma once

#include "SecretString.h"
#include "SecureBuffer.h"

#include <QString>
#include <QByteArray>
#include <cstdint>

namespace EasePass::Core {

/**
 * @brief Argon2id cost parameters used to derive the database keys.
 *
 * The default-constructed value (== production()) is the only one that produces databases
 * compatible with the C# app (C#: Helper/Security/HashHelper.cs: 250 MB, 10 iterations,
 * 10 lanes). Cheaper values exist ONLY so tests can run fast (steps.txt 3.3): they are
 * reachable solely through an explicit function/constructor parameter, there is no global
 * switch, and the parameters are never written into a database file.
 */
struct KdfParams {
    uint32_t timeCost = 10;
    uint32_t memoryCostKiB = 256000;
    uint32_t lanes = 10;

    static constexpr KdfParams production() { return KdfParams{}; }
    bool isProduction() const { return *this == production(); }
    bool operator==(const KdfParams &) const = default;
};

class CryptoHelper {
public:
    static const int CURRENT_DB_VERSION_TAG = 3;
    static constexpr double CURRENT_VERSION = 1.4;

    static QByteArray toUtf16LE(const QString &str);

    static QByteArray customToBase64(const QByteArray &input);
    static SecureBuffer customToBase64(const SecureBuffer &input);

    static SecureBuffer argon2idHash(const SecureBuffer &passwordBytes, const QByteArray &salt,
                                     const QByteArray &associatedData = QByteArray(), uint32_t t_cost = 10,
                                     uint32_t m_cost = 256000, uint32_t lanes = 10, size_t hashLen = 32);

    static QByteArray argon2idHash(const QByteArray &passwordBytes, const QByteArray &salt,
                                   const QByteArray &associatedData = QByteArray(), uint32_t t_cost = 10,
                                   uint32_t m_cost = 256000, uint32_t lanes = 10, size_t hashLen = 32);

    static SecureBuffer deriveOuterKey(const SecretString &password, const KdfParams &kdf = KdfParams::production());

    static SecureBuffer deriveInnerKey(const SecretString &password, const SecretString &secondFactor = SecretString(),
                                       bool useOldAssociatedData = false,
                                       const KdfParams &kdf = KdfParams::production());

    static QByteArray encryptAes(const QByteArray &plaintext, const SecureBuffer &key);
    static QByteArray encryptAes(const QByteArray &plaintext, const QByteArray &key);

    static bool decryptAes(const QByteArray &cipherWithIv, const SecureBuffer &key, QByteArray &outPlaintext);
    static bool decryptAes(const QByteArray &cipherWithIv, const QByteArray &key, QByteArray &outPlaintext);

    static QByteArray addVersionTag(const QByteArray &data, int version = CURRENT_DB_VERSION_TAG);
    static bool extractVersionTag(const QByteArray &fileData, int &outVersion, QByteArray &outData);
};

} // namespace EasePass::Core
