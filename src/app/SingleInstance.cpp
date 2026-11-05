#include "SingleInstance.h"

#include <QCryptographicHash>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLoggingCategory>
#include <QStandardPaths>

Q_LOGGING_CATEGORY(lcSingleInstance, "easepass.app.singleinstance")

namespace EasePass::App {

namespace {
QString userId() {
    // Unique per user and home directory; hashed so the socket name stays short and neutral.
    const QByteArray home = QDir::homePath().toUtf8();
    const QByteArray user = qEnvironmentVariable("USER", qEnvironmentVariable("USERNAME")).toUtf8();
    return QString::fromLatin1(QCryptographicHash::hash(user + '\0' + home, QCryptographicHash::Sha256).toHex().left(16));
}

const QByteArray kAck = "ok\n";
} // namespace

SingleInstance::SingleInstance(const QString &key, const QString &lockDirectory, QObject *parent) : QObject(parent) {
    const QString id = key + QLatin1Char('-') + userId();
    m_serverName = id;
    QString dir = lockDirectory;
    if (dir.isEmpty())
        dir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (dir.isEmpty())
        dir = QDir::tempPath();
    QDir().mkpath(dir);
    m_lockPath = QDir(dir).filePath(id + QStringLiteral(".lock"));
}

SingleInstance::~SingleInstance() {
    if (m_server)
        m_server->close();
    if (m_lock)
        m_lock->unlock();
}

bool SingleInstance::tryBecomePrimary() {
    if (m_primary)
        return true;
    m_lock = std::make_unique<QLockFile>(m_lockPath);
    m_lock->setStaleLockTime(0); // only a dead owner makes the lock stale
    if (!m_lock->tryLock(100)) {
        m_lock.reset();
        return false;
    }
    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server->listen(m_serverName)) {
        // Socket left behind by a crashed instance: we own the lock, so it is stale.
        QLocalServer::removeServer(m_serverName);
        if (!m_server->listen(m_serverName))
            qCWarning(lcSingleInstance) << "cannot listen for other instances:" << m_server->errorString();
    }
    connect(m_server, &QLocalServer::newConnection, this, &SingleInstance::onNewConnection);
    m_primary = true;
    return true;
}

bool SingleInstance::sendToPrimary(const QStringList &arguments, int timeoutMs) const {
    QLocalSocket socket;
    socket.connectToServer(m_serverName);
    if (!socket.waitForConnected(timeoutMs))
        return false;
    QByteArray payload = QJsonDocument(QJsonArray::fromStringList(arguments)).toJson(QJsonDocument::Compact);
    payload += '\n';
    socket.write(payload);
    if (!socket.waitForBytesWritten(timeoutMs))
        return false;
    while (!socket.canReadLine()) {
        if (!socket.waitForReadyRead(timeoutMs))
            return false;
    }
    return socket.readLine() == kAck;
}

void SingleInstance::onNewConnection() {
    while (QLocalSocket *socket = m_server->nextPendingConnection()) {
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
            if (!socket->canReadLine()) {
                if (socket->bytesAvailable() > 64 * 1024) // not one of ours
                    socket->disconnectFromServer();
                return;
            }
            const QByteArray line = socket->readLine();
            const QJsonDocument doc = QJsonDocument::fromJson(line);
            QStringList args;
            if (doc.isArray())
                for (const QJsonValue &v : doc.array())
                    args << v.toString();
            socket->write(kAck);
            socket->flush();
            emit activationRequested(args);
        });
    }
}

} // namespace EasePass::App
