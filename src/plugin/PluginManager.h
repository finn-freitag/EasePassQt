#pragma once

#include "PluginPackage.h"
#include "PluginWorker.h"

#include <QDateTime>
#include <QHash>
#include <QHostAddress>
#include <QObject>
#include <QSslCertificate>
#include <functional>
#include <memory>
#include <optional>

namespace EasePass::Core {
class LeakCheckService;
}

namespace EasePass::Plugin {

/// How much the catalog knows about a package hash (steps.txt 4.3).
enum class PluginVerification {
    Verified,   ///< hash is in the catalog
    Unverified, ///< catalog reachable, hash unknown: "not officially verified"
    Offline,    ///< catalog not reachable: "could not verify"
    Revoked,    ///< hash is in the catalog and marked revoked: blocked
};

/// Catalog lookup by package hash. The real client comes with T28; tests use a fake.
class IPluginCatalog {
public:
    virtual ~IPluginCatalog() = default;
    virtual PluginVerification verify(const QByteArray &sha256Hex) const = 0;
};

/// What the application provides to plugins. Callbacks run on plugin worker threads.
struct PluginEnvironment {
    QString pluginsDirectory; ///< e.g. <AppDataLocation>/plugins
    QString appVersion;       ///< compared with manifest min_app_version
    PluginLimits limits;
    const Core::IClock *clock = nullptr;
    /// Receives sanitized log lines of all plugins. Default: qInfo().
    std::function<void(const QString &pluginId, const QString &level, const QString &message)> log;
    /// Shows a file dialog for the plugin (the UI implements it, T28). nullopt = cancelled.
    std::function<std::optional<PickedFile>(const QString &pluginId, const QString &pluginName, const QString &title,
                                            const QStringList &extensions)>
        pickFile;
    std::function<LeakCheckOutcome(const QString &password)> leakCheck;
    /// AppSettings::removePluginSettingsOnUninstall. Default: true.
    std::function<bool()> removeStorageOnUninstall;
    /// Test seam for name resolution (default: system resolver).
    std::function<QList<QHostAddress>(const QString &host)> resolver;
    /// Test seam: certificates plugins' TLS connections trust besides the system's.
    QList<QSslCertificate> extraCaCertificates;
};

/// Record of an installed plugin (stored in installed.json).
struct InstalledPlugin {
    QString id;
    QString name;
    QString version;
    QString description;
    QString author;
    QString homepage;
    QByteArray sha256;               ///< hash of the installed package; re-checked at every load
    QStringList types;               ///< computed from the component's exports
    QStringList permissions;         ///< computed from the component's imports
    QStringList approvedPermissions; ///< what the user confirmed at install time
    QStringList networkHosts;        ///< manifest allow-list the user saw
    bool enabled = true;
    bool allowPrivateNetwork = false; ///< user override: loopback/private addresses allowed
    PluginVerification verification = PluginVerification::Offline;
    QDateTime installedAt;
};

/// A checked package that is waiting for the user's confirmation.
struct InstallCandidate {
    QByteArray packageBytes;
    PluginPackage package;
    PluginInspection inspection; ///< computed types and permissions to show in the dialog
    PluginVerification verification = PluginVerification::Offline;
    bool isUpdate = false;
    QString installedVersion; ///< if isUpdate
};

struct PluginResult {
    enum class Code {
        Ok,
        TooLarge,
        HashMismatch,     ///< downloaded bytes differ from the catalog entry
        Revoked,
        InvalidPackage,   ///< not a valid .epplugin / manifest
        Unsupported,      ///< api_version, operating system or application version
        Rejected,         ///< forbidden imports/exports, or manifest disagrees with the component
        NotApproved,      ///< the user did not approve every computed permission
        NotInstalled,
        Disabled,
        Tampered,         ///< installed file differs from the hash stored at install time
        Io,
    };
    Code code = Code::Ok;
    QString message;
    bool ok() const { return code == Code::Ok; }
};

/**
 * Installs, verifies, loads and removes plugins (steps.txt T27, 4.2, 4.3).
 *
 *   <pluginsDirectory>/installed.json            records
 *   <pluginsDirectory>/packages/<id>.epplugin    the installed package, unchanged
 *   <pluginsDirectory>/storage/<id>/             private storage of the plugin
 *
 * Nothing a plugin says about itself is trusted: types and permissions are computed from
 * the component at install time AND again at every load, and the package hash is
 * re-checked at every load. Lives on the UI thread.
 */
class PluginManager : public QObject {
    Q_OBJECT
public:
    explicit PluginManager(PluginEnvironment environment, QObject *parent = nullptr);
    ~PluginManager() override;

    /// Catalog used for verification; null = everything is "Offline". Not owned.
    void setCatalog(const IPluginCatalog *catalog) { m_catalog = catalog; }

    /// Step 1 of an install: hash, verify, parse, inspect. Shows nothing and changes nothing.
    /// `expectedSha256`: hash of the catalog entry for installs from the store (must match).
    PluginResult prepareInstall(const QByteArray &packageBytes, InstallCandidate *candidate,
                                const QByteArray &expectedSha256 = {}) const;
    /// Step 2, after the user confirmed: `approvedPermissions` must contain every computed permission.
    PluginResult install(const InstallCandidate &candidate, const QStringList &approvedPermissions);

    PluginResult uninstall(const QString &id);
    PluginResult setEnabled(const QString &id, bool enabled);
    PluginResult setAllowPrivateNetwork(const QString &id, bool allow);

    QList<InstalledPlugin> installedPlugins() const;
    std::optional<InstalledPlugin> plugin(const QString &id) const;
    /// icon.png of the installed package (empty if none).
    QByteArray icon(const QString &id) const;

    /// Returns the running plugin, loading it if necessary (hash and permissions re-checked).
    std::shared_ptr<PluginWorker> load(const QString &id, PluginResult *result = nullptr);
    /// Stops the running plugin (its thread ends; whoever still holds the worker gets errors).
    void unload(const QString &id);
    /// Loads every enabled plugin of type `initializer` and runs its `init` in the background.
    void startInitializers();

    QString packagePath(const QString &id) const;
    QString storageDirectory(const QString &id) const;

    /// leak-check callback backed by Core::LeakCheckService (which lives on another thread).
    static std::function<LeakCheckOutcome(const QString &)> leakCheckWith(Core::LeakCheckService *service);

signals:
    void pluginsChanged();

private:
    void readRecords();
    bool writeRecords(QString *error) const;

    PluginEnvironment m_environment;
    const IPluginCatalog *m_catalog = nullptr;
    QList<InstalledPlugin> m_plugins;
    QHash<QString, std::shared_ptr<PluginWorker>> m_loaded;
    /// Every worker ever handed out and maybe still alive (queued callbacks and open
    /// databases keep a worker alive after unload()); all are stopped with the manager.
    QList<std::weak_ptr<PluginWorker>> m_started;
};

} // namespace EasePass::Plugin
