#pragma once

#include "PluginInspector.h"
#include "PluginValue.h"

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>
#include <atomic>
#include <functional>
#include <memory>
#include <stdexcept>

namespace EasePass::Plugin {

/// Why a plugin call did not produce a value.
struct PluginError {
    enum class Kind {
        None,
        Rejected,    ///< inspection refused the component (forbidden import, not a component, ...)
        Instantiate, ///< linking failed, e.g. the component imports an interface that was not approved
        Plugin,      ///< the plugin returned an `error` value; `code` holds its error-code
        Trap,        ///< the plugin crashed (panic, out of memory, ...)
        Timeout,     ///< the call exceeded its time limit and was interrupted
        Unusable,    ///< an earlier trap or timeout left this instance unusable
        Host,        ///< failure on the host side (missing export, unexpected value, refused to load, ...)
    };

    Kind kind = Kind::None;
    QString code;    ///< WIT error-code, only for Kind::Plugin (e.g. "cancelled", "not-found")
    QString message;

    bool ok() const { return kind == Kind::None; }

    /// Turns a `result<T, error>` returned by a plugin into its ok payload; sets `error` for `err`.
    static Value unwrap(const Value &result, PluginError *error);
};

struct PluginLimits {
    /// Cap for each linear memory of the plugin. Kept small on purpose: a byte list a plugin
    /// returns costs the host 32 bytes per byte while it is converted (docs/PLUGIN_DECISION.md).
    qint64 maxMemoryBytes = 64ll * 1024 * 1024;
    int callTimeoutMs = 30'000; ///< default wall-clock budget of one call
    /// Set to true (from any thread) to interrupt the running call like a timeout, e.g. at shutdown.
    std::shared_ptr<std::atomic_bool> cancel;

    static constexpr qsizetype MaxByteListSize = 8 * 1024 * 1024; ///< list<u8> or string per call
    /// A database handled by a database-source plugin. Wasmtime limits the data copied
    /// between host and plugin within one call (128 MiB of its value type, about 2.6 MiB
    /// of bytes), and a database crosses twice (network <-> plugin <-> Ease Pass).
    static constexpr qsizetype MaxDatabaseBytes = 2 * 1024 * 1024;
};

/// Thrown by a host function to make the calling plugin trap (misuse that has no error value).
struct HostTrap : std::runtime_error {
    using std::runtime_error::runtime_error;
};

/// One function of a host interface. Runs on the thread that calls the plugin.
struct HostFunction {
    std::function<Value(const QList<Value> &arguments)> function;
    /// The user decides how long it takes (file dialog): its time does not count as plugin time.
    bool interactive = false;
};

/// One interface the host offers, e.g. "easepass:plugin/storage@1.0.0".
struct HostInterface {
    QString name;
    QHash<QString, HostFunction> functions;
};

/**
 * One instantiated plugin component (Wasmtime C API, Component Model): explicit linker
 * with exactly the given host interfaces, a store of its own with a memory cap, call
 * timeouts, and errors instead of crashes.
 *
 * Not thread-safe: use an instance from one thread (PluginWorker does that).
 */
class PluginInstance {
public:
    /// Inspects, links and instantiates `componentBytes`. Returns null and sets `error` on failure.
    /// A component that imports an interface missing from `hostInterfaces` cannot be instantiated.
    static std::unique_ptr<PluginInstance> create(const QByteArray &componentBytes,
                                                  const QList<HostInterface> &hostInterfaces, PluginLimits limits,
                                                  PluginError *error);
    ~PluginInstance();

    const PluginInspection &inspection() const;
    /// false after a trap or timeout: every further call fails with Kind::Unusable.
    bool usable() const;

    /// Calls `function` of the exported interface `interface` (name without version, e.g.
    /// "easepass:plugin/importer"). `timeoutMs` <= 0 uses PluginLimits::callTimeoutMs.
    Value call(const QString &interface, const QString &function, const QList<Value> &arguments, int timeoutMs,
               PluginError *error);

private:
    struct Private;
    explicit PluginInstance(std::unique_ptr<Private> d);
    std::unique_ptr<Private> d;
};

} // namespace EasePass::Plugin
