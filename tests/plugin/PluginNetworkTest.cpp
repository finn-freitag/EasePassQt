#include "PluginTests.h"

#include "PluginNetwork.h"
#include "PluginTestSupport.h"

#include <QTcpServer>
#include <QTest>

using namespace EasePass::Plugin;
using namespace PluginTest;

namespace {

const QStringList ManifestHosts = {QStringLiteral("allowed.conformance.test:*"),
                                   QStringLiteral("*.wild.conformance.test:*"),
                                   QStringLiteral("port.conformance.test:8080"), QStringLiteral("192.0.2.10:*")};

/// Policy of the conformance network plugin, pointed at the local test server.
PluginNetwork::Policy testPolicy() {
    PluginNetwork::Policy policy;
    policy.allowedHosts = ManifestHosts;
    policy.allowPrivateAddresses = true; // the test server is on 127.0.0.1
    policy.resolver = testResolver;
    policy.defaultTimeoutMs = 5000;
    return policy;
}

PluginNetwork::HttpRequest get(const QString &url) {
    PluginNetwork::HttpRequest request;
    request.method = QStringLiteral("GET");
    request.url = url;
    return request;
}

QString header(const PluginNetwork::HttpResponse &response, const QString &name) {
    for (const PluginNetwork::Header &h : response.headers) {
        if (h.first.compare(name, Qt::CaseInsensitive) == 0)
            return h.second;
    }
    return {};
}

} // namespace

void PluginNetworkTest::testAllowListMatching_data() {
    QTest::addColumn<QStringList>("entries");
    QTest::addColumn<QString>("host");
    QTest::addColumn<int>("port");
    QTest::addColumn<bool>("allowed");

    const QStringList exact = {"api.example.com:443"};
    QTest::newRow("exact") << exact << "api.example.com" << 443 << true;
    QTest::newRow("exact, case") << exact << "API.Example.COM" << 443 << true;
    QTest::newRow("exact, trailing dot") << exact << "api.example.com." << 443 << true;
    QTest::newRow("other port") << exact << "api.example.com" << 80 << false;
    QTest::newRow("other host") << exact << "example.com" << 443 << false;
    QTest::newRow("subdomain of exact") << exact << "x.api.example.com" << 443 << false;
    QTest::newRow("suffix trick") << exact << "evilapi.example.com" << 443 << false;
    QTest::newRow("prefix trick") << exact << "api.example.com.evil.org" << 443 << false;
    QTest::newRow("empty host") << exact << "" << 443 << false;
    QTest::newRow("port zero") << QStringList{"api.example.com:*"} << "api.example.com" << 0 << false;

    const QStringList wildcard = {"*.example.org:8443"};
    QTest::newRow("subdomain") << wildcard << "a.example.org" << 8443 << true;
    QTest::newRow("deep subdomain") << wildcard << "a.b.example.org" << 8443 << true;
    QTest::newRow("wildcard excludes apex") << wildcard << "example.org" << 8443 << false;
    QTest::newRow("wildcard suffix trick") << wildcard << "evilexample.org" << 8443 << false;
    QTest::newRow("wildcard port") << wildcard << "a.example.org" << 443 << false;

    QTest::newRow("any port") << QStringList{"example.com:*"} << "example.com" << 65535 << true;
    QTest::newRow("any host") << QStringList{"*:21"} << "ftp.anything.net" << 21 << true;
    QTest::newRow("any host, address") << QStringList{"*:21"} << "203.0.113.5" << 21 << true;
    QTest::newRow("any host, other port") << QStringList{"*:21"} << "ftp.anything.net" << 22 << false;
    QTest::newRow("any host any port") << QStringList{"*:*"} << "whatever.example" << 1 << true;

    const QStringList addresses = {"192.0.2.10:8443", "[2001:db8::1]:443"};
    QTest::newRow("ipv4") << addresses << "192.0.2.10" << 8443 << true;
    QTest::newRow("ipv4 other") << addresses << "192.0.2.11" << 8443 << false;
    QTest::newRow("ipv6") << addresses << "2001:db8::1" << 443 << true;
    QTest::newRow("ipv6 bracketed") << addresses << "[2001:db8::1]" << 443 << true;
    QTest::newRow("ipv6 long form") << addresses << "2001:0db8:0:0:0:0:0:1" << 443 << true;
    QTest::newRow("ipv6 other") << addresses << "2001:db8::2" << 443 << false;
    QTest::newRow("name is not an address entry") << addresses << "192.0.2.10.example.com" << 8443 << false;
    // A name entry never matches an IP literal and the other way round.
    QTest::newRow("address vs wildcard name") << wildcard << "192.0.2.10" << 8443 << false;

    QTest::newRow("empty list") << QStringList{} << "example.com" << 443 << false;
    QTest::newRow("second entry") << QStringList{"a.example.com:1", "b.example.com:2"} << "b.example.com" << 2 << true;
}

