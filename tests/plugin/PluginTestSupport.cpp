#include "PluginTestSupport.h"

#include <QTimer>

#include <QFile>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtEndian>
#include <zlib.h>

using namespace EasePass::Plugin;

namespace PluginTest {

QByteArray fixture(const QString &name) {
    QFile file(QStringLiteral(EASEPASS_TEST_FIXTURES_DIR "/plugins/") + name);
    if (!file.open(QIODevice::ReadOnly))
        qFatal("missing fixture %s", qPrintable(file.fileName()));
    return file.readAll();
}

QByteArray component(const QString &name) { return fixture(QStringLiteral("conformance-%1.wasm").arg(name)); }

QByteArray conformancePackage(const QString &name) {
    return fixture(QStringLiteral("packages/org.easepass.conformance.%1.epplugin").arg(name));
}

Value callSync(PluginWorker &worker, const QString &interface, const QString &function, const QList<Value> &arguments,
               PluginError *error, int timeoutMs) {
    bool done = false;
    Value result;
    QObject context;
    worker.call(interface, function, arguments, timeoutMs, &context, [&](const Value &value, const PluginError &e) {
        result = value;
        *error = e;
        done = true;
    });
    if (!waitUntil([&] { return done; }, 150'000))
        *error = {PluginError::Kind::Host, {}, QStringLiteral("test: no answer from the plugin worker")};
    return result;
}

namespace {

void put16(QByteArray &out, quint16 value) {
    char bytes[2];
    qToLittleEndian(value, bytes);
    out.append(bytes, 2);
}
void put32(QByteArray &out, quint32 value) {
    char bytes[4];
    qToLittleEndian(value, bytes);
    out.append(bytes, 4);
}

QByteArray rawDeflate(const QByteArray &data) {
    uLongf size = compressBound(uLong(data.size()));
    QByteArray out(qsizetype(size), Qt::Uninitialized);
    compress2(reinterpret_cast<Bytef *>(out.data()), &size, reinterpret_cast<const Bytef *>(data.constData()),
              uLong(data.size()), 9);
    // zlib stream = 2 byte header + raw deflate + 4 byte Adler-32
    return out.mid(2, qsizetype(size) - 6);
}

} // namespace

QByteArray makeZip(const QList<ZipFile> &files) {
    QByteArray archive, directory;
    for (const ZipFile &file : files) {
        const QByteArray stored = file.deflate ? rawDeflate(file.data) : file.data;
        const quint16 method = quint16(file.method >= 0 ? file.method : (file.deflate ? 8 : 0));
        quint32 crc = quint32(crc32(crc32(0L, Z_NULL, 0), reinterpret_cast<const Bytef *>(file.data.constData()),
                                    uInt(file.data.size())));
        if (file.corruptCrc)
            crc ^= 1;
        const quint32 size = quint32(file.declaredSize >= 0 ? file.declaredSize : file.data.size());
        const quint32 offset = quint32(archive.size());

        put32(archive, 0x04034b50);
        put16(archive, 20);
        put16(archive, file.flags);
        put16(archive, method);
        put16(archive, 0);
        put16(archive, 0x21);
        put32(archive, crc);
        put32(archive, quint32(stored.size()));
        put32(archive, size);
        put16(archive, quint16(file.name.size()));
        put16(archive, 0);
        archive += file.name + stored;

        put32(directory, 0x02014b50);
        put16(directory, 20);
        put16(directory, 20);
        put16(directory, file.flags);
        put16(directory, method);
        put16(directory, 0);
        put16(directory, 0x21);
        put32(directory, crc);
        put32(directory, quint32(stored.size()));
        put32(directory, size);
        put16(directory, quint16(file.name.size()));
        put16(directory, 0);
        put16(directory, 0);
        put16(directory, 0);
        put16(directory, 0);
        put32(directory, 0);
        put32(directory, offset);
        directory += file.name;
    }
    const quint32 directoryOffset = quint32(archive.size());
    archive += directory;
    put32(archive, 0x06054b50);
    put16(archive, 0);
    put16(archive, 0);
    put16(archive, quint16(files.size()));
    put16(archive, quint16(files.size()));
    put32(archive, quint32(directory.size()));
    put32(archive, directoryOffset);
    put16(archive, 0);
    return archive;
}

QByteArray makePackage(const QByteArray &manifest, const QByteArray &wasm, const QByteArray &icon) {
    QList<ZipFile> files = {{"manifest.json", manifest}, {"plugin.wasm", wasm, true}};
    if (!icon.isEmpty())
        files << ZipFile{"icon.png", icon};
    return makeZip(files);
}

QByteArray manifestJson(const QString &id, const QString &extraMembers) {
    QString json = QStringLiteral(
                       R"({"id": "%1", "name": "Test plugin", "version": "1.0.0", "api_version": 1, "description": "d")")
                       .arg(id);
    if (!extraMembers.isEmpty())
        json += QStringLiteral(", ") + extraMembers;
    return (json + QLatin1Char('}')).toUtf8();
}

void FakeHttp::get(const QNetworkRequest &request, ResponseCallback callback) {
    const QString url = request.url().toString();
    requested << url;
    if (silent)
        return;
    EasePass::Core::NetworkResponse response;
    const auto it = replies.constFind(url);
    if (it == replies.constEnd()) {
        response.statusCode = 404;
    } else {
        response.statusCode = it->status;
        response.data = it->body;
        response.errorString = it->error;
    }
    QTimer::singleShot(0, this, [callback, response] { callback(response); });
}

QString downloadUrl(const QString &id) {
    return QStringLiteral("https://plugins.example.test/%1.epplugin").arg(id);
}

QByteArray catalogEntryJson(const QString &id, const QByteArray &sha256, const QString &extraMembers,
                            const QString &version) {
    QString json = QStringLiteral(R"({"id": "%1", "name": "Catalog %1", "description": "From the catalog", )"
                                  R"("author": "Catalog author", "version": "%2", "api_version": 1, )"
                                  R"("download_url": "%3", "sha256": "%4")")
                       .arg(id, version, downloadUrl(id), QString::fromLatin1(sha256));
    if (!extraMembers.isEmpty())
        json += QStringLiteral(", ") + extraMembers;
    return (json + QLatin1Char('}')).toUtf8();
}

QByteArray catalogJson(const QList<QByteArray> &entries) {
    return R"({"schema": 1, "updated": "2026-11-01T00:00:00Z", "plugins": [)" + entries.join(", ") + "]}";
}

QList<QHostAddress> testResolver(const QString &host) {
    if (host.endsWith(QLatin1String(".conformance.test")))
        return {QHostAddress(QHostAddress::LocalHost)};
    return {};
}

// --- TestServer ------------------------------------------------------------------

TestServer::TestServer() {
    start();
    m_ready.acquire();
}

TestServer::~TestServer() {
    quit();
    wait();
}

QString TestServer::url(const QString &path, const QString &host) const {
    return QStringLiteral("http://%1:%2%3").arg(host).arg(m_httpPort).arg(path);
}

QStringList TestServer::requests() const {
    QMutexLocker lock(&m_mutex);
    return m_requests;
}

int TestServer::tcpConnections() const {
    QMutexLocker lock(&m_mutex);
    return m_tcpConnections;
}

void TestServer::setFile(const QByteArray &content) {
    QMutexLocker lock(&m_mutex);
    m_file = content;
}
QByteArray TestServer::cloudDatabase() const {
    QMutexLocker lock(&m_mutex);
    return m_cloudDatabase;
}
void TestServer::setCloudDatabase(const QByteArray &content) {
    QMutexLocker lock(&m_mutex);
    m_cloudDatabase = content;
}
bool TestServer::cloudLocked() const {
    QMutexLocker lock(&m_mutex);
    return m_cloudLocked;
}
void TestServer::setCloudLocked(bool locked) {
    QMutexLocker lock(&m_mutex);
    m_cloudLocked = locked;
}

QByteArray TestServer::respond(const QByteArray &method, const QByteArray &path, const QList<QByteArray> &headerLines,
                               const QByteArray &body, bool *keepSilent) const {
    const auto header = [&](const QByteArray &name) {
        for (const QByteArray &line : headerLines) {
            if (line.toLower().startsWith(name + ":"))
                return line.mid(name.size() + 1).trimmed();
        }
        return QByteArray();
    };
    const auto ok = [](const QByteArray &content, const QByteArray &extra = {}) {
        return "HTTP/1.1 200 OK\r\nContent-Length: " + QByteArray::number(content.size()) + "\r\n" + extra + "\r\n" +
               content;
    };
    const auto redirect = [](int status, const QByteArray &location) {
        return "HTTP/1.1 " + QByteArray::number(status) + " Moved\r\nLocation: " + location +
               "\r\nContent-Length: 0\r\n\r\n";
    };
    const QByteArray reply = "X-Conformance-Reply: " + header("x-conformance") + "-" + method + "\r\n";

    if (path == "/files/vault.epdb" || path.startsWith("/files/vault.epdb?")) {
        QMutexLocker lock(&m_mutex);
        return m_file.isEmpty() ? QByteArray("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n") : ok(m_file);
    }
    if (path.startsWith("/pluginapi/")) {
        const auto json = [](const QByteArray &status, const QByteArray &content) {
            return "HTTP/1.1 " + status + "\r\nContent-Type: application/json\r\nContent-Length: " +
                   QByteArray::number(content.size()) + "\r\n\r\n" + content;
        };
        if (header("accesstoken") != "test-token")
            return json("401 Unauthorized", R"({"success":false,"message":"Invalid access token."})");
        QMutexLocker lock(&m_mutex);
        if (path == "/pluginapi/metadata" && method == "GET")
            return json("200 OK", QByteArray(R"({"success":true,"data":{"LastModified":"2026-01-29T21:35:17.000Z",)") +
                                      R"("DatabaseName":"Team vault","DatabaseID":"1","Locked":)" +
                                      (m_cloudLocked ? "true" : "false") + R"(,"Readonly":false}})");
        if (path == "/pluginapi/database" && method == "GET")
            return ok(m_cloudDatabase, "Content-Type: application/octet-stream\r\n");
        if (path == "/pluginapi/database" && method == "POST") {
            m_cloudDatabase = body;
            return json("200 OK", R"({"success":true,"message":"File uploaded successfully."})");
        }
        if (path == "/pluginapi/userLock" && method == "GET") {
            m_cloudLocked = header("lockstatus") == "lock";
            return json("200 OK", R"({"success":true})");
        }
        return json("404 Not Found", "{}");
    }
    if (path == "/hello")
        return ok("hello", reply);
    if (path == "/echo")
        return ok(body, "X-Conformance-Reply: " + header("content-type") + "\r\n");
    if (path == "/query?a=1&b=two")
        return ok("query ok", reply);
    if (path == "/chunked")
        return "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n4\r\nWiki\r\n5;ext=1\r\npedia\r\n0\r\n\r\n";
    if (path == "/until-close")
        return "HTTP/1.1 200 OK\r\n\r\nstreamed";
    if (path == "/continue")
        return "HTTP/1.1 100 Continue\r\n\r\n" + ok("after continue");
    if (path == "/status-404")
        return "HTTP/1.1 404 Not Found\r\nContent-Length: 4\r\n\r\nnope";
    if (path == "/redirect-same")
        return redirect(302, "/hello");
    if (path == "/redirect-post")
        return redirect(303, "/hello");
    if (path == "/redirect-other")
        return redirect(302, "http://other.conformance.test:" + QByteArray::number(m_httpPort) + "/hello");
    if (path == "/redirect-ftp")
        return redirect(302, "ftp://allowed.conformance.test/file");
    if (path == "/redirect-loop")
        return redirect(302, "/redirect-loop");
    if (path == "/big")
        return ok(QByteArray(5000, 'x'));
    if (path == "/slow") {
        *keepSilent = true;
        return {};
    }
    return "HTTP/1.1 500 Unknown Test Path\r\nContent-Length: 0\r\n\r\n";
}

void TestServer::run() {
    QTcpServer http, tcp;
    http.listen(QHostAddress::LocalHost, 0);
    tcp.listen(QHostAddress::LocalHost, 0);
    m_httpPort = http.serverPort();
    m_tcpPort = tcp.serverPort();

    QObject::connect(&http, &QTcpServer::newConnection, &http, [&] {
        while (QTcpSocket *socket = http.nextPendingConnection()) {
            auto buffer = std::make_shared<QByteArray>();
            QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            QObject::connect(socket, &QTcpSocket::readyRead, socket, [this, socket, buffer] {
                buffer->append(socket->readAll());
                const qsizetype headEnd = buffer->indexOf("\r\n\r\n");
                if (headEnd < 0)
                    return;
                const QList<QByteArray> lines = buffer->left(headEnd).split('\n');
                qsizetype contentLength = 0;
                QList<QByteArray> headerLines;
                for (qsizetype i = 1; i < lines.size(); ++i) {
                    headerLines << lines.at(i).trimmed();
                    if (headerLines.last().toLower().startsWith("content-length:"))
                        contentLength = headerLines.last().mid(15).trimmed().toLongLong();
                }
                if (buffer->size() < headEnd + 4 + contentLength)
                    return;
                const QList<QByteArray> requestLine = lines.value(0).trimmed().split(' ');
                const QByteArray method = requestLine.value(0), path = requestLine.value(1);
                {
                    QMutexLocker lock(&m_mutex);
                    m_requests << QString::fromLatin1(method + " " + path);
                }
                bool keepSilent = false;
                const QByteArray response =
                    respond(method, path, headerLines, buffer->mid(headEnd + 4, contentLength), &keepSilent);
                buffer->clear();
                if (keepSilent)
                    return;
                socket->write(response);
                socket->disconnectFromHost();
            });
        }
    });
    QObject::connect(&tcp, &QTcpServer::newConnection, &tcp, [&] {
        while (QTcpSocket *socket = tcp.nextPendingConnection()) {
            {
                QMutexLocker lock(&m_mutex);
                ++m_tcpConnections;
            }
            QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket] {
                while (socket->canReadLine()) {
                    const QByteArray line = socket->readLine().trimmed();
                    if (line == "bye") {
                        socket->write("closing\n");
                        socket->disconnectFromHost();
                        return;
                    }
                    socket->write("echo:" + line + "\n");
                }
            });
        }
    });
    m_ready.release();
    exec();
}

} // namespace PluginTest
