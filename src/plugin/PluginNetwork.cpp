#include "PluginNetwork.h"

#include <QElapsedTimer>
#include <QHostInfo>
#include <QNetworkProxy>
#include <QRegularExpression>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QUrl>

namespace EasePass::Plugin {

namespace {

HostError error(const char *code, const QString &message) { return {QLatin1String(code), message}; }

QString sizeText(qsizetype bytes) {
    constexpr qsizetype MiB = 1024 * 1024;
    return bytes >= MiB && bytes % MiB == 0 ? QStringLiteral("%1 MiB").arg(bytes / MiB)
                                            : QStringLiteral("%1 bytes").arg(bytes);
}

bool inSubnet(const QHostAddress &address, const char *subnet, int prefix) {
    return address.isInSubnet(QHostAddress(QLatin1String(subnet)), prefix);
}

/// Reads from a blocking socket until a deadline.
struct Reader {
    enum class State { Ok, Closed, Timeout };

    QSslSocket &socket;
    QElapsedTimer &timer;
    int timeoutMs;
    QByteArray buffer;

    int remaining() const { return int(qMax<qint64>(0, timeoutMs - timer.elapsed())); }

    /// Appends whatever arrives next to `buffer`.
    State more() {
        if (socket.bytesAvailable() == 0) {
            if (remaining() == 0)
                return State::Timeout;
            if (!socket.waitForReadyRead(remaining())) {
                if (socket.bytesAvailable() == 0)
                    return socket.state() == QAbstractSocket::ConnectedState ? State::Timeout : State::Closed;
            }
        }
        buffer.append(socket.readAll());
        return State::Ok;
    }

    /// Reads up to and including `delimiter`; the returned data excludes it.
    State readUntil(const QByteArray &delimiter, qsizetype limit, QByteArray *out) {
        for (;;) {
            const qsizetype index = buffer.indexOf(delimiter);
            if (index >= 0) {
                *out = buffer.left(index);
                buffer.remove(0, index + delimiter.size());
                return State::Ok;
            }
            if (buffer.size() > limit)
                return State::Closed;
            const State state = more();
            if (state != State::Ok)
                return state;
        }
    }