void PluginNetworkTest::testAllowListMatching() {
    QFETCH(QStringList, entries);
    QFETCH(QString, host);
    QFETCH(int, port);
    QFETCH(bool, allowed);

    QStringList invalid;
    const NetworkAllowList list(entries, &invalid);
    QVERIFY2(invalid.isEmpty(), qPrintable(invalid.join(", ")));
    QCOMPARE(list.allows(host, quint16(port)), allowed);
}

void PluginNetworkTest::testAllowListEntries() {
    for (const char *valid : {"example.com:443", "*.example.com:*", "*:21", "*:*", "192.0.2.10:8443",
                              "[2001:db8::1]:443", "a-b.example.com:65535", "localhost:1"})
        QVERIFY2(NetworkAllowList::isValidEntry(QLatin1String(valid)), valid);
    for (const char *invalid : {"", "example.com", "example.com:", "example.com:0", "example.com:65536",
                                "example.com:99999", "https://example.com:443", "Example.com:443", "*.*.example.com:1",
                                "api.*.com:443", "*example.com:443", "example.com:443/path", "exa mple.com:443",
                                "[not-ipv6]:443", "[zz::1]:443", "-example.com:443", "example.com:*:*"})
        QVERIFY2(!NetworkAllowList::isValidEntry(QLatin1String(invalid)), invalid);

    // Invalid entries are dropped and never match anything.
    QStringList dropped;
    const NetworkAllowList list({QStringLiteral("example.com"), QStringLiteral("ok.example.com:443")}, &dropped);
    QCOMPARE(dropped, QStringList{QStringLiteral("example.com")});
    QVERIFY(!list.allows(QStringLiteral("example.com"), 443));
    QVERIFY(list.allows(QStringLiteral("ok.example.com"), 443));
    QVERIFY(!list.allowsAnyHost());
    QVERIFY(NetworkAllowList({QStringLiteral("*:22")}).allowsAnyHost());
    QVERIFY(NetworkAllowList().isEmpty());
}

void PluginNetworkTest::testBlockedAddresses_data() {
    QTest::addColumn<QString>("address");
    QTest::addColumn<bool>("blocked");

    for (const char *blocked :
         {"127.0.0.1", "127.255.255.254", "10.0.0.1", "10.255.255.255", "172.16.0.1", "172.31.255.255",
          "192.168.0.1", "192.168.178.1", "169.254.169.254", "100.64.0.1", "0.0.0.0", "0.1.2.3", "224.0.0.1",
          "239.255.255.250", "255.255.255.255", "240.0.0.1", "198.18.0.1", "192.0.0.8", "::1", "::", "fe80::1",
          "fc00::1", "fd12:3456:789a::1", "ff02::1", "::ffff:127.0.0.1", "::ffff:10.0.0.1", "::ffff:192.168.1.1",
          "64:ff9b::7f00:1"})
        QTest::newRow(blocked) << blocked << true;
    for (const char *open : {"8.8.8.8", "1.1.1.1", "93.184.216.34", "172.15.255.255", "172.32.0.1", "192.167.255.255",
                             "192.169.0.1", "100.63.255.255", "100.128.0.1", "169.253.0.1", "11.0.0.1",
                             "2606:4700:4700::1111", "2001:4860:4860::8888", "::ffff:8.8.8.8"})
        QTest::newRow(open) << open << false;
}

