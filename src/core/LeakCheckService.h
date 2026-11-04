#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QUrl>
#include <functional>
#include <memory>

#include "IClock.h"
#include "INetworkClient.h"
#include "SecretString.h"

namespace EasePass::Core {

class AppSettings;

/**
 * Checks passwords against the Have I Been Pwned "Pwned Passwords" range API using
 * k-anonymity: only the first 5 hex characters of the password's SHA-1 ever leave the
 * machine; the suffix comparison happens locally. Requests ask for response padding
 * ("Add-Padding: true"; padded entries have count 0 and are ignored).
 * C#: EasePass/Helper/Security/IsPwnedHelper.cs, HashHelper.cs.
 *
 * Opt-in via AppSettings::verifyNotLeaked (when off, every check returns Unknown without
 * network access). Range responses are cached per prefix for cacheTtlSeconds(); concurrent
 * checks sharing a prefix share one request. Callbacks always run asynchronously (queued
 * on this object's thread). This service is meant to back the "leak-check" plugin
 * permission later (T27).
 */
class LeakCheckService : public QObject {
    Q_OBJECT
public:
    enum class Status { NotLeaked, Leaked, Unknown };
    Q_ENUM(Status)

    struct Result {
        Status status = Status::Unknown;
        int count = 0;          ///< times seen in breaches (only for Leaked)
        QString errorString;    ///< set for Unknown (disabled, network error, bad response)
    };
    using Callback = std::function<void(const Result &)>;

    static inline const QString DefaultBaseUrl = QStringLiteral("https://api.pwnedpasswords.com/range/");
    static constexpr int DefaultCacheTtlSeconds = 60 * 60;

    explicit LeakCheckService(std::shared_ptr<INetworkClient> client, AppSettings *settings,
                              const IClock *clock = nullptr, QObject *parent = nullptr);

    /// True if the user enabled leak checks (AppSettings::verifyNotLeaked).
    bool isEnabled() const;

    /// Asynchronously checks @p password. Empty passwords yield Unknown.
    void check(const SecretString &password, Callback callback);

    void setBaseUrl(const QString &url) { m_baseUrl = url; }
    void setCacheTtlSeconds(int seconds) { m_cacheTtl = std::max(0, seconds); }
    int cacheTtlSeconds() const { return m_cacheTtl; }
    void clearCache() { m_cache.clear(); }

    /// Uppercase hex SHA-1 of the UTF-8 password (40 chars). C#: HashHelper.
    static QByteArray sha1Hex(const SecretString &password);
    /// Parses a range response ("SUFFIX:COUNT" lines) and returns the count for @p suffix (0 if absent).
    static int countForSuffix(const QByteArray &rangeBody, const QByteArray &suffix, bool *ok = nullptr);

signals:
    /// Emitted after each completed check (for UI badges); never carries the password.
    void checkFinished(LeakCheckService::Status status, int count);

private:
    struct CacheEntry {
        QByteArray body;
        qint64 fetchedAt = 0;
    };
    struct Waiter {
        QByteArray suffix;
        Callback callback;
    };

    void deliver(Callback callback, const Result &result);
    void handleResponse(const QByteArray &prefix, const NetworkResponse &resp);
    Result evaluate(const QByteArray &body, const QByteArray &suffix) const;

    std::shared_ptr<INetworkClient> m_client;
    QPointer<AppSettings> m_settings;
    const IClock *m_clock;
    QString m_baseUrl = DefaultBaseUrl;
    int m_cacheTtl = DefaultCacheTtlSeconds;
    QHash<QByteArray, CacheEntry> m_cache;
    QHash<QByteArray, QList<Waiter>> m_pending;
};

} // namespace EasePass::Core
