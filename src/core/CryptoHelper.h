#pragma once

#include <QString>
#include <QByteArray>
#include <cstdint>

namespace EasePass::Core {

class CryptoHelper {
public:
    static const int CURRENT_DB_VERSION_TAG = 3;
    static constexpr double CURRENT_VERSION = 1.4;

    static QByteArray toUtf16LE(const QString& str);
    static QByteArray customToBase64(const QByteArray& input);

    static QByteArray argon2idHash(const QByteArray& passwordBytes,
                                   const QByteArray& salt,
                                   const QByteArray& associatedData = QByteArray(),
                                   uint32_t t_cost = 10,
                                   uint32_t m_cost = 256000,
                                   uint32_t lanes = 10,
                                   size_t hashLen = 32);

    static QByteArray deriveOuterKey(const QString& password);
    static QByteArray deriveInnerKey(const QString& password,
                                     const QString& secondFactor = QString(),
                                     bool useOldAssociatedData = false);

    static QByteArray encryptAes(const QByteArray& plaintext, const QByteArray& key);
    static bool decryptAes(const QByteArray& cipherWithIv, const QByteArray& key, QByteArray& outPlaintext);

    static QByteArray addVersionTag(const QByteArray& data, int version = CURRENT_DB_VERSION_TAG);
    static bool extractVersionTag(const QByteArray& fileData, int& outVersion, QByteArray& outData);
};

} // namespace EasePass::Core