void PluginNetworkTest::testBlockedAddresses() {
    QFETCH(QString, address);
    QFETCH(bool, blocked);
    QCOMPARE(PluginNetwork::isBlockedAddress(QHostAddress(address)), blocked);
    QVERIFY(PluginNetwork::isBlockedAddress(QHostAddress()));
}

void PluginNetworkTest::testHttpGetAndPost() {
    TestServer server;
    PluginNetwork network(testPolicy());
    PluginNetwork::HttpResponse response;

    PluginNetwork::HttpRequest request = get(server.url(QStringLiteral("/hello")));
    request.headers = {{QStringLiteral("X-Conformance"), QStringLiteral("abc")}};
    auto error = network.httpRequest(request, &response);
    QVERIFY2(!error, error ? qPrintable(error->message) : "");
    QCOMPARE(response.status, 200);
    QCOMPARE(response.body, QByteArray("hello"));
    QCOMPARE(header(response, QStringLiteral("x-conformance-reply")), QStringLiteral("abc-GET"));

    // Query string and method case are passed on correctly.
    request = get(server.url(QStringLiteral("/query?a=1&b=two")));
    request.method = QStringLiteral("get");
    QVERIFY(!network.httpRequest(request, &response));
    QCOMPARE(response.body, QByteArray("query ok"));

    // POST with a binary body.
    QByteArray body;
    for (int i = 0; i < 256; ++i)
        body.append(char(i));
    request = get(server.url(QStringLiteral("/echo")));
    request.method = QStringLiteral("POST");
    request.body = body;
    request.headers = {{QStringLiteral("Content-Type"), QStringLiteral("application/octet-stream")}};
    QVERIFY(!network.httpRequest(request, &response));
    QCOMPARE(response.body, body);
    QCOMPARE(header(response, QStringLiteral("X-Conformance-Reply")), QStringLiteral("application/octet-stream"));

    // HEAD has no body even though the server announces one.
    request = get(server.url(QStringLiteral("/hello")));
    request.method = QStringLiteral("HEAD");
    QVERIFY(!network.httpRequest(request, &response));
    QCOMPARE(response.status, 200);
    QVERIFY(response.body.isEmpty());

    QCOMPARE(server.requests(),
             (QStringList{"GET /hello", "GET /query?a=1&b=two", "POST /echo", "HEAD /hello"}));
}

void PluginNetworkTest::testHttpBodyFramings() {
    TestServer server;
    PluginNetwork network(testPolicy());
    PluginNetwork::HttpResponse response;

    QVERIFY(!network.httpRequest(get(server.url(QStringLiteral("/chunked"))), &response));
    QCOMPARE(response.body, QByteArray("Wikipedia"));
    QVERIFY(!network.httpRequest(get(server.url(QStringLiteral("/until-close"))), &response));
    QCOMPARE(response.body, QByteArray("streamed"));
    // An interim "100 Continue" is skipped.
    QVERIFY(!network.httpRequest(get(server.url(QStringLiteral("/continue"))), &response));
    QCOMPARE(response.status, 200);
    QCOMPARE(response.body, QByteArray("after continue"));
}

void PluginNetworkTest::testHttpStatusPassedThrough() {
    TestServer server;
    PluginNetwork network(testPolicy());
    PluginNetwork::HttpResponse response;
    // An HTTP error status is a response, not a host error.
    QVERIFY(!network.httpRequest(get(server.url(QStringLiteral("/status-404"))), &response));
    QCOMPARE(response.status, 404);
    QCOMPARE(response.body, QByteArray("nope"));
}

