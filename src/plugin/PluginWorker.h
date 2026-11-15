#pragma once

#include "PluginHostFunctions.h"
#include "PluginInstance.h"

#include <QObject>
#include <QPointer>
#include <QReadWriteLock>
#include <QThread>
#include <functional>
#include <memory>

namespace EasePass::Plugin {

/**
 * Runs ONE plugin on its own thread (steps.txt 4.2). The UI never waits for plugin code:
 * call() returns at once and the callback arrives later on this object's thread.
 *
 *  - The instance is created on first use: a plugin runs only when one of its exports is called.
 *  - After a trap or timeout the instance is thrown away; the next call starts a new one.
 *  - Only the host interfaces of the approved permissions are linked.
 */
class PluginWorker : public QObject {
    Q_OBJECT
public:
    struct Setup {
        QByteArray component;
        HostServices services;
        QStringList approvedPermissions;
        PluginLimits limits;
    };
    using Callback = std::function<void(const Value &result, const PluginError &error)>;

    explicit PluginWorker(Setup setup, QObject *parent = nullptr);
    ~PluginWorker() override;

    /// Calls an exported function. `callback` runs on the thread of this object, and only if
    /// `context` (which must live on that thread) still exists. `timeoutMs` <= 0: default.
    void call(const QString &interface, const QString &function, const QList<Value> &arguments, int timeoutMs,
              QObject *context, Callback callback);

    /// Same, but waits for the result. For worker threads (e.g. DatabaseService); do not
    /// use it on the UI thread.
    Value callBlocking(const QString &interface, const QString &function, const QList<Value> &arguments,
                       int timeoutMs, PluginError *error);

    /// Stops the plugin for good: interrupts a running call, drops the instance and ends the
    /// thread. Calls made afterwards fail (Kind::Unusable). The destructor does the same, but
    /// a worker can outlive its owner (callbacks that are still queued keep it alive), so
    /// PluginManager calls this when a plugin is unloaded and when the manager goes away.
    /// Call it on the thread of this object.
    void shutdown();

    /// Compiles and instantiates the plugin now instead of on the first call.
    void start(QObject *context, std::function<void(const PluginError &error)> callback);

private:
    class Runner;
    QThread m_thread;
    Runner *m_runner = nullptr;
    std::shared_ptr<std::atomic_bool> m_cancel;
    /// Read-locked while a blocking call from another thread waits; shutdown() write-locks.
    QReadWriteLock m_shutdownLock;
    bool m_stopped = false; ///< guarded by m_shutdownLock
};

} // namespace EasePass::Plugin
