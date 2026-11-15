#pragma once

#include "PasswordGeneratorViewModel.h"
#include "PasswordItem.h"
#include "PluginManager.h"
#include "RemoteDatabase.h"

#include <QHash>
#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <functional>
#include <memory>

namespace EasePass::Core {
class AppSettings;
class DatabaseService;
class IClock;
class INetworkClient;
class LeakCheckService;
} // namespace EasePass::Core

namespace EasePass::Plugin {
class CatalogClient;
}

namespace EasePass::App {

class DialogService;

/**
 * Everything the UI does with plugins (steps.txt T28): plugin manager page (installed list,
 * store list), the install flow with its two dialogs, the file dialog plugins ask for, and
 * the Settings sections "import passwords" and "remote database settings".
 * Only built when the plugin host is available (EASEPASS_HAVE_PLUGINS).
 *
 * Install flow (4.2, 4.3): installFromFile()/installFromStore() -> the package is hashed,
 * verified against the catalog and inspected -> `pendingInstall` holds what the dialogs
 * show. Every text about what the plugin can do is derived from the values the host
 * COMPUTED from the component (types, permissions) plus the allow-list of servers; the
 * plugin's own description is never part of it.
 *   installStep = UnverifiedWarning  ("not officially verified" / "could not verify")
 *     -> confirmUnverified() -> Permissions -> approveInstall()
 * cancelInstall() drops the candidate at any step.
 *
 * Use of the installed plugins by the rest of the application:
 *   remoteDatabases()    the databases of all enabled database-source plugins, asked at
 *                        start, after every change of the installed plugins and after a
 *                        provider's settings were saved (AppContext hands them to
 *                        DatabaseController and DatabaseManagementViewModel)
 *   passwordGenerators() the enabled password-generator plugins for
 *                        PasswordGeneratorViewModel
 * A plugin whose database is open cannot be uninstalled, disabled or updated: the open
 * database would keep running the old code.
 */
class PluginsViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList installed READ installed NOTIFY installedChanged)
    Q_PROPERTY(QVariantList store READ store NOTIFY storeChanged)
    Q_PROPERTY(bool showIncompatible READ showIncompatible WRITE setShowIncompatible NOTIFY storeChanged)
    Q_PROPERTY(CatalogStatus catalogStatus READ catalogStatus NOTIFY storeChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(InstallStep installStep READ installStep NOTIFY pendingInstallChanged)
    Q_PROPERTY(QVariantMap pendingInstall READ pendingInstall NOTIFY pendingInstallChanged)
    Q_PROPERTY(int importerCount READ importerCount NOTIFY installedChanged)
    Q_PROPERTY(int providerCount READ providerCount NOTIFY installedChanged)
    Q_PROPERTY(QVariantList importSources READ importSources NOTIFY importSourcesChanged)
    Q_PROPERTY(QStringList importCandidates READ importCandidates NOTIFY importCandidatesChanged)
    Q_PROPERTY(QVariantList providers READ providers NOTIFY providersChanged)
    Q_PROPERTY(QVariantList configFields READ configFields NOTIFY configChanged)
    Q_PROPERTY(QString configPluginId READ configPluginId NOTIFY configChanged)
    Q_PROPERTY(bool remoteLoading READ remoteLoading NOTIFY remoteDatabasesChanged)
public:
    enum Severity { Info, Success, Warning, Error }; // same order as Theme.Severity
    enum class InstallStep { None, UnverifiedWarning, Permissions };
    Q_ENUM(InstallStep)
    enum class CatalogStatus { Unknown, Loading, Live, Cached, Unavailable };
    Q_ENUM(CatalogStatus)
    /// Values of `verification` in the QML maps.
    enum Verification { Verified, Unverified, Offline, Revoked };
    Q_ENUM(Verification)

    struct Config {
        QString pluginsDirectory;
        QUrl catalogUrl;
        std::shared_ptr<Core::INetworkClient> network;
        Core::AppSettings *settings = nullptr;
        Core::DatabaseService *service = nullptr;
        Core::LeakCheckService *leakCheck = nullptr;
        DialogService *dialogs = nullptr;
        const Core::IClock *clock = nullptr;
        QString appVersion;
        QString os;                  ///< default: the system we run on (tests override it)
        Plugin::PluginLimits limits; ///< tests use short timeouts
        std::function<QList<QHostAddress>(const QString &host)> resolver; ///< test seam
    };
    using UrlOpener = std::function<bool(const QUrl &)>;

    static inline const QString DefaultCatalogUrl =
        QStringLiteral("https://raw.githubusercontent.com/finn-freitag/EasePassQt/main/plugins/PluginList.json");
    static constexpr int MaxConfigFields = 64;
    static constexpr int MaxDatabasesPerPlugin = 50;

    explicit PluginsViewModel(Config config, QObject *parent = nullptr);
    ~PluginsViewModel() override;

    Plugin::PluginManager *manager() const { return m_manager; }
    Plugin::CatalogClient *catalog() const { return m_catalog; }
    void setUrlOpener(UrlOpener opener) { m_opener = std::move(opener); }

    // --- Lists ----------------------------------------------------------------------
    /// {id, name, version, author, description, homepage, icon, types[], permissions[],
    ///  warnings[], verification, verificationText, enabled, usesNetwork, allowPrivateNetwork}
    QVariantList installed() const;
    /// {id, name, description, author, version, feed, compatible, note, installed,
    ///  installedVersion, updateAvailable, sensitive}. Revoked entries are never listed;
    /// incompatible ones only while showIncompatible is on.
    QVariantList store() const;
    bool showIncompatible() const { return m_showIncompatible; }
    void setShowIncompatible(bool show);
    CatalogStatus catalogStatus() const;
    bool busy() const { return m_busy > 0; }
    int importerCount() const { return countOfType(QStringLiteral("importer")); }
    int providerCount() const { return countOfType(QStringLiteral("database-source")); }

    /// Fetches the catalog (and the catalogs named by store-feed plugins).
    Q_INVOKABLE void refreshStore();

    // --- Install --------------------------------------------------------------------
    InstallStep installStep() const { return m_step; }
    /// {id, name, version, author, types[], permissions[], warnings[], verification,
    ///  verificationText, isUpdate, installedVersion, osWarning, feed}; empty if none.
    QVariantMap pendingInstall() const { return m_pending; }
    Q_INVOKABLE void installFromFile(const QUrl &file);
    Q_INVOKABLE void installFromStore(const QString &id, const QString &feed = QString());
    Q_INVOKABLE void confirmUnverified();
    Q_INVOKABLE void approveInstall();
    Q_INVOKABLE void cancelInstall();

    // --- Installed plugins ----------------------------------------------------------
    /// Asks through DialogService, then uninstalls.
    Q_INVOKABLE void requestUninstall(const QString &id);
    Q_INVOKABLE bool uninstall(const QString &id);
    Q_INVOKABLE bool setEnabled(const QString &id, bool enabled);
    Q_INVOKABLE bool setAllowPrivateNetwork(const QString &id, bool allow);
    /// Opens the plugin's homepage (http/https only).
    Q_INVOKABLE bool openWebsite(const QString &id);

    // --- File dialog for plugins (permission pick-file) -----------------------------
    /// Answer to filePickRequested: the chosen file, or an empty URL if the user cancelled.
    Q_INVOKABLE void resolveFilePick(int requestId, const QUrl &file);

    // --- Import passwords through importer plugins ----------------------------------
    /// {pluginId, pluginName, sourceId, name, description}
    QVariantList importSources() const { return m_importSources; }
    QStringList importCandidates() const;
    Q_INVOKABLE void loadImportSources();
    /// Result: importCandidates + importReady(), or message().
    Q_INVOKABLE void importFrom(const QString &pluginId, const QString &sourceId);
    Q_INVOKABLE bool importSelected(const QVariantList &indexes, bool overrideExisting);
    Q_INVOKABLE void cancelImport();

    // --- Remote database settings (database-source plugins) -------------------------
    /// {pluginId, name}
    QVariantList providers() const { return m_providers; }
    Q_INVOKABLE void loadProviders();
    /// {key, title, description, kind, value, options[], required, minimum, maximum, maxLength};
    /// kind: "string", "password", "integer", "number", "boolean" or "choice".
    QVariantList configFields() const { return m_configFields; }
    QString configPluginId() const { return m_configPluginId; }
    /// Result: configFields + configReady(), or message().
    Q_INVOKABLE void openProviderConfig(const QString &pluginId);
    /// Result: configSaved(ok).
    Q_INVOKABLE void saveProviderConfig(const QVariantMap &values);
    Q_INVOKABLE void closeProviderConfig();

    // --- Databases and generators of plugins -----------------------------------------
    QList<RemoteDatabase> remoteDatabases() const;
    bool remoteLoading() const { return m_remotePending > 0; }
    /// Asks every enabled database-source plugin for its databases (asynchronous).
    Q_INVOKABLE void refreshRemoteDatabases();
    QList<PasswordGeneratorViewModel::ExternalGenerator> passwordGenerators();

    // --- Texts (steps.txt 4.2; the English texts equal spec/permissions.json) --------
    static QString permissionText(const QString &permission, const QStringList &networkHosts);
    static QString typeText(const QString &type);
    static QStringList riskWarnings(const QStringList &types, const QStringList &permissions,
                                    const QStringList &networkHosts);
    static QString verificationText(Plugin::PluginVerification verification);
    static QString describe(const Plugin::PluginResult &result);
    static QString describe(const Plugin::PluginError &error);
    /// Form fields of a JSON Schema object (`type: object` with flat `properties`).
    static QVariantList configFieldsFromSchema(const QByteArray &schemaJson, const QByteArray &configJson,
                                               QString *error);
    /// The JSON a plugin gets back: form values converted to the types of the fields.
    static QByteArray configJsonFromValues(const QVariantList &fields, const QVariantMap &values, QString *error);

signals:
    void installedChanged();
    void storeChanged();
    void busyChanged();
    void pendingInstallChanged();
    void importSourcesChanged();
    void importCandidatesChanged();
    void importReady();
    void providersChanged();
    void configChanged();
    void configReady();
    void configSaved(bool ok);
    void remoteDatabasesChanged();
    void filePickRequested(int requestId, const QString &pluginName, const QString &title,
                           const QStringList &nameFilters);
    void urlOpened(const QString &url);
    /// Severity as Theme.Severity.
    void message(int severity, const QString &text);

private:
    struct PickBridge;
    struct PickRequest;

    struct RemoteEntry {
        QString pluginId;
        RemoteDatabase database;
    };
    void finishRemoteRefresh();
    /// Id of the plugin that stores the open database (empty if it is a local one).
    QString openDatabasePlugin() const;
    bool refuseWhileDatabaseOpen(const QString &pluginId);

    int countOfType(const QString &type) const;
    QList<Plugin::InstalledPlugin> enabledOfType(const QString &type) const;
    void beginBusy();
    void endBusy();
    void prepare(const QByteArray &packageBytes, const QByteArray &expectedSha256, const QString &feed);
    void setPending(Plugin::InstallCandidate candidate, const QString &feed);
    void clearPending();
    void applyRevocations();
    void fetchFeedsThenRefresh();
    void beginFilePick(const std::shared_ptr<PickRequest> &request, const QString &pluginName, const QString &title,
                       const QStringList &extensions);
    QString iconSource(const QString &id) const;

    Config m_config;
    Plugin::CatalogClient *m_catalog = nullptr;
    Plugin::PluginManager *m_manager = nullptr;
    UrlOpener m_opener;
    std::shared_ptr<PickBridge> m_pickBridge;
    QHash<int, std::shared_ptr<PickRequest>> m_picks;
    int m_nextPickId = 1;

    int m_busy = 0;
    bool m_showIncompatible = false;
    bool m_refreshFailed = false;
    InstallStep m_step = InstallStep::None;
    QVariantMap m_pending;
    Plugin::InstallCandidate m_candidate;
    mutable QHash<QString, QString> m_iconCache; ///< sha256 -> data URL

    QVariantList m_importSources;
    quint64 m_importGeneration = 0;
    QList<Core::PasswordItem> m_importItems;
    QVariantList m_providers;
    quint64 m_providerGeneration = 0;
    quint64 m_configGeneration = 0;
    QVariantList m_configFields;
    QString m_configPluginId;

    QList<RemoteEntry> m_remote;
    QList<RemoteEntry> m_remoteIncoming;
    int m_remotePending = 0;
    quint64 m_remoteGeneration = 0;
};

} // namespace EasePass::App