    State readExactly(qsizetype count, QByteArray *out) {
        while (buffer.size() < count) {
            const State state = more();
            if (state != State::Ok)
                return state;
        }
        out->append(buffer.left(count));
        buffer.remove(0, count);
        return State::Ok;
    }
};

HostError readFailure(Reader::State state) {
    return state == Reader::State::Timeout ? error("timeout", QStringLiteral("the server did not answer in time"))
                                           : error("network", QStringLiteral("the connection was closed unexpectedly"));
}

bool isToken(const QString &text) {
    static const QRegularExpression token(QStringLiteral("^[!#$%&'*+.^_`|~0-9A-Za-z-]+$"));
    return token.match(text).hasMatch();
}

// Headers the host sets itself; a plugin must not override them (request smuggling).
bool isReservedHeader(const QString &name) {
    static const QStringList reserved = {QStringLiteral("host"),
                                         QStringLiteral("connection"),
                                         QStringLiteral("content-length"),
                                         QStringLiteral("transfer-encoding"),
                                         QStringLiteral("upgrade"),
                                         QStringLiteral("te"),
                                         QStringLiteral("trailer"),
                                         QStringLiteral("expect"),
                                         QStringLiteral("proxy-authorization"),
                                         QStringLiteral("proxy-connection"),
                                         QStringLiteral("keep-alive"),
                                         QStringLiteral("accept-encoding")};
    return reserved.contains(name.toLower());
}

QString headerValue(const QList<PluginNetwork::Header> &headers, const QString &name) {
    for (const PluginNetwork::Header &header : headers) {
        if (header.first.compare(name, Qt::CaseInsensitive) == 0)
            return header.second;
    }
    return {};
}

void removeHeader(QList<PluginNetwork::Header> &headers, const QString &name) {
    headers.removeIf([&](const PluginNetwork::Header &h) { return h.first.compare(name, Qt::CaseInsensitive) == 0; });
}

std::optional<HostError> checkUrl(const QUrl &url) {
    if (!url.isValid() || url.host().isEmpty() ||
        (url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https")))
        return error("invalid-argument", QStringLiteral("the URL must be an absolute http:// or https:// URL"));
    if (!url.userInfo().isEmpty())
        return error("invalid-argument", QStringLiteral("credentials in the URL are not supported"));
    return std::nullopt;
}

} // namespace

// --- NetworkAllowList ----------------------------------------------------------

std::optional<NetworkAllowList::Entry> NetworkAllowList::parse(const QString &text) {
    // Same grammar as schemas/manifest.schema.json of the plugin spec.
    static const QRegularExpression pattern(QStringLiteral(
        "^(\\*|(\\*\\.)?[a-z0-9]([a-z0-9-]*[a-z0-9])?(\\.[a-z0-9]([a-z0-9-]*[a-z0-9])?)*|\\[[0-9a-f:]+\\]):(\\*|[1-9][0-9]{0,4})$"));
    if (text.size() > 262 || !pattern.match(text).hasMatch())
        return std::nullopt;
    const qsizetype colon = text.lastIndexOf(QLatin1Char(':'));
    QString host = text.left(colon);
    const QString port = text.mid(colon + 1);

    Entry entry;
    if (port != QLatin1String("*")) {
        entry.port = port.toInt();
        if (entry.port < 1 || entry.port > 65535)
            return std::nullopt;
    }
    if (host == QLatin1String("*")) {
        entry.kind = Entry::HostKind::Any;
    } else if (host.startsWith(QLatin1String("*."))) {
        entry.kind = Entry::HostKind::Subdomains;
        entry.host = host.mid(2);
    } else {
        if (host.startsWith(QLatin1Char('[')))
            host = host.mid(1, host.size() - 2);
        QHostAddress address;
        if (address.setAddress(host)) {
            entry.kind = Entry::HostKind::Address;
            entry.address = address;
        } else if (host.contains(QLatin1Char(':'))) {
            return std::nullopt; // bracketed text that is not an IPv6 address
        } else {
            entry.kind = Entry::HostKind::Exact;
            entry.host = host;
        }
    }
    return entry;
}

bool NetworkAllowList::isValidEntry(const QString &entry) { return parse(entry).has_value(); }

NetworkAllowList::NetworkAllowList(const QStringList &entries, QStringList *invalid) {
    for (const QString &text : entries) {
        if (const std::optional<Entry> entry = parse(text))
            m_entries << *entry;
        else if (invalid)
            *invalid << text;
    }
}

bool NetworkAllowList::allowsAnyHost() const {
    for (const Entry &entry : m_entries) {
        if (entry.kind == Entry::HostKind::Any)
            return true;
    }
    return false;
}

bool NetworkAllowList::allows(const QString &hostName, quint16 port) const {
    QString host = hostName.toLower();
    if (host.endsWith(QLatin1Char('.'))) // "example.com." is the same host
        host.chop(1);
    if (host.startsWith(QLatin1Char('[')) && host.endsWith(QLatin1Char(']')))
        host = host.mid(1, host.size() - 2);
    if (host.isEmpty() || port == 0)
        return false;
    QHostAddress address;
    const bool isAddress = address.setAddress(host);

    for (const Entry &entry : m_entries) {
        if (entry.port != 0 && entry.port != port)
            continue;
        switch (entry.kind) {
        case Entry::HostKind::Any:
            return true;
        case Entry::HostKind::Exact:
            if (!isAddress && host == entry.host)
                return true;
            break;
        case Entry::HostKind::Subdomains:
            if (!isAddress && host.endsWith(QLatin1Char('.') + entry.host))
                return true;
            break;
        case Entry::HostKind::Address:
            if (isAddress && address.isEqual(entry.address, QHostAddress::TolerantConversion))
                return true;
            break;
        }
    }
    return false;
}

// --- PluginNetwork ---------------------------------------------------------------

struct PluginNetwork::Connection {
    QSslSocket socket;
    QString host;
    int timeoutMs = 0;
};

PluginNetwork::PluginNetwork(Policy policy) : m_policy(std::move(policy)), m_allowList(m_policy.allowedHosts) {
    if (!m_policy.resolver)
        m_policy.resolver = [](const QString &host) { return QHostInfo::fromName(host).addresses(); };
}

PluginNetwork::~PluginNetwork() = default;

bool PluginNetwork::isBlockedAddress(const QHostAddress &address) {
    if (address.isNull())
        return true;
    bool isV4 = false;
    const quint32 v4 = address.toIPv4Address(&isV4); // also true for ::ffff:a.b.c.d
    if (isV4) {
        const QHostAddress ip(v4);
        return inSubnet(ip, "0.0.0.0", 8) || inSubnet(ip, "10.0.0.0", 8) || inSubnet(ip, "100.64.0.0", 10) ||
               inSubnet(ip, "127.0.0.0", 8) || inSubnet(ip, "169.254.0.0", 16) || inSubnet(ip, "172.16.0.0", 12) ||
               inSubnet(ip, "192.0.0.0", 24) || inSubnet(ip, "192.168.0.0", 16) || inSubnet(ip, "198.18.0.0", 15) ||
               inSubnet(ip, "224.0.0.0", 4) || inSubnet(ip, "240.0.0.0", 4);
    }
    return address == QHostAddress(QHostAddress::AnyIPv6) || address.isLoopback() ||
           inSubnet(address, "fe80::", 10) || inSubnet(address, "fc00::", 7) || inSubnet(address, "ff00::", 8) ||
           inSubnet(address, "64:ff9b::", 96); // NAT64 can embed a private IPv4 address
}

int PluginNetwork::effectiveTimeout(std::optional<quint32> requested) const {
    if (!requested || *requested == 0)
        return qMin(m_policy.defaultTimeoutMs, m_policy.maxTimeoutMs);
    return int(qMin<quint32>(*requested, quint32(m_policy.maxTimeoutMs)));
}

int PluginNetwork::openConnections() const { return int(m_connections.size()); }

std::optional<HostError> PluginNetwork::openSocket(const QString &host, quint16 port, int timeoutMs,
                                                   QSslSocket *socket) {
    // 1. Allow-list, by the name the plugin asked for, before any lookup.
    if (!m_allowList.allows(host, port))
        return error("not-allowed", QStringLiteral("%1:%2 is not in the plugin's list of allowed hosts").arg(host).arg(port));

    // 2. Resolve once and judge the addresses.
    QString name = host;
    if (name.startsWith(QLatin1Char('[')) && name.endsWith(QLatin1Char(']')))
        name = name.mid(1, name.size() - 2);
    QList<QHostAddress> addresses;
    QHostAddress literal;
    if (literal.setAddress(name))
        addresses << literal;
    else
        addresses = m_policy.resolver(name);
    if (addresses.isEmpty())
        return error("network", QStringLiteral("cannot resolve %1").arg(host));

    QHostAddress target;
    for (const QHostAddress &address : addresses) {
        if (m_policy.allowPrivateAddresses || !isBlockedAddress(address)) {
            target = address;
            break;
        }
    }
    if (target.isNull())
        return error("not-allowed",
                     QStringLiteral("%1 is a local or private network address").arg(host));

    // 3. Connect to exactly the address that was checked.
    socket->setProxy(QNetworkProxy::NoProxy);
    socket->connectToHost(target, port);
    if (!socket->waitForConnected(timeoutMs)) {
        const bool timedOut = socket->error() == QAbstractSocket::SocketTimeoutError;
        return error(timedOut ? "timeout" : "network",
                     QStringLiteral("cannot connect to %1:%2: %3").arg(host).arg(port).arg(socket->errorString()));
    }
    return std::nullopt;
}

std::optional<HostError> PluginNetwork::startTls(QSslSocket *socket, const QString &host, int timeoutMs) {
    if (socket->isEncrypted())
        return error("invalid-argument", QStringLiteral("the connection is already encrypted"));
    socket->setPeerVerifyMode(QSslSocket::VerifyPeer);
    socket->setPeerVerifyName(host);
    if (!m_policy.extraCaCertificates.isEmpty()) {
        QSslConfiguration configuration = socket->sslConfiguration();
        configuration.addCaCertificates(m_policy.extraCaCertificates);
        socket->setSslConfiguration(configuration);
    }
    socket->startClientEncryption();
    if (!socket->waitForEncrypted(timeoutMs)) {
        const bool timedOut = socket->error() == QAbstractSocket::SocketTimeoutError;
        return error(timedOut ? "timeout" : "network",
                     QStringLiteral("TLS handshake with %1 failed: %2").arg(host, socket->errorString()));
    }
    return std::nullopt;
}

std::optional<HostError> PluginNetwork::httpRequest(const HttpRequest &request, HttpResponse *response) {
    QString method = request.method.toUpper();
    if (method.isEmpty() || method.size() > 16 || !isToken(method) || method == QLatin1String("CONNECT"))
        return error("invalid-argument", QStringLiteral("unsupported HTTP method"));
    QUrl url(request.url, QUrl::StrictMode);
    if (auto failure = checkUrl(url))
        return failure;
    std::optional<QByteArray> body = request.body;
    if (body && body->size() > m_policy.maxBodyBytes)
        return error("too-large", QStringLiteral("the request body is larger than %1").arg(sizeText(m_policy.maxBodyBytes)));

    QList<Header> headers;
    for (const Header &header : request.headers) {
        if (!isToken(header.first) || header.second.contains(QLatin1Char('\r')) ||
            header.second.contains(QLatin1Char('\n')) || header.second.contains(QChar(0)))
            return error("invalid-argument", QStringLiteral("invalid header '%1'").arg(header.first));
        if (isReservedHeader(header.first))
            return error("invalid-argument", QStringLiteral("the header '%1' is set by the host").arg(header.first));
        headers << header;
    }

    QElapsedTimer timer;
    timer.start();
    const int timeoutMs = effectiveTimeout(request.timeoutMs);
    const auto remaining = [&] { return int(qMax<qint64>(0, timeoutMs - timer.elapsed())); };

    for (int hop = 0;; ++hop) {
        if (remaining() == 0)
            return error("timeout", QStringLiteral("the request took too long"));
        const bool https = url.scheme() == QLatin1String("https");
        const int port = url.port(https ? 443 : 80);
        QSslSocket socket;
        if (auto failure = openSocket(url.host(), quint16(port), remaining(), &socket)) {
            if (hop > 0)
                failure->message = QStringLiteral("redirect refused: ") + failure->message;
            return failure;
        }
        if (https) {
            if (auto failure = startTls(&socket, url.host(), remaining()))
                return failure;
        }

        // --- send
        QString target = url.path(QUrl::FullyEncoded);
        if (target.isEmpty())
            target = QStringLiteral("/");
        if (url.hasQuery())
            target += QLatin1Char('?') + url.query(QUrl::FullyEncoded);
        QString hostHeader = url.host(QUrl::FullyEncoded);
        if (hostHeader.contains(QLatin1Char(':')))
            hostHeader = QLatin1Char('[') + hostHeader + QLatin1Char(']');
        if (port != (https ? 443 : 80))
            hostHeader += QLatin1Char(':') + QString::number(port);

        QByteArray out = method.toLatin1() + ' ' + target.toLatin1() + " HTTP/1.1\r\n";
        out += "Host: " + hostHeader.toLatin1() + "\r\n";
        out += "Connection: close\r\nAccept-Encoding: identity\r\n";
        if (headerValue(headers, QStringLiteral("user-agent")).isEmpty())
            out += "User-Agent: EasePass-Plugin/1\r\n";
        for (const Header &header : headers)
            out += header.first.toLatin1() + ": " + header.second.toUtf8() + "\r\n";
        if (body)
            out += "Content-Length: " + QByteArray::number(body->size()) + "\r\n";
        out += "\r\n";
        if (body)
            out += *body;
        socket.write(out);
        // With TLS the data waits twice: as plain text and, once encrypted, for the network.
    while (socket.bytesToWrite() > 0 || socket.encryptedBytesToWrite() > 0) {
            if (remaining() == 0 || !socket.waitForBytesWritten(remaining()))
                return error(remaining() == 0 ? "timeout" : "network",
                             QStringLiteral("cannot send the request: %1").arg(socket.errorString()));
        }

        // --- receive
        Reader reader{socket, timer, timeoutMs, {}};
        int status = 0;
        QList<Header> responseHeaders;
        for (;;) { // skip interim 1xx responses
            QByteArray head;
            const Reader::State state = reader.readUntil("\r\n\r\n", 64 * 1024, &head);
            if (state != Reader::State::Ok)
                return readFailure(state);
            const QList<QByteArray> lines = head.split('\n');
            static const QRegularExpression statusLine(QStringLiteral("^HTTP/1\\.[01] (\\d{3})( .*)?$"));
            const QRegularExpressionMatch match = statusLine.match(QString::fromLatin1(lines.value(0).trimmed()));
            if (!match.hasMatch())
                return error("network", QStringLiteral("the server sent an invalid HTTP response"));
            status = match.captured(1).toInt();
            responseHeaders.clear();
            for (qsizetype i = 1; i < lines.size(); ++i) {
                const QByteArray line = lines.at(i).trimmed();
                const qsizetype colon = line.indexOf(':');
                if (colon > 0)
                    responseHeaders << Header(QString::fromLatin1(line.left(colon).trimmed()),
                                              QString::fromLatin1(line.mid(colon + 1).trimmed()));
            }
            if (status >= 200 || status == 101)
                break;
        }

        QByteArray responseBody;
        const QString transferEncoding = headerValue(responseHeaders, QStringLiteral("transfer-encoding")).toLower();
        const QString contentLength = headerValue(responseHeaders, QStringLiteral("content-length"));
        const auto tooLarge = [this] {
            return error("too-large", QStringLiteral("the response is larger than %1").arg(sizeText(m_policy.maxBodyBytes)));
        };
        if (method == QLatin1String("HEAD") || status == 204 || status == 304 || status < 200) {
            // no body
        } else if (transferEncoding.contains(QLatin1String("chunked"))) {
            for (;;) {
                QByteArray sizeLine;
                Reader::State state = reader.readUntil("\r\n", 1024, &sizeLine);
                if (state != Reader::State::Ok)
                    return readFailure(state);
                bool ok = false;
                const qlonglong size = sizeLine.split(';').value(0).trimmed().toLongLong(&ok, 16);
                if (!ok || size < 0)
                    return error("network", QStringLiteral("the server sent an invalid chunked response"));
                if (size == 0)
                    break; // trailers are ignored; the connection is closed anyway
                if (responseBody.size() + size > m_policy.maxBodyBytes)
                    return tooLarge();
                state = reader.readExactly(qsizetype(size), &responseBody);
                QByteArray lineEnd;
                if (state == Reader::State::Ok)
                    state = reader.readExactly(2, &lineEnd);
                if (state != Reader::State::Ok)
                    return readFailure(state);
            }
        } else if (!contentLength.isEmpty()) {
            bool ok = false;
            const qlonglong size = contentLength.toLongLong(&ok);
            if (!ok || size < 0)
                return error("network", QStringLiteral("the server sent an invalid Content-Length"));
            if (size > m_policy.maxBodyBytes)
                return tooLarge();
            const Reader::State state = reader.readExactly(qsizetype(size), &responseBody);
            if (state != Reader::State::Ok)
                return readFailure(state);
        } else {
            for (;;) {
                const Reader::State state = reader.more();
                if (reader.buffer.size() > m_policy.maxBodyBytes)
                    return tooLarge();
                if (state == Reader::State::Timeout)
                    return readFailure(state);
                if (state == Reader::State::Closed)
                    break;
            }
            responseBody = reader.buffer;
        }
        socket.abort();

        // --- redirect: every hop passes the same checks as the first request
        const QString location = headerValue(responseHeaders, QStringLiteral("location"));
        const bool redirect = (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) &&
                              !location.isEmpty();
        if (!redirect) {
            response->status = status;
            response->headers = responseHeaders;
            response->body = responseBody;
            return std::nullopt;
        }
        if (hop >= m_policy.maxRedirects)
            return error("network", QStringLiteral("too many redirects"));
        const QUrl next = url.resolved(QUrl(location, QUrl::StrictMode));
        if (auto failure = checkUrl(next)) {
            failure->code = QStringLiteral("not-allowed");
            failure->message = QStringLiteral("redirect refused: ") + failure->message;
            return failure;
        }
        if (status == 303 || ((status == 301 || status == 302) && method == QLatin1String("POST"))) {
            method = QStringLiteral("GET");
            body.reset();
            removeHeader(headers, QStringLiteral("content-type"));
        }
        if (next.host() != url.host() || next.port() != url.port() || next.scheme() != url.scheme()) {
            removeHeader(headers, QStringLiteral("authorization"));
            removeHeader(headers, QStringLiteral("cookie"));
        }
        url = next;
    }
}

std::optional<HostError> PluginNetwork::tcpConnect(const QString &host, quint16 port, bool tls,
                                                   std::optional<quint32> timeoutMs, quint32 *handle) {
    if (m_connections.size() >= m_policy.maxConnections)
        return error("unavailable", QStringLiteral("too many open connections"));
    auto connection = std::make_shared<Connection>();
    connection->host = host;
    connection->timeoutMs = effectiveTimeout(timeoutMs);
    if (auto failure = openSocket(host, port, connection->timeoutMs, &connection->socket))
        return failure;
    if (tls) {
        if (auto failure = startTls(&connection->socket, host, connection->timeoutMs))
            return failure;
    }
    while (m_connections.contains(m_nextHandle) || m_nextHandle == 0)
        ++m_nextHandle;
    *handle = m_nextHandle++;
    m_connections.insert(*handle, connection);
    return std::nullopt;
}

std::optional<HostError> PluginNetwork::tcpRead(quint32 handle, quint32 maxBytes, QByteArray *data) {
    const auto connection = m_connections.value(handle);
    if (!connection)
        return error("invalid-argument", QStringLiteral("unknown connection"));
    if (maxBytes == 0)
        return error("invalid-argument", QStringLiteral("max-bytes must be at least 1"));
    QSslSocket &socket = connection->socket;
    if (socket.bytesAvailable() == 0 && !socket.waitForReadyRead(connection->timeoutMs) &&
        socket.bytesAvailable() == 0) {
        if (socket.state() != QAbstractSocket::ConnectedState) {
            data->clear(); // the peer closed the connection
            return std::nullopt;
        }
        return error("timeout", QStringLiteral("no data received in time"));
    }
    *data = socket.read(qMin<qint64>(maxBytes, m_policy.maxBodyBytes));
    return std::nullopt;
}

std::optional<HostError> PluginNetwork::tcpWrite(quint32 handle, const QByteArray &data, quint32 *written) {
    const auto connection = m_connections.value(handle);
    if (!connection)
        return error("invalid-argument", QStringLiteral("unknown connection"));
    if (data.size() > m_policy.maxBodyBytes)
        return error("too-large", QStringLiteral("more than %1 in one write").arg(sizeText(m_policy.maxBodyBytes)));
    QSslSocket &socket = connection->socket;
    if (socket.state() != QAbstractSocket::ConnectedState)
        return error("network", QStringLiteral("the connection is closed"));
    socket.write(data);
    QElapsedTimer timer;
    timer.start();
    while (socket.bytesToWrite() > 0) {
        const int remaining = int(qMax<qint64>(0, connection->timeoutMs - timer.elapsed()));
        if (remaining == 0)
            return error("timeout", QStringLiteral("the data could not be sent in time"));
        if (!socket.waitForBytesWritten(remaining) && socket.state() != QAbstractSocket::ConnectedState)
            return error("network", QStringLiteral("the connection was closed: %1").arg(socket.errorString()));
    }
    *written = quint32(data.size());
    return std::nullopt;
}

std::optional<HostError> PluginNetwork::tcpStartTls(quint32 handle) {
    const auto connection = m_connections.value(handle);
    if (!connection)
        return error("invalid-argument", QStringLiteral("unknown connection"));
    return startTls(&connection->socket, connection->host, connection->timeoutMs);
}

void PluginNetwork::tcpClose(quint32 handle) {
    const auto connection = m_connections.take(handle);
    if (!connection)
        return;
    // An orderly end: with TLS the other side must see "close notify", or it has to treat
    // what it received as cut off (FTPS servers discard such an upload).
    QSslSocket &socket = connection->socket;
    if (socket.state() == QAbstractSocket::ConnectedState) {
        socket.disconnectFromHost();
        if (socket.state() != QAbstractSocket::UnconnectedState)
            socket.waitForDisconnected(qMin(connection->timeoutMs, 5000));
    }
    socket.abort();
}

void PluginNetwork::closeAll() {
    for (const auto &connection : std::as_const(m_connections))
        connection->socket.abort();
    m_connections.clear();
}

} // namespace EasePass::Plugin
