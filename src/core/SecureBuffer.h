#pragma once

#include <QByteArray>
#include <cstddef>
#include <cstdint>
#include <functional>

namespace EasePass::Core {

/**
 * @brief SecureBuffer is a byte buffer designed to hold cryptographic keys and sensitive data.
 *
 * Features:
 * - Best-effort memory locking (mlock on Linux/POSIX, VirtualLock on Windows) to prevent
 *   paging sensitive data to swap/disk.
 * - Automatic and secure zeroing of all memory on destruction, clearing, resizing, or reallocation
 *   using OPENSSL_cleanse (compiler will not optimize away).
 * - QByteArray-like interface and interoperability.
 * - Constant-time comparison to prevent side-channel timing attacks.
 * - Test hooks for custom allocators and wipe inspection.
 */
class SecureBuffer {
public:
    struct CustomAllocator {
        std::function<void *(size_t size)> allocate;
        std::function<void(void *ptr, size_t size)> deallocate;
    };
    using WipeHook = std::function<void(const void *ptr, size_t size)>;

    // Global test hooks
    static void setWipeHook(WipeHook hook);
    static void setCustomAllocator(const CustomAllocator *alloc);

    SecureBuffer() noexcept;
    explicit SecureBuffer(size_t size, char fill = 0);
    SecureBuffer(const void *data, size_t size);
    SecureBuffer(const char *str);
    SecureBuffer(const QByteArray &ba);

    SecureBuffer(const SecureBuffer &other);
    SecureBuffer(SecureBuffer &&other) noexcept;

    SecureBuffer &operator=(const SecureBuffer &other);
    SecureBuffer &operator=(SecureBuffer &&other) noexcept;
    SecureBuffer &operator=(const QByteArray &ba);
    SecureBuffer &operator=(const char *str);

    ~SecureBuffer();

    size_t size() const noexcept { return m_size; }
    size_t length() const noexcept { return m_size; }
    size_t capacity() const noexcept { return m_capacity; }
    bool isEmpty() const noexcept { return m_size == 0; }
    bool empty() const noexcept { return m_size == 0; }

    char *data() noexcept { return m_data; }
    const char *data() const noexcept { return m_data ? m_data : ""; }
    const char *constData() const noexcept { return m_data ? m_data : ""; }

    char at(size_t index) const;
    char &operator[](size_t index) noexcept;
    const char &operator[](size_t index) const noexcept;

    void clear();
    void wipe();
    void resize(size_t newSize, char fill = 0);
    void reserve(size_t newCapacity);

    void append(const char *data, size_t len);
    void append(const SecureBuffer &other);
    void append(const QByteArray &ba);
    void append(char c);

    SecureBuffer &operator+=(char c);
    SecureBuffer &operator+=(const SecureBuffer &other);
    SecureBuffer &operator+=(const QByteArray &other);

    QByteArray toQByteArray() const;
    operator QByteArray() const { return toQByteArray(); }

    bool operator==(const SecureBuffer &other) const;
    bool operator!=(const SecureBuffer &other) const { return !(*this == other); }

    bool operator==(const QByteArray &other) const;
    bool operator!=(const QByteArray &other) const { return !(*this == other); }

    char *begin() noexcept { return m_data; }
    char *end() noexcept { return m_data + m_size; }
    const char *begin() const noexcept { return m_data; }
    const char *end() const noexcept { return m_data + m_size; }
    const char *cbegin() const noexcept { return m_data; }
    const char *cend() const noexcept { return m_data + m_size; }

private:
    static char *allocateBuffer(size_t cap);
    static void deallocateBuffer(char *ptr, size_t cap);

    char *m_data = nullptr;
    size_t m_size = 0;
    size_t m_capacity = 0;
};

} // namespace EasePass::Core
