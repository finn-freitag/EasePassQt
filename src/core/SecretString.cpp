#include "SecretString.h"

#include <cstring>
#include <openssl/crypto.h>

namespace EasePass::Core {

SecretString::SecretString(const char *utf8Str) : SecretString(utf8Str, utf8Str ? std::strlen(utf8Str) : 0) {}

SecretString::SecretString(const char *utf8Str, size_t len) {
    if (utf8Str && len > 0) {
        m_buffer = SecureBuffer(utf8Str, len);
    }
}

SecretString::SecretString(std::string_view sv) : SecretString(sv.data(), sv.size()) {}

SecretString::SecretString(const QString &str) {
    if (!str.isEmpty()) {
        QByteArray utf8 = str.toUtf8();
        m_buffer = SecureBuffer(utf8.constData(), static_cast<size_t>(utf8.size()));
        OPENSSL_cleanse(utf8.data(), static_cast<size_t>(utf8.size()));
    }
}

SecretString::SecretString(SecureBuffer buffer) noexcept : m_buffer(std::move(buffer)) {}

SecretString &SecretString::operator=(const QString &str) {
    if (str.isEmpty()) {
        clear();
    } else {
        QByteArray utf8 = str.toUtf8();
        m_buffer = SecureBuffer(utf8.constData(), static_cast<size_t>(utf8.size()));
        OPENSSL_cleanse(utf8.data(), static_cast<size_t>(utf8.size()));
    }
    return *this;
}

SecretString &SecretString::operator=(const char *str) {
    if (!str || *str == '\0') {
        clear();
    } else {
        m_buffer = SecureBuffer(str);
    }
    return *this;
}

qsizetype SecretString::codePointCount() const noexcept {
    qsizetype count = 0;
    const auto *p = reinterpret_cast<const unsigned char *>(m_buffer.constData());
    for (size_t i = 0; i < m_buffer.size(); ++i) {
        if ((p[i] & 0xC0) != 0x80) { // skip UTF-8 continuation bytes
            ++count;
        }
    }
    return count;
}

QString SecretString::toQString() const {
    if (m_buffer.isEmpty())
        return QString();
    return QString::fromUtf8(m_buffer.constData(), static_cast<qsizetype>(m_buffer.size()));
}

SecureBuffer SecretString::toUtf16LE() const {
    if (m_buffer.isEmpty()) {
        return SecureBuffer();
    }

    const uint8_t *p = reinterpret_cast<const uint8_t *>(m_buffer.constData());
    size_t len = m_buffer.size();
    SecureBuffer bytes;
    bytes.reserve(len * 2);

    size_t i = 0;
    while (i < len) {
        uint32_t cp = 0;
        uint8_t c = p[i++];
        if (c < 0x80) {
            cp = c;
        } else if ((c & 0xE0) == 0xC0) {
            if (i < len) {
                cp = ((c & 0x1F) << 6) | (p[i++] & 0x3F);
            }
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 1 < len) {
                cp = ((c & 0x0F) << 12) | ((p[i] & 0x3F) << 6) | (p[i + 1] & 0x3F);
                i += 2;
            }
        } else if ((c & 0xF8) == 0xF0) {
            if (i + 2 < len) {
                cp = ((c & 0x07) << 18) | ((p[i] & 0x3F) << 12) | ((p[i + 1] & 0x3F) << 6) | (p[i + 2] & 0x3F);
                i += 3;
            }
        }

        if (cp <= 0xFFFF) {
            uint16_t u = static_cast<uint16_t>(cp);
            char b1 = static_cast<char>(u & 0xFF);
            char b2 = static_cast<char>((u >> 8) & 0xFF);
            bytes.append(&b1, 1);
            bytes.append(&b2, 1);
        } else {
            // Surrogate pair for codepoints above 0xFFFF
            cp -= 0x10000;
            uint16_t high = static_cast<uint16_t>(0xD800 + (cp >> 10));
            uint16_t low = static_cast<uint16_t>(0xDC00 + (cp & 0x3FF));
            char h1 = static_cast<char>(high & 0xFF);
            char h2 = static_cast<char>((high >> 8) & 0xFF);
            char l1 = static_cast<char>(low & 0xFF);
            char l2 = static_cast<char>((low >> 8) & 0xFF);
            bytes.append(&h1, 1);
            bytes.append(&h2, 1);
            bytes.append(&l1, 1);
            bytes.append(&l2, 1);
        }
    }
    return bytes;
}

bool SecretString::operator==(const SecretString &other) const {
    return m_buffer == other.m_buffer;
}

bool SecretString::operator==(const QString &other) const {
    SecretString temp(other);
    return *this == temp;
}

bool SecretString::operator==(const char *other) const {
    if (!other) {
        return isEmpty();
    }
    SecretString temp(other);
    return *this == temp;
}

} // namespace EasePass::Core
