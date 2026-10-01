#include "CryptoHelper.h"

#include <cmath>
#include <cstring>
#include <algorithm>
#include <stdexcept>
#include <argon2.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

namespace EasePass::Core {

QByteArray CryptoHelper::toUtf16LE(const QString& str) {
    QByteArray bytes;
    bytes.reserve(str.size() * 2);
    const char16_t* u16 = reinterpret_cast<const char16_t*>(str.utf16());
    for (qsizetype i = 0; i < str.size(); ++i) {
        uint16_t u = static_cast<uint16_t>(u16[i]);
        bytes.append(static_cast<char>(u & 0xFF));
        bytes.append(static_cast<char>((u >> 8) & 0xFF));
    }
    return bytes;
}

QByteArray CryptoHelper::customToBase64(const QByteArray& input) {
    static const char base64ByteTo[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int nChars = static_cast<int>(std::ceil(static_cast<double>(input.size()) / 3.0)) * 4;
    QByteArray outBase64Chars(nChars, '\0');

    int iByte = 0, iBit = 7, iBase64Bits = 0, iBase64Chars = 0;
    uint8_t base64Byte = 0;

    int length = static_cast<int>(input.size());
    const uint8_t* inPtr = reinterpret_cast<const uint8_t*>(input.constData());

    while (iByte < length) {
        base64Byte = static_cast<uint8_t>(
            (base64Byte << 1) |
            ((inPtr[iByte] & (1 << iBit)) ? 1 : 0)
        );

        iBase64Bits++;
        iBit--;
        if (iBase64Bits % 6 == 0) {
            outBase64Chars[iBase64Chars] = base64ByteTo[base64Byte];
            base64Byte = 0;
            iBase64Bits = 0;
            iBase64Chars++;
        }
        if (iBit < 0) {
            iByte++;
            iBit = 7;
        }
    }

    if (iByte % 3 > 0 || iByte % 3 > 1) {
        outBase64Chars[iBase64Chars] = '=';
        iBase64Chars++;
    }
    return outBase64Chars;
}

QByteArray CryptoHelper::argon2idHash(const QByteArray& passwordBytes,
                                     const QByteArray& salt,
                                     const QByteArray& associatedData,
                                     uint32_t t_cost,
                                     uint32_t m_cost,
                                     uint32_t lanes,
                                     size_t hashLen) {
    QByteArray out(static_cast<int>(hashLen), 0);
    argon2_context context;
    std::memset(&context, 0, sizeof(context));
    context.out = reinterpret_cast<uint8_t*>(out.data());
    context.outlen = static_cast<uint32_t>(hashLen);
    context.pwd = reinterpret_cast<uint8_t*>(const_cast<char*>(passwordBytes.constData()));
    context.pwdlen = static_cast<uint32_t>(passwordBytes.size());
    context.salt = reinterpret_cast<uint8_t*>(const_cast<char*>(salt.constData()));
    context.saltlen = static_cast<uint32_t>(salt.size());
    context.secret = nullptr;
    context.secretlen = 0;
    context.ad = associatedData.isEmpty() ? nullptr : reinterpret_cast<uint8_t*>(const_cast<char*>(associatedData.constData()));
    context.adlen = static_cast<uint32_t>(associatedData.size());
    context.t_cost = t_cost;
    context.m_cost = m_cost;
    context.lanes = lanes;
    context.threads = lanes;
    context.version = ARGON2_VERSION_13;
    context.flags = ARGON2_DEFAULT_FLAGS;

    int res = argon2id_ctx(&context);
    if (res != ARGON2_OK) {
        return QByteArray();
    }
    return out;
}

QByteArray CryptoHelper::deriveOuterKey(const QString& password) {
    QByteArray salt = "EasePassArgonHash";
    QByteArray pwBytes = toUtf16LE(password);
    return argon2idHash(pwBytes, salt);
}

QByteArray CryptoHelper::deriveInnerKey(const QString& password,
                                       const QString& secondFactor,
                                       bool useOldAssociatedData) {
    QByteArray salt = "EasePassArgonHash";
    QByteArray ad = useOldAssociatedData ? "Database_Version_1,4" : "Database_Version_1.4";

    if (!secondFactor.isEmpty()) {
        QByteArray sfBytes = toUtf16LE(secondFactor);
        return argon2idHash(sfBytes, salt, ad);
    } else {
        QByteArray pwBytes = toUtf16LE(password);
        QByteArray b64 = customToBase64(pwBytes);
        std::reverse(b64.begin(), b64.end());
        return argon2idHash(b64, salt, ad);
    }
}

QByteArray CryptoHelper::encryptAes(const QByteArray& plaintext, const QByteArray& key) {
    if (key.size() != 32) {
        return QByteArray();
    }

    uint8_t iv[16];
    if (RAND_bytes(iv, sizeof(iv)) != 1) {
        return QByteArray();
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return QByteArray();

    if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr,
                                reinterpret_cast<const uint8_t*>(key.constData()), iv)) {
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }

    QByteArray cipher;
    cipher.resize(plaintext.size() + 32);
    int outLen1 = 0;
    if (1 != EVP_EncryptUpdate(ctx, reinterpret_cast<uint8_t*>(cipher.data()), &outLen1,
                               reinterpret_cast<const uint8_t*>(plaintext.constData()), plaintext.size())) {
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }

    int outLen2 = 0;
    if (1 != EVP_EncryptFinal_ex(ctx, reinterpret_cast<uint8_t*>(cipher.data()) + outLen1, &outLen2)) {
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }

    EVP_CIPHER_CTX_free(ctx);
    cipher.resize(outLen1 + outLen2);

    QByteArray result;
    result.reserve(16 + cipher.size());
    result.append(reinterpret_cast<const char*>(iv), 16);
    result.append(cipher);
    return result;
}

bool CryptoHelper::decryptAes(const QByteArray& cipherWithIv, const QByteArray& key, QByteArray& outPlaintext) {
    if (cipherWithIv.size() < 16 || key.size() != 32) {
        return false;
    }

    const uint8_t* iv = reinterpret_cast<const uint8_t*>(cipherWithIv.constData());
    const uint8_t* cipher = iv + 16;
    int cipherLen = cipherWithIv.size() - 16;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return false;

    if (1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr,
                                reinterpret_cast<const uint8_t*>(key.constData()), iv)) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    outPlaintext.resize(cipherLen + 16);
    int outLen1 = 0;
    if (1 != EVP_DecryptUpdate(ctx, reinterpret_cast<uint8_t*>(outPlaintext.data()), &outLen1,
                               cipher, cipherLen)) {
        EVP_CIPHER_CTX_free(ctx);
        outPlaintext.clear();
        return false;
    }

