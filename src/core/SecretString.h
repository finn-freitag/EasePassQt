#pragma once

#include "SecureBuffer.h"
#include <QString>
#include <string_view>

namespace EasePass::Core {

/**
 * @brief SecretString is a memory-safe wrapper for sensitive text such as master passwords
 *        and second factor tokens.
 *
 * Backed by SecureBuffer, it ensures:
 * - Best-effort memory locking (mlock / VirtualLock).
 * - Automatic cryptographic zeroing upon destruction or clearing.
 * - Constant-time comparisons.
 * - Direct conversion to UTF-16LE without leaking intermediate QStrings into heap memory.
 * - Seamless interop and backwards compatibility with existing QString callers.
 */
class SecretString {
public:
    SecretString() noexcept = default;
    ~SecretString() = default;

    SecretString(const char *utf8Str);
    SecretString(const char *utf8Str, size_t len);
    SecretString(std::string_view sv);
    SecretString(const QString &str);
    explicit SecretString(SecureBuffer buffer) noexcept;

    SecretString(const SecretString &other) = default;
    SecretString(SecretString &&other) noexcept = default;

    SecretString &operator=(const SecretString &other) = default;
    SecretString &operator=(SecretString &&other) noexcept = default;

    SecretString &operator=(const QString &str);
    SecretString &operator=(const char *str);

    bool isEmpty() const noexcept { return m_buffer.isEmpty(); }
    bool empty() const noexcept { return m_buffer.empty(); }
    size_t size() const noexcept { return m_buffer.size(); }
    size_t length() const noexcept { return m_buffer.size(); }
    /// Number of Unicode code points (UTF-8 lead bytes) without creating a QString copy.
    qsizetype codePointCount() const noexcept;

    const char *c_str() const noexcept { return m_buffer.constData(); }
    const char *constData() const noexcept { return m_buffer.constData(); }
    const char *data() const noexcept { return m_buffer.constData(); }

    const SecureBuffer &secureBuffer() const noexcept { return m_buffer; }
    SecureBuffer &secureBuffer() noexcept { return m_buffer; }

    void clear() { m_buffer.clear(); }
    void wipe() { m_buffer.wipe(); }

    QString toQString() const;
    operator QString() const { return toQString(); }

    QByteArray toUtf8() const { return m_buffer.toQByteArray(); }
    SecureBuffer toUtf16LE() const;

    bool operator==(const SecretString &other) const;
    bool operator!=(const SecretString &other) const { return !(*this == other); }

    bool operator==(const QString &other) const;
    bool operator!=(const QString &other) const { return !(*this == other); }

    bool operator==(const char *other) const;
    bool operator!=(const char *other) const { return !(*this == other); }

private:
    SecureBuffer m_buffer;
};

} // namespace EasePass::Core