void PluginNetworkTest::testRedirectOnSameHostFollowed() {
    TestServer server;
    PluginNetwork network(testPolicy());
    PluginNetwork::HttpResponse response;

    QVERIFY(!network.httpRequest(get(server.url(QStringLiteral("/redirect-same"))), &response));
    QCOMPARE(response.status, 200);
    QCOMPARE(response.body, QByteArray("hello"));

    // 303 after a POST becomes a GET without body.
    PluginNetwork::HttpRequest post = get(server.url(QStringLiteral("/redirect-post")));
    post.method = QStringLiteral("POST");
    post.body = QByteArray("payload");
    post.headers = {{QStringLiteral("x-conformance"), QStringLiteral("p")}};
    QVERIFY(!network.httpRequest(post, &response));
    QCOMPARE(header(response, QStringLiteral("x-conformance-reply")), QStringLiteral("p-GET"));
    QCOMPARE(server.requests(),
             (QStringList{"GET /redirect-same", "GET /hello", "POST /redirect-post", "GET /hello"}));
}

void PluginNetworkTest::testRedirectToOtherHostBlocked() {
    TestServer server;
    PluginNetwork network(testPolicy());
    PluginNetwork::HttpResponse response;

    // other.conformance.test resolves to the same server but is not on the allow-list.
    auto error = network.httpRequest(get(server.url(QStringLiteral("/redirect-other"))), &response);
    QVERIFY(error.has_value());
    QCOMPARE(error->code, QStringLiteral("not-allowed"));
    QVERIFY2(error->message.contains(QStringLiteral("redirect refused")), qPrintable(error->message));
    QVERIFY2(error->message.contains(QStringLiteral("other.conformance.test")), qPrintable(error->message));
    // Only the first request was sent; the redirect target was never contacted.
    QCOMPARE(server.requests(), QStringList{QStringLiteral("GET /redirect-other")});
    QCOMPARE(response.status, 0);

    // A redirect to another protocol is refused as well.
    error = network.httpRequest(get(server.url(QStringLiteral("/redirect-ftp"))), &response);
    QVERIFY(error.has_value());
    QCOMPARE(error->code, QStringLiteral("not-allowed"));
}

void PluginNetworkTest::testRedirectLoopStops() {
    TestServer server;
    PluginNetwork::Policy policy = testPolicy();
    policy.maxRedirects = 3;
    PluginNetwork network(policy);
    PluginNetwork::HttpResponse response;

    const auto error = network.httpRequest(get(server.url(QStringLiteral("/redirect-loop"))), &response);
    QVERIFY(error.has_value());
    QCOMPARE(error->code, QStringLiteral("network"));
    QCOMPARE(server.requests().size(), 4); // the first request plus three redirects
}

