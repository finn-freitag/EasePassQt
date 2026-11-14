#pragma once

#include "PluginHostFunctions.h"
#include "PluginInstance.h"
#include "PluginWorker.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QList>
#include <QMutex>
#include <QSemaphore>
#include <QString>
#include <QStringList>
#include <QThread>

// Shared helpers of test_plugin (steps.txt T27).

namespace PluginTest {

/// File below tests/fixtures/plugins.
QByteArray fixture(const QString &name);
/// conformance-<name>.wasm
QByteArray component(const QString &name);
/// packages/org.easepass.conformance.<name>.epplugin
QByteArray conformancePackage(const QString &name);

/// Spins the event loop until `condition()` or the timeout.
template <typename Condition> bool waitUntil(Condition condition, int timeoutMs = 20'000) {
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeoutMs)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    return condition();
}

/// PluginWorker::call() (the asynchronous path) made synchronous for tests.
EasePass::Plugin::Value callSync(EasePass::Plugin::PluginWorker &worker, const QString &interface,
                                 const QString &function, const QList<EasePass::Plugin::Value> &arguments,
                                 EasePass::Plugin::PluginError *error, int timeoutMs = 0);

// --- ZIP writer for package tests -------------------------------------------------

struct ZipFile {
    QByteArray name;
    QByteArray data;
    bool deflate = false;
    quint16 flags = 0;
    int method = -1;            ///< -1: 0 or 8 according to `deflate`
    bool corruptCrc = false;
    qint64 declaredSize = -1;   ///< -1: the real size
};
QByteArray makeZip(const QList<ZipFile> &files);
/// A .epplugin with the given manifest JSON and component.
QByteArray makePackage(const QByteArray &manifestJson, const QByteArray &wasm, const QByteArray &icon = {});
/// Minimal valid manifest with the given id and extra JSON members (e.g. `"os": ["linux"]`).
QByteArray manifestJson(const QString &id, const QString &extraMembers = {});

// --- local server --------------------------------------------------------------------

/// Names ending in ".conformance.test" resolve to 127.0.0.1, everything else does not resolve.
QList<QHostAddress> testResolver(const QString &host);

/**
 * HTTP and TCP echo server on 127.0.0.1 in its own thread (plugin network calls block
 * the calling thread). HTTP paths: /hello, /echo, /chunked, /until-close, /continue,
 * /status-404, /redirect-same, /redirect-post, /redirect-other, /redirect-loop, /big,
 * /slow. TCP: answers each line with "echo:<line>".
 */
class TestServer : public QThread {
public:
    TestServer();
    ~TestServer() override;

    quint16 httpPort() const { return m_httpPort; }
    quint16 tcpPort() const { return m_tcpPort; }
    QString url(const QString &path, const QString &host = QStringLiteral("allowed.conformance.test")) const;
    /// "METHOD path" of every HTTP request that reached the server.
    QStringList requests() const;
    int tcpConnections() const;

protected:
    void run() override;

private:
    QByteArray respond(const QByteArray &method, const QByteArray &path, const QList<QByteArray> &headerLines,
                       const QByteArray &body, bool *keepSilent) const;

    QSemaphore m_ready;
    quint16 m_httpPort = 0;
    quint16 m_tcpPort = 0;
    mutable QMutex m_mutex;
    QStringList m_requests;
    int m_tcpConnections = 0;
};

} // namespace PluginTest
