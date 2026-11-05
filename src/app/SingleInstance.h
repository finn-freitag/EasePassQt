#pragma once

#include <QLockFile>
#include <QObject>
#include <QStringList>
#include <memory>

class QLocalServer;

namespace EasePass::App {

/**
 * Single application instance (steps.txt T19): the first process takes a QLockFile and
 * listens on a QLocalServer; later processes send their arguments to it and exit, and the
 * first instance raises its window (activationRequested). Both names derive from @p key and
 * the user, so different users on one machine do not see each other.
 *
 * Messages are a UTF-8 JSON array of strings ending with '\n' (the secondary's arguments
 * without the program name). The lock file guards against two primaries; a stale lock of a
 * crashed instance is taken over (QLockFile checks the PID), as is a stale socket name.
 */
class SingleInstance : public QObject {
    Q_OBJECT
public:
    /// @p lockDirectory: where the lock file lives (default: QStandardPaths::RuntimeLocation,
    /// falling back to the temp dir). Tests pass a temporary directory and a unique key.
    explicit SingleInstance(const QString &key, const QString &lockDirectory = QString(), QObject *parent = nullptr);
    ~SingleInstance() override;

    /// Takes the lock and starts listening. false: another instance runs (call sendToPrimary()).
    bool tryBecomePrimary();
    bool isPrimary() const { return m_primary; }

    /// Sends @p arguments to the running primary; true when it acknowledged them.
    bool sendToPrimary(const QStringList &arguments, int timeoutMs = 2000) const;

    QString serverName() const { return m_serverName; }
    QString lockFilePath() const { return m_lockPath; }

signals:
    /// A second launch asked the primary to show itself; @p arguments are its arguments.
    void activationRequested(const QStringList &arguments);

private:
    void onNewConnection();

    QString m_serverName;
    QString m_lockPath;
    std::unique_ptr<QLockFile> m_lock;
    QLocalServer *m_server = nullptr;
    bool m_primary = false;
};

} // namespace EasePass::App