void PluginNetworkTest::testHostAndPortNotOnAllowListBlocked() {
    TestServer server;
    PluginNetwork network(testPolicy());
    PluginNetwork::HttpResponse response;

    // Host not listed (it would resolve to the test server).
    auto error = network.httpRequest(get(server.url(QStringLiteral("/hello"), QStringLiteral("other.conformance.test"))),
                                     &response);
    QVERIFY(error.has_value());
    QCOMPARE(error->code, QStringLiteral("not-allowed"));

    // Host listed for port 8080 only.
    error = network.httpRequest(get(server.url(QStringLiteral("/hello"), QStringLiteral("port.conformance.test"))),
                                &response);
    QVERIFY(error.has_value());
    QCOMPARE(error->code, QStringLiteral("not-allowed"));

    // Wildcard: subdomains yes, the domain itself no.
    QVERIFY(!network.httpRequest(get(server.url(QStringLiteral("/hello"), QStringLiteral("a.wild.conformance.test"))),
                                 &response));
    error = network.httpRequest(get(server.url(QStringLiteral("/hello"), QStringLiteral("wild.conformance.test"))),
                                &response);
    QVERIFY(error.has_value());
    QCOMPARE(error->code, QStringLiteral("not-allowed"));

    // The IP of an allowed name is not allowed by itself.
    error = network.httpRequest(get(server.url(QStringLiteral("/hello"), QStringLiteral("127.0.0.1"))), &response);
    QVERIFY(error.has_value());
    QCOMPARE(error->code, QStringLiteral("not-allowed"));

    // Nothing but the one allowed request reached the server.
    QCOMPARE(server.requests(), QStringList{QStringLiteral("GET /hello")});

    // An empty allow-list allows nothing.
    PluginNetwork::Policy empty = testPolicy();
    empty.allowedHosts.clear();
    PluginNetwork none(empty);
    error = none.httpRequest(get(server.url(QStringLiteral("/hello"))), &response);
    QVERIFY(error.has_value());
    QCOMPARE(error->code, QStringLiteral("not-allowed"));
    quint32 handle = 0;
    QCOMPARE(none.tcpConnect(QStringLiteral("allowed.conformance.test"), server.tcpPort(), false, {}, &handle)->code,
             QStringLiteral("not-allowed"));
}

void PluginNetworkTest::testPrivateAddressBlocked() {
    TestServer server;
    PluginNetwork::Policy policy = testPolicy();
    policy.allowPrivateAddresses = false; // the default
    policy.allowedHosts << QStringLiteral("*:*");
    PluginNetwork network(policy);
    PluginNetwork::HttpResponse response;

    // An allowed NAME that resolves to loopback (DNS pointing into the local network).
    auto error = network.httpRequest(get(server.url(QStringLiteral("/hello"))), &response);
    QVERIFY(error.has_value());
    QCOMPARE(error->code, QStringLiteral("not-allowed"));
    QVERIFY2(error->message.contains(QStringLiteral("private")), qPrintable(error->message));

    // Literal addresses, even with a "*:*" allow-list.
    for (const char *host : {"127.0.0.1", "[::1]", "[::ffff:127.0.0.1]"}) {
        error = network.httpRequest(get(QStringLiteral("http://%1:%2/hello").arg(QLatin1String(host)).arg(server.httpPort())),
                                    &response);
        QVERIFY2(error.has_value(), host);
        QCOMPARE(error->code, QStringLiteral("not-allowed"));
    }
    quint32 handle = 0;
    error = network.tcpConnect(QStringLiteral("127.0.0.1"), server.tcpPort(), false, {}, &handle);
    QVERIFY(error.has_value());
    QCOMPARE(error->code, QStringLiteral("not-allowed"));
    error = network.tcpConnect(QStringLiteral("allowed.conformance.test"), server.tcpPort(), false, {}, &handle);
    QVERIFY(error.has_value());
    QCOMPARE(error->code, QStringLiteral("not-allowed"));

    // A "*:*" entry does not help a name that resolves to loopback either.
    policy.resolver = [](const QString &) { return QList<QHostAddress>{QHostAddress(QHostAddress::LocalHost)}; };
    PluginNetwork anyName(policy);
    error = anyName.httpRequest(get(QStringLiteral("http://rebind.example.com:%1/hello").arg(server.httpPort())),
                                &response);
    QVERIFY(error.has_value());
    QCOMPARE(error->code, QStringLiteral("not-allowed"));

    QVERIFY(server.requests().isEmpty());
    QCOMPARE(server.tcpConnections(), 0);

    // A name that does not resolve is a network error.
    error = network.httpRequest(get(QStringLiteral("http://nowhere.invalid/")), &response);
    QVERIFY(error.has_value());
    QCOMPARE(error->code, QStringLiteral("network"));
}

