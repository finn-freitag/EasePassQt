#include "PluginWorker.h"

namespace EasePass::Plugin {

/// Lives on the worker thread and owns the instance.
class PluginWorker::Runner : public QObject {
public:
    explicit Runner(Setup setup) : m_setup(std::move(setup)) {}

    bool ensureInstance(PluginError *error) {
        if (m_instance && m_instance->usable())
            return true;
        reset();
        m_instance = PluginInstance::create(m_setup.component,
                                            buildHostInterfaces(m_setup.services, m_setup.approvedPermissions),
                                            m_setup.limits, error);
        return m_instance != nullptr;
    }

    Value run(const QString &interface, const QString &function, const QList<Value> &arguments, int timeoutMs,
              PluginError *error) {
        *error = {};
        if (!ensureInstance(error))
            return {};
        const Value result = m_instance->call(interface, function, arguments, timeoutMs, error);
        if (!m_instance->usable())
            reset(); // trap or timeout: the next call gets a fresh instance
        return result;
    }

    void reset() {
        m_instance.reset();
        if (m_setup.services.network)
            m_setup.services.network->closeAll();
    }

private:
    Setup m_setup;
    std::unique_ptr<PluginInstance> m_instance;
};

PluginWorker::PluginWorker(Setup setup, QObject *parent)
    : QObject(parent), m_cancel(std::make_shared<std::atomic_bool>(false)) {
    setup.limits.cancel = m_cancel;
    m_runner = new Runner(std::move(setup));
    m_runner->moveToThread(&m_thread);
    m_thread.setObjectName(QStringLiteral("plugin"));
    m_thread.start();
}

PluginWorker::~PluginWorker() {
    shutdown();
    delete m_runner;
}

void PluginWorker::shutdown() {
    // Interrupt a running call first: a blocking caller on another thread then returns and
    // releases the lock.
    m_cancel->store(true);
    QWriteLocker lock(&m_shutdownLock);
    if (m_stopped)
        return;
    m_stopped = true;
    // Drop the instance on its own thread (sockets live there).
    QMetaObject::invokeMethod(m_runner, [runner = m_runner] { runner->reset(); }, Qt::BlockingQueuedConnection);
    m_thread.quit();
    m_thread.wait();
}

namespace {
PluginError stoppedError() {
    return {PluginError::Kind::Unusable, {}, QStringLiteral("the plugin was stopped")};
}
} // namespace

void PluginWorker::call(const QString &interface, const QString &function, const QList<Value> &arguments,
                        int timeoutMs, QObject *context, Callback callback) {
    QPointer<QObject> guard(context);
    {
        QReadLocker lock(&m_shutdownLock);
        if (m_stopped) {
            QMetaObject::invokeMethod(
                this,
                [guard, callback] {
                    if (guard && callback)
                        callback(Value(), stoppedError());
                },
                Qt::QueuedConnection);
            return;
        }
    }
    QMetaObject::invokeMethod(
        m_runner,
        [=, this, runner = m_runner] {
            PluginError error;
            const Value result = runner->run(interface, function, arguments, timeoutMs, &error);
            // Deliver on the thread of the PluginWorker; dropped if the worker is gone by then.
            QMetaObject::invokeMethod(
                this,
                [guard, callback, result, error] {
                    if (guard && callback)
                        callback(result, error);
                },
                Qt::QueuedConnection);
        },
        Qt::QueuedConnection);
}

Value PluginWorker::callBlocking(const QString &interface, const QString &function, const QList<Value> &arguments,
                                 int timeoutMs, PluginError *error) {
    PluginError ignored;
    if (!error)
        error = &ignored;
    if (QThread::currentThread() == &m_thread)
        return m_runner->run(interface, function, arguments, timeoutMs, error);
    // Held while waiting, so the thread cannot end underneath (which would wait forever).
    QReadLocker lock(&m_shutdownLock);
    if (m_stopped) {
        *error = stoppedError();
        return {};
    }
    Value result;
    QMetaObject::invokeMethod(
        m_runner, [&, runner = m_runner] { result = runner->run(interface, function, arguments, timeoutMs, error); },
        Qt::BlockingQueuedConnection);
    return result;
}

void PluginWorker::start(QObject *context, std::function<void(const PluginError &error)> callback) {
    QPointer<QObject> guard(context);
    {
        QReadLocker lock(&m_shutdownLock);
        if (m_stopped) {
            QMetaObject::invokeMethod(
                this,
                [guard, callback] {
                    if (guard && callback)
                        callback(stoppedError());
                },
                Qt::QueuedConnection);
            return;
        }
    }
    QMetaObject::invokeMethod(
        m_runner,
        [=, this, runner = m_runner] {
            PluginError error;
            runner->ensureInstance(&error);
            QMetaObject::invokeMethod(
                this,
                [guard, callback, error] {
                    if (guard && callback)
                        callback(error);
                },
                Qt::QueuedConnection);
        },
        Qt::QueuedConnection);
}

} // namespace EasePass::Plugin