    int outLen2 = 0;
    if (1 != EVP_DecryptFinal_ex(ctx, reinterpret_cast<uint8_t*>(outPlaintext.data()) + outLen1, &outLen2)) {
        EVP_CIPHER_CTX_free(ctx);
        outPlaintext.clear();
        return false;
    }

    EVP_CIPHER_CTX_free(ctx);
    outPlaintext.resize(outLen1 + outLen2);
    return true;
}

QByteArray CryptoHelper::addVersionTag(const QByteArray& data, int version) {
    static const char ident[] = "epdbversion";
    QByteArray res;
    res.reserve(11 + 4 + data.size());
    res.append(ident, 11);
    uint32_t v = static_cast<uint32_t>(version);
    res.append(static_cast<char>(v & 0xFF));
    res.append(static_cast<char>((v >> 8) & 0xFF));
    res.append(static_cast<char>((v >> 16) & 0xFF));
    res.append(static_cast<char>((v >> 24) & 0xFF));
    res.append(data);
    return res;
}

bool CryptoHelper::extractVersionTag(const QByteArray& fileData, int& outVersion, QByteArray& outData) {
    static const char ident[] = "epdbversion";
    if (fileData.size() < 15) return false;
    if (std::memcmp(fileData.constData(), ident, 11) != 0) return false;

    const uint8_t* vPtr = reinterpret_cast<const uint8_t*>(fileData.constData() + 11);
    uint32_t v = static_cast<uint32_t>(vPtr[0]) |
                 (static_cast<uint32_t>(vPtr[1]) << 8) |
                 (static_cast<uint32_t>(vPtr[2]) << 16) |
                 (static_cast<uint32_t>(vPtr[3]) << 24);
    outVersion = static_cast<int>(v);
    outData = fileData.mid(15);
    return true;
}

} // namespace EasePass::Core