void PluginNetworkTest::testResponseTooLarge() {
    TestServer server;
    PluginNetwork::Policy policy = testPolicy();
    policy.maxBodyBytes = 1000;
    PluginNetwork network(policy);
    PluginNetwork::HttpResponse response;

    auto error = network.httpRequest(get(server.url(QStringLiteral("/big"))), &response); // 5000 bytes
    QVERIFY(error.has_value());
    QCOMPARE(error->code, QStringLiteral("too-large"));
    QVERIFY(response.body.isEmpty());

    // The request body is capped too, before anything is sent.
    PluginNetwork::HttpRequest post = get(server.url(QStringLiteral("/echo")));
    post.method = QStringLiteral("POST");
    post.body = QByteArray(1001, 'x');
    error = network.httpRequest(post, &response);
    QCOMPARE(error->code, QStringLiteral("too-large"));
    QCOMPARE(server.requests(), QStringList{QStringLiteral("GET /big")});
}

void PluginNetworkTest::testHttpTimeout() {
    TestServer server;
    PluginNetwork::Policy policy = testPolicy();
    policy.maxTimeoutMs = 400; // the host's upper bound wins over what the plugin asks for
    PluginNetwork network(policy);
    PluginNetwork::HttpResponse response;

    PluginNetwork::HttpRequest request = get(server.url(QStringLiteral("/slow")));
    request.timeoutMs = 60'000;
    QElapsedTimer timer;
    timer.start();
    const auto error = network.httpRequest(request, &response);
    QVERIFY(error.has_value());
    QCOMPARE(error->code, QStringLiteral("timeout"));
    QVERIFY2(timer.elapsed() >= 350 && timer.elapsed() < 5000, qPrintable(QString::number(timer.elapsed())));
}

void PluginNetworkTest::testInvalidRequestsRejected_data() {
    QTest::addColumn<QString>("method");
    QTest::addColumn<QString>("url");
    QTest::addColumn<QString>("headerName");
    QTest::addColumn<QString>("headerValue");

    const QString ok = QStringLiteral("http://allowed.conformance.test:%PORT%/hello");
    QTest::newRow("empty method") << "" << ok << "" << "";
    QTest::newRow("method with space") << "GET /x" << ok << "" << "";
    QTest::newRow("method with CRLF") << "GET\r\nX: y" << ok << "" << "";
    QTest::newRow("CONNECT") << "CONNECT" << ok << "" << "";
    QTest::newRow("relative url") << "GET" << "/hello" << "" << "";
    QTest::newRow("ftp") << "GET" << "ftp://allowed.conformance.test/x" << "" << "";
    QTest::newRow("file") << "GET" << "file:///etc/passwd" << "" << "";
    QTest::newRow("credentials") << "GET" << "http://user:pw@allowed.conformance.test:%PORT%/hello" << "" << "";
    QTest::newRow("no host") << "GET" << "http:///hello" << "" << "";
    QTest::newRow("Host header") << "GET" << ok << "Host" << "evil.example.com";
    QTest::newRow("Content-Length header") << "GET" << ok << "content-length" << "5";
    QTest::newRow("Transfer-Encoding header") << "GET" << ok << "Transfer-Encoding" << "chunked";
    QTest::newRow("Connection header") << "GET" << ok << "Connection" << "keep-alive";
    QTest::newRow("header name with colon") << "GET" << ok << "X:Y" << "v";
    QTest::newRow("header name with space") << "GET" << ok << "X Y" << "v";
    QTest::newRow("header value CRLF") << "GET" << ok << "X-Test" << "a\r\nHost: evil.example.com";
    QTest::newRow("header value LF") << "GET" << ok << "X-Test" << "a\nb";
}

