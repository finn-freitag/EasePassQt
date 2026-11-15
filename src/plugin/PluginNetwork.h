#pragma once

#include "PluginStorage.h" // HostError

#include <QByteArray>
#include <QHash>
#include <QHostAddress>
#include <QList>
#include <QPair>
#include <QString>
#include <QSslCertificate>
#include <QStringList>
#include <functional>
#include <memory>
#include <optional>

class QSslSocket;

namespace EasePass::Plugin {

/**
 * The `network_hosts` list of a plugin manifest: which host:port pairs the plugin may
 * reach (steps.txt 4.2, spec: docs/package-format.md).
 */
class NetworkAllowList {
public:
    NetworkAllowList() = default;
    /// Invalid entries are dropped (they never match); `invalid` receives them.
    explicit NetworkAllowList(const QStringList &entries, QStringList *invalid = nullptr);

    bool allows(const QString &host, quint16 port) const;
    /// At least one entry allows any host ("*:port").
    bool allowsAnyHost() const;
    bool isEmpty() const { return m_entries.isEmpty(); }

    static bool isValidEntry(const QString &entry);

private:
    struct Entry {
        enum class HostKind { Any, Exact, Subdomains, Address };
        HostKind kind = HostKind::Exact;
        QString host; ///< lower case; for Subdomains the part after "*."
        QHostAddress address;
        int port = 0; ///< 0 = any
    };
    static std::optional<Entry> parse(const QString &entry);

    QList<Entry> m_entries;
};

/**
 * Permission `network`: every connection of a plugin goes through this class.
 *
 *  - host and port must be on the manifest allow-list (checked before name resolution);
 *  - the resolved address must not be loopback, link-local, private or otherwise
 *    non-public unless the user allowed that; the connection goes to exactly the address
 *    that was checked (no second lookup);
 *  - HTTP redirects are followed by hand and every hop passes the same checks;
 *  - size, time and connection caps.
 *
 * Blocking, no event loop needed. Use it from one thread (the plugin's worker thread).
 */
class PluginNetwork {
public:
    struct Policy {
        QStringList allowedHosts;            ///< manifest network_hosts the user approved
        bool allowPrivateAddresses = false;  ///< user override for loopback/private ranges
        qsizetype maxBodyBytes = 2 * 1024 * 1024; ///< PluginLimits::MaxByteListSize
        int maxTimeoutMs = 30'000;
        int defaultTimeoutMs = 30'000;
        int maxConnections = 8;
        int maxRedirects = 5;
        /// Name resolution; default: the system resolver. Tests map fake names to a local server.
        std::function<QList<QHostAddress>(const QString &host)> resolver;
        /// Test seam: certificates trusted in addition to the system's (empty in the application).
        QList<QSslCertificate> extraCaCertificates;
    };

    using Header = QPair<QString, QString>;
    struct HttpRequest {
        QString method;
        QString url;
        QList<Header> headers;
        std::optional<QByteArray> body;
        std::optional<quint32> timeoutMs;
    };
    struct HttpResponse {
        int status = 0;
        QList<Header> headers;
        QByteArray body;
    };

    explicit PluginNetwork(Policy policy);
    ~PluginNetwork();

    std::optional<HostError> httpRequest(const HttpRequest &request, HttpResponse *response);

    std::optional<HostError> tcpConnect(const QString &host, quint16 port, bool tls, std::optional<quint32> timeoutMs,
                                        quint32 *handle);
    std::optional<HostError> tcpRead(quint32 handle, quint32 maxBytes, QByteArray *data);
    std::optional<HostError> tcpWrite(quint32 handle, const QByteArray &data, quint32 *written);
    std::optional<HostError> tcpStartTls(quint32 handle);
    void tcpClose(quint32 handle);
    /// Closes every open TCP connection (the plugin instance ended).
    void closeAll();
    int openConnections() const;

    /// true for addresses a plugin must not reach by default: loopback, link-local, private,
    /// unique-local, carrier-grade NAT, multicast, broadcast, unspecified, documentation-free
    /// special ranges; IPv4-mapped IPv6 addresses are judged by their IPv4 part.
    static bool isBlockedAddress(const QHostAddress &address);

private:
    struct Connection;
    std::optional<HostError> openSocket(const QString &host, quint16 port, int timeoutMs, QSslSocket *socket);
    std::optional<HostError> startTls(QSslSocket *socket, const QString &host, int timeoutMs);
    int effectiveTimeout(std::optional<quint32> requested) const;

    Policy m_policy;
    NetworkAllowList m_allowList;
    QHash<quint32, std::shared_ptr<Connection>> m_connections;
    quint32 m_nextHandle = 1;
};

} // namespace EasePass::Plugin
