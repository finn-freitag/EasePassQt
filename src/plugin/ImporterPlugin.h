#pragma once

#include "PluginInspector.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>
#include <functional>
#include <memory>
#include <optional>

namespace EasePass::Plugin {

/// Why a plugin call did not produce a value.
struct PluginError {
    enum class Kind {
        None,
        Rejected,    ///< inspection refused the component (forbidden import, not a component, ...)
        Instantiate, ///< linking failed, e.g. the component imports an interface that was not approved
        Plugin,      ///< the plugin returned an `error` value; `code` holds its error-code
        Trap,        ///< the plugin crashed (panic, out of memory, ...)
        Timeout,     ///< the call exceeded PluginLimits::callTimeoutMs and was interrupted
        Unusable,    ///< an earlier trap or timeout left this instance unusable
        Host,        ///< unexpected failure in the host glue (wrong value shape, missing export)
    };

    Kind kind = Kind::None;
    QString code;    ///< WIT error-code, only for Kind::Plugin (e.g. "cancelled", "not-found")
    QString message;

    bool ok() const { return kind == Kind::None; }
};

struct PluginLimits {
    qint64 maxMemoryBytes = 256ll * 1024 * 1024; ///< cap for each linear memory of the plugin
    int callTimeoutMs = 30'000;                   ///< wall-clock budget of one call
};

/// Host functions a plugin may call. Callbacks run on the thread that calls the plugin.
struct PluginHost {
    struct PickedFile {
        QString name;
        QByteArray data;
    };

    /// easepass:plugin/log (always linked). Level is "debug", "info", "warning" or "error".
    std::function<void(const QString &level, const QString &message)> log;
    /// easepass:plugin/file-picker (linked only if "pick-file" is approved). nullopt = cancelled.
    std::function<std::optional<PickedFile>(const QString &title, const QStringList &extensions)> pickFile;
};

struct ImportSource {
    QString id;
    QString name;
    QString description;
};

/// record password-item of the plugin interface.
struct ImportedPassword {
    QString displayName, userName, email, website, password, notes;
    QStringList tags;
    bool hasTotp = false;
    QString totpSecret;
    int totpDigits = 0;
    int totpInterval = 0;
    QString totpAlgorithm; ///< "sha1", "sha256" or "sha512"
};

/**
 * T26 proof of concept: one instantiated plugin of type `importer` (Wasmtime C API,
 * Component Model). Shows the pieces the real host (T27) needs: inspection before
 * instantiation, an explicit linker that only defines approved interfaces, a memory cap,
 * call timeouts and errors instead of crashes.
 *
 * Not thread-safe: use an instance from one thread at a time.
 */
class ImporterPlugin {
public:
    /// Inspects, links and instantiates `componentBytes`. Returns null and sets `error` on failure.
    /// Only interfaces of `approvedPermissions` (permission ids) are linked besides the neutral ones.
    static std::unique_ptr<ImporterPlugin> load(const QByteArray &componentBytes,
                                                const QStringList &approvedPermissions, PluginHost host,
                                                PluginLimits limits, PluginError *error);
    ~ImporterPlugin();

    const PluginInspection &inspection() const;

    QList<ImportSource> sources(PluginError *error);
    QList<ImportedPassword> importPasswords(const QString &sourceId, PluginError *error);

private:
    struct Private;
    explicit ImporterPlugin(std::unique_ptr<Private> d);
    std::unique_ptr<Private> d;
};

} // namespace EasePass::Plugin