void PluginNetworkTest::testInvalidRequestsRejected() {
    QFETCH(QString, method);
    QFETCH(QString, url);
    QFETCH(QString, headerName);
    QFETCH(QString, headerValue);

    TestServer server;
    PluginNetwork network(testPolicy());
    PluginNetwork::HttpRequest request;
    request.method = method;
    request.url = url.replace(QStringLiteral("%PORT%"), QString::number(server.httpPort()));
    if (!headerName.isEmpty())
        request.headers = {{headerName, headerValue}};
    PluginNetwork::HttpResponse response;
    const auto error = network.httpRequest(request, &response);
    QVERIFY(error.has_value());
    QCOMPARE(error->code, QStringLiteral("invalid-argument"));
    QVERIFY(server.requests().isEmpty()); // rejected before any connection
}

void PluginNetworkTest::testTcpEcho() {
    TestServer server;
    PluginNetwork network(testPolicy());
    const QString host = QStringLiteral("allowed.conformance.test");

    quint32 handle = 0;
    auto error = network.tcpConnect(host, server.tcpPort(), false, {}, &handle);
    QVERIFY2(!error, error ? qPrintable(error->message) : "");
    QVERIFY(handle != 0);
    QCOMPARE(network.openConnections(), 1);

    quint32 written = 0;
    QVERIFY(!network.tcpWrite(handle, "ping\n", &written));
    QCOMPARE(written, 5u);
    QByteArray data, chunk;
    while (!data.endsWith('\n')) {
        QVERIFY(!network.tcpRead(handle, 3, &chunk)); // small reads: data arrives in pieces
        QVERIFY(!chunk.isEmpty() && chunk.size() <= 3);
        data += chunk;
    }
    QCOMPARE(data, QByteArray("echo:ping\n"));

    // The peer closes: an empty read signals the end of the stream.
    QVERIFY(!network.tcpWrite(handle, "bye\n", &written));
    data.clear();
    for (int i = 0; i < 20; ++i) {
        QVERIFY(!network.tcpRead(handle, 100, &chunk));
        if (chunk.isEmpty())
            break;
        data += chunk;
    }
    QCOMPARE(data, QByteArray("closing\n"));
    QVERIFY(chunk.isEmpty());

    network.tcpClose(handle);
    QCOMPARE(network.openConnections(), 0);
    network.tcpClose(handle);  // closing twice is harmless
    network.tcpClose(123456);  // and so is an unknown handle
    QCOMPARE(network.tcpRead(handle, 10, &chunk)->code, QStringLiteral("invalid-argument"));
    QCOMPARE(network.tcpWrite(handle, "x", &written)->code, QStringLiteral("invalid-argument"));
    QCOMPARE(network.tcpStartTls(handle)->code, QStringLiteral("invalid-argument"));
}

void PluginNetworkTest::testTcpLimitsAndErrors() {
    TestServer server;
    PluginNetwork::Policy policy = testPolicy();
    policy.maxConnections = 2;
    policy.defaultTimeoutMs = 400;
    PluginNetwork network(policy);
    const QString host = QStringLiteral("allowed.conformance.test");

    quint32 first = 0, second = 0, third = 0;
    QVERIFY(!network.tcpConnect(host, server.tcpPort(), false, {}, &first));
    QVERIFY(!network.tcpConnect(host, server.tcpPort(), false, {}, &second));
    QVERIFY(first != second);
    QCOMPARE(network.tcpConnect(host, server.tcpPort(), false, {}, &third)->code, QStringLiteral("unavailable"));
    network.tcpClose(first);
    QVERIFY(!network.tcpConnect(host, server.tcpPort(), false, {}, &third)); // a slot is free again

    // Reading when nothing arrives times out instead of blocking forever.
    QByteArray data;
    QElapsedTimer timer;
    timer.start();
    QCOMPARE(network.tcpRead(second, 10, &data)->code, QStringLiteral("timeout"));
    QVERIFY(timer.elapsed() < 5000);
    QCOMPARE(network.tcpRead(second, 0, &data)->code, QStringLiteral("invalid-argument"));

    // TLS against a server that does not speak TLS fails cleanly (no silent fallback to plain text).
    const auto tlsError = network.tcpStartTls(second);
    QVERIFY(tlsError.has_value());
    QVERIFY(tlsError->code == QLatin1String("network") || tlsError->code == QLatin1String("timeout"));
    network.tcpClose(third);
    quint32 tls = 0;
    const auto connectError = network.tcpConnect(host, server.tcpPort(), true, 400, &tls);
    QVERIFY(connectError.has_value());
    QCOMPARE(tls, 0u);

    // Nobody listens on this port.
    QTcpServer closed;
    QVERIFY(closed.listen(QHostAddress::LocalHost, 0));
    const quint16 freePort = closed.serverPort();
    closed.close();
    quint32 refused = 0;
    QCOMPARE(network.tcpConnect(host, freePort, false, {}, &refused)->code, QStringLiteral("network"));

    network.closeAll();
    QCOMPARE(network.openConnections(), 0);
}

