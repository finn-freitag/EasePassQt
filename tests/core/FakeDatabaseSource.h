#pragma once

#include "IDatabaseSource.h"

#include <QMutex>
#include <QMutexLocker>
#include <QStringList>
#include <QThread>
#include <atomic>

/**
 * In-memory IDatabaseSource for tests. getBytes()/saveBytes()/login()/logout() are called on
 * DatabaseService's worker thread, so all state is mutex/atomic protected.
 */
class FakeDatabaseSource : public EasePass::Core::IDatabaseSource {
public:
    explicit FakeDatabaseSource(QByteArray bytes = QByteArray(), QObject *parent = nullptr)
        : IDatabaseSource(parent), m_bytes(std::move(bytes)) {}

    // --- IDatabaseSource
    QString databaseName() const override { return QStringLiteral("fake"); }
    QString sourceDescription() const override { return QStringLiteral("memory://fake"); }
    bool isReadOnly() const override { return m_readOnly.load(); }
    Availability availability() const override { return m_availability.load(); }
    QDateTime lastModified() const override {
        QMutexLocker lock(&m_mutex);
        return m_lastModified;
    }

    std::optional<QByteArray> getBytes() override {
        QMutexLocker lock(&m_mutex);
        m_calls << QStringLiteral("getBytes");
        if (m_failReads) {
            return std::nullopt;
        }
        return m_bytes;
    }

    bool saveBytes(const QByteArray &bytes) override {
        if (m_saveDelayMs.load() > 0) {
            QThread::msleep(static_cast<unsigned long>(m_saveDelayMs.load()));
        }
        QMutexLocker lock(&m_mutex);
        m_calls << QStringLiteral("saveBytes");
        ++m_saveCount;
        if (m_failSaves) {
            return false;
        }
        m_bytes = bytes;
        m_lastModified = QDateTime::currentDateTimeUtc();
        return true;
    }

    void login() override {
        QMutexLocker lock(&m_mutex);
        m_calls << QStringLiteral("login");
    }
    void logout() override {
        QMutexLocker lock(&m_mutex);
        m_calls << QStringLiteral("logout");
    }

    // --- Test controls
    void setReadOnly(bool readOnly) {
        m_readOnly = readOnly;
        emit changed();
    }
    void setAvailability(Availability a) { m_availability = a; }
    void setFailSaves(bool fail) {
        QMutexLocker lock(&m_mutex);
        m_failSaves = fail;
    }
    void setFailReads(bool fail) {
        QMutexLocker lock(&m_mutex);
        m_failReads = fail;
    }
    void setSaveDelayMs(int ms) { m_saveDelayMs = ms; }

    QByteArray bytes() const {
        QMutexLocker lock(&m_mutex);
        return m_bytes;
    }
    int saveCount() const {
        QMutexLocker lock(&m_mutex);
        return m_saveCount;
    }
    QStringList calls() const {
        QMutexLocker lock(&m_mutex);
        return m_calls;
    }

private:
    mutable QMutex m_mutex;
    QByteArray m_bytes;
    QDateTime m_lastModified;
    QStringList m_calls;
    int m_saveCount = 0;
    bool m_failSaves = false;
    bool m_failReads = false;
    std::atomic<bool> m_readOnly{false};
    std::atomic<Availability> m_availability{Availability::Available};
    std::atomic<int> m_saveDelayMs{0};
};
