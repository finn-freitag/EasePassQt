#include "SecureBuffer.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <openssl/crypto.h>

#if defined(__unix__) || defined(__linux__) || defined(__APPLE__)
#include <sys/mman.h>
#define EASEPASS_HAS_MLOCK 1
#elif defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX // windows.h must not define min/max macros (they break std::max)
#endif
#include <windows.h>
#define EASEPASS_HAS_VIRTUALLOCK 1
#endif

namespace EasePass::Core {

namespace {

SecureBuffer::WipeHook s_wipeHook = nullptr;
const SecureBuffer::CustomAllocator *s_customAllocator = nullptr;

void lockMemory(void *ptr, size_t size) {
    if (!ptr || size == 0)
        return;
#if defined(EASEPASS_HAS_MLOCK)
    ::mlock(ptr, size); // Best effort, ignore return value
#elif defined(EASEPASS_HAS_VIRTUALLOCK)
    ::VirtualLock(ptr, size); // Best effort, ignore return value
#else
    (void)ptr;
    (void)size;
#endif
}

void unlockMemory(void *ptr, size_t size) {
    if (!ptr || size == 0)
        return;
#if defined(EASEPASS_HAS_MLOCK)
    ::munlock(ptr, size);
#elif defined(EASEPASS_HAS_VIRTUALLOCK)
    ::VirtualUnlock(ptr, size);
#else
    (void)ptr;
    (void)size;
#endif
}

} // namespace

void SecureBuffer::setWipeHook(WipeHook hook) {
    s_wipeHook = std::move(hook);
}

void SecureBuffer::setCustomAllocator(const CustomAllocator *alloc) {
    s_customAllocator = alloc;
}

char *SecureBuffer::allocateBuffer(size_t cap) {
    if (cap == 0)
        return nullptr;
    // Allocate cap + 1 bytes to guarantee null-terminator for string convenience
    size_t allocSize = cap + 1;
    char *ptr = nullptr;
    if (s_customAllocator && s_customAllocator->allocate) {
        ptr = static_cast<char *>(s_customAllocator->allocate(allocSize));
    } else {
        ptr = static_cast<char *>(std::malloc(allocSize));
    }
    if (ptr) {
        lockMemory(ptr, allocSize);
        std::memset(ptr, 0, allocSize);
    }
    return ptr;
}

void SecureBuffer::deallocateBuffer(char *ptr, size_t cap) {
    if (!ptr)
        return;
    size_t allocSize = cap + 1;
    OPENSSL_cleanse(ptr, allocSize);
    if (s_wipeHook) {
        s_wipeHook(ptr, allocSize);
    }
    unlockMemory(ptr, allocSize);
    if (s_customAllocator && s_customAllocator->deallocate) {
        s_customAllocator->deallocate(ptr, allocSize);
    } else {
        std::free(ptr);
    }
}

SecureBuffer::SecureBuffer() noexcept : m_data(nullptr), m_size(0), m_capacity(0) {}

SecureBuffer::SecureBuffer(size_t size, char fill) : m_data(nullptr), m_size(0), m_capacity(0) {
    if (size > 0) {
        m_capacity = size;
        m_data = allocateBuffer(m_capacity);
        if (m_data) {
            m_size = size;
            std::memset(m_data, fill, m_size);
            m_data[m_size] = '\0';
        }
    }
}

SecureBuffer::SecureBuffer(const void *data, size_t size) : m_data(nullptr), m_size(0), m_capacity(0) {
    if (data && size > 0) {
        m_capacity = size;
        m_data = allocateBuffer(m_capacity);
        if (m_data) {
            m_size = size;
            std::memcpy(m_data, data, m_size);
            m_data[m_size] = '\0';
        }
    }
}

SecureBuffer::SecureBuffer(const char *str) : SecureBuffer(str, str ? std::strlen(str) : 0) {}

SecureBuffer::SecureBuffer(const QByteArray &ba) : SecureBuffer(ba.constData(), static_cast<size_t>(ba.size())) {}

SecureBuffer::SecureBuffer(const SecureBuffer &other) : m_data(nullptr), m_size(0), m_capacity(0) {
    if (!other.isEmpty()) {
        m_capacity = other.m_size;
        m_data = allocateBuffer(m_capacity);
        if (m_data) {
            m_size = other.m_size;
            std::memcpy(m_data, other.m_data, m_size);
            m_data[m_size] = '\0';
        }
    }
}

SecureBuffer::SecureBuffer(SecureBuffer &&other) noexcept
    : m_data(other.m_data), m_size(other.m_size), m_capacity(other.m_capacity) {
    other.m_data = nullptr;
    other.m_size = 0;
    other.m_capacity = 0;
}

SecureBuffer &SecureBuffer::operator=(const SecureBuffer &other) {
    if (this == &other) {
        return *this;
    }
    if (other.isEmpty()) {
        clear();
        return *this;
    }
    if (m_capacity >= other.m_size) {
        wipe();
        m_size = other.m_size;
        std::memcpy(m_data, other.m_data, m_size);
        m_data[m_size] = '\0';
    } else {
        char *oldData = m_data;
        size_t oldCap = m_capacity;
        m_capacity = other.m_size;
        m_data = allocateBuffer(m_capacity);
        if (m_data) {
            m_size = other.m_size;
            std::memcpy(m_data, other.m_data, m_size);
            m_data[m_size] = '\0';
        } else {
            m_size = 0;
            m_capacity = 0;
        }
        deallocateBuffer(oldData, oldCap);
    }
    return *this;
}

SecureBuffer &SecureBuffer::operator=(SecureBuffer &&other) noexcept {
    if (this == &other) {
        return *this;
    }
    clear();
    m_data = other.m_data;
    m_size = other.m_size;
    m_capacity = other.m_capacity;
    other.m_data = nullptr;
    other.m_size = 0;
    other.m_capacity = 0;
    return *this;
}

SecureBuffer &SecureBuffer::operator=(const QByteArray &ba) {
    *this = SecureBuffer(ba);
    return *this;
}

SecureBuffer &SecureBuffer::operator=(const char *str) {
    *this = SecureBuffer(str);
    return *this;
}

SecureBuffer::~SecureBuffer() {
    clear();
}

char SecureBuffer::at(size_t index) const {
    if (index >= m_size) {
        throw std::out_of_range("SecureBuffer::at index out of range");
    }
    return m_data[index];
}

char &SecureBuffer::operator[](size_t index) noexcept {
    return m_data[index];
}

const char &SecureBuffer::operator[](size_t index) const noexcept {
    return m_data[index];
}

void SecureBuffer::clear() {
    if (m_data) {
        deallocateBuffer(m_data, m_capacity);
        m_data = nullptr;
    }
    m_size = 0;
    m_capacity = 0;
}

void SecureBuffer::wipe() {
    if (m_data && m_capacity > 0) {
        OPENSSL_cleanse(m_data, m_capacity + 1);
        if (s_wipeHook) {
            s_wipeHook(m_data, m_capacity + 1);
        }
    }
}

void SecureBuffer::resize(size_t newSize, char fill) {
    if (newSize == m_size) {
        return;
    }
    if (newSize == 0) {
        if (m_data) {
            OPENSSL_cleanse(m_data, m_size);
            m_data[0] = '\0';
        }
        m_size = 0;
        return;
    }
    if (newSize <= m_capacity) {
        if (newSize > m_size) {
            std::memset(m_data + m_size, fill, newSize - m_size);
        } else {
            OPENSSL_cleanse(m_data + newSize, m_size - newSize);
        }
        m_size = newSize;
        m_data[m_size] = '\0';
        return;
    }

    size_t newCap = std::max(newSize, m_capacity == 0 ? 16 : m_capacity * 2);
    char *newData = allocateBuffer(newCap);
    if (m_data) {
        std::memcpy(newData, m_data, m_size);
        deallocateBuffer(m_data, m_capacity);
    }
    if (newSize > m_size) {
        std::memset(newData + m_size, fill, newSize - m_size);
    }
    m_data = newData;
    m_size = newSize;
    m_capacity = newCap;
    m_data[m_size] = '\0';
}

void SecureBuffer::reserve(size_t newCapacity) {
    if (newCapacity <= m_capacity) {
        return;
    }
    char *newData = allocateBuffer(newCapacity);
    if (m_data) {
        std::memcpy(newData, m_data, m_size);
        deallocateBuffer(m_data, m_capacity);
    }
    m_data = newData;
    m_capacity = newCapacity;
    if (m_data) {
        m_data[m_size] = '\0';
    }
}

void SecureBuffer::append(const char *data, size_t len) {
    if (!data || len == 0) {
        return;
    }
    if (m_size + len > m_capacity) {
        size_t newCap = std::max(m_capacity == 0 ? 16 : m_capacity * 2, m_size + len);
        reserve(newCap);
    }
    std::memcpy(m_data + m_size, data, len);
    m_size += len;
    m_data[m_size] = '\0';
}

void SecureBuffer::append(const SecureBuffer &other) {
    append(other.m_data, other.m_size);
}

void SecureBuffer::append(const QByteArray &ba) {
    append(ba.constData(), static_cast<size_t>(ba.size()));
}

void SecureBuffer::append(char c) {
    append(&c, 1);
}

SecureBuffer &SecureBuffer::operator+=(char c) {
    append(c);
    return *this;
}

SecureBuffer &SecureBuffer::operator+=(const SecureBuffer &other) {
    append(other);
    return *this;
}

SecureBuffer &SecureBuffer::operator+=(const QByteArray &other) {
    append(other);
    return *this;
}

QByteArray SecureBuffer::toQByteArray() const {
    if (isEmpty())
        return QByteArray();
    return QByteArray(m_data, static_cast<qsizetype>(m_size));
}

bool SecureBuffer::operator==(const SecureBuffer &other) const {
    if (m_size != other.m_size) {
        return false;
    }
    if (m_size == 0) {
        return true;
    }
    return CRYPTO_memcmp(m_data, other.m_data, m_size) == 0;
}

bool SecureBuffer::operator==(const QByteArray &other) const {
    if (m_size != static_cast<size_t>(other.size())) {
        return false;
    }
    if (m_size == 0) {
        return true;
    }
    return CRYPTO_memcmp(m_data, other.constData(), m_size) == 0;
}

} // namespace EasePass::Core