void PluginNetworkTest::testNetworkPluginEndToEnd() {
    // The conformance network plugin drives the same host code through the plugin boundary.
    TestServer server;
    HostServices services;
    services.network = std::make_shared<PluginNetwork>(testPolicy());
    PluginError error;
    const auto plugin = PluginInstance::create(
        component(QStringLiteral("network")), buildHostInterfaces(services, {QStringLiteral("network")}), {}, &error);
    QVERIFY2(plugin, qPrintable(error.message));

    const auto run = [&](const QString &command) {
        const Value raw = plugin->call(QStringLiteral("easepass:plugin/importer"), QStringLiteral("import-passwords"),
                                       {Value::string(command)}, 0, &error);
        return error.ok() ? PluginError::unwrap(raw, &error).items().value(0) : Value();
    };

    Value item = run(QStringLiteral("http GET ") + server.url(QStringLiteral("/hello")));
    QVERIFY2(error.ok(), qPrintable(error.message));
    QCOMPARE(item.field("display-name").toString(), QStringLiteral("200"));
    QCOMPARE(item.field("user-name").toString(), QStringLiteral("1-GET")); // the plugin's own header arrived
    QCOMPARE(item.field("notes").toString(), QStringLiteral("hello"));

    item = run(QStringLiteral("http POST %1 body text").arg(server.url(QStringLiteral("/echo"))));
    QVERIFY2(error.ok(), qPrintable(error.message));
    QCOMPARE(item.field("notes").toString(), QStringLiteral("body text"));

    item = run(QStringLiteral("tcp allowed.conformance.test %1 plain hi there").arg(server.tcpPort()));
    QVERIFY2(error.ok(), qPrintable(error.message));
    QCOMPARE(item.field("notes").toString(), QStringLiteral("echo:hi there"));
    QCOMPARE(services.network->openConnections(), 0); // the plugin closed its connection

    // Host errors arrive in the plugin as error values and come back unchanged.
    run(QStringLiteral("http GET ") + server.url(QStringLiteral("/hello"), QStringLiteral("other.conformance.test")));
    QCOMPARE(error.kind, PluginError::Kind::Plugin);
    QCOMPARE(error.code, QStringLiteral("not-allowed"));
    run(QStringLiteral("http GET ") + server.url(QStringLiteral("/redirect-other")));
    QCOMPARE(error.code, QStringLiteral("not-allowed"));
    run(QStringLiteral("tcp other.conformance.test %1 plain x").arg(server.tcpPort()));
    QCOMPARE(error.code, QStringLiteral("not-allowed"));
    run(QStringLiteral("tcp port.conformance.test %1 plain x").arg(server.tcpPort()));
    QCOMPARE(error.code, QStringLiteral("not-allowed"));
    run(QStringLiteral("tcp allowed.conformance.test %1 starttls x").arg(server.tcpPort()));
    QVERIFY(error.code == QLatin1String("network") || error.code == QLatin1String("timeout"));
    QCOMPARE(services.network->openConnections(), 0);
    QVERIFY(plugin->usable());
}
