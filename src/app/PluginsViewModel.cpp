#include "PluginsViewModel.h"

#include "AppSettings.h"
#include "DatabaseOperationResult.h"
#include "DatabaseService.h"
#include "DialogService.h"
#include "INetworkClient.h"
#include "LeakCheckService.h"
#include "PluginAdapters.h"
#include "PluginCatalog.h"
#include "PluginInspector.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QMutex>
#include <QPointer>
#include <QSemaphore>
#include <QTimer>
#include <atomic>

Q_LOGGING_CATEGORY(lcPluginLog, "easepass.plugin.log")

namespace EasePass::App {

using namespace EasePass::Core;
using namespace EasePass::Plugin;

// --- File dialog bridge ---------------------------------------------------------------------
// A plugin calls pick-file on ITS thread and waits there; the dialog runs on the UI thread.

struct PluginsViewModel::PickBridge {
    std::atomic_bool closed{false}; ///< the view model is gone: every waiting plugin gets "cancelled"
};

struct PluginsViewModel::PickRequest {
    QSemaphore done;
    QMutex mutex;
    QString path; ///< empty = cancelled
};

namespace {

QString osName(const QString &os) {
    if (os == QLatin1String("windows"))
        return QStringLiteral("Windows");
    if (os == QLatin1String("macos"))
        return QStringLiteral("macOS");
    if (os == QLatin1String("linux"))
        return QStringLiteral("Linux");
    return os;
}

bool hasAnyServer(const QStringList &hosts) { return NetworkAllowList(hosts).allowsAnyHost(); }

int verificationValue(PluginVerification verification) {
    switch (verification) {
    case PluginVerification::Verified:
        return PluginsViewModel::Verified;
    case PluginVerification::Unverified:
        return PluginsViewModel::Unverified;
    case PluginVerification::Revoked:
        return PluginsViewModel::Revoked;
    case PluginVerification::Offline:
        break;
    }
    return PluginsViewModel::Offline;
}

QStringList typeTexts(const QStringList &types) {
    QStringList texts;
    for (const QString &type : types)
        texts << PluginsViewModel::typeText(type);
    return texts;
}

QStringList permissionTexts(const QStringList &permissions, const QStringList &hosts) {
    QStringList texts;
    for (const QString &permission : permissions)
        texts << PluginsViewModel::permissionText(permission, hosts);
    return texts;
}

} // namespace

// --- Texts ----------------------------------------------------------------------------------

QString PluginsViewModel::permissionText(const QString &permission, const QStringList &networkHosts) {
    if (permission == QLatin1String("storage"))
        return tr("Keeps its own private settings and files");
    if (permission == QLatin1String("pick-file"))
        return tr("Asks you to choose a file (every time)");
    if (permission == QLatin1String("network")) {
        if (networkHosts.isEmpty())
            return tr("Uses the network, but names no server it may connect to");
        return tr("Connects to: %1 (any protocol)").arg(networkHosts.join(QStringLiteral(", ")));
    }
    if (permission == QLatin1String("leak-check"))
        return tr("Checks passwords against a breach service (sends a hash prefix to a third party)");
    return tr("Unknown permission: %1").arg(permission);
}

QString PluginsViewModel::typeText(const QString &type) {
    if (type == QLatin1String("about"))
        return tr("Shows information about itself");
    if (type == QLatin1String("initializer"))
        return tr("Runs when Ease Pass starts");
    if (type == QLatin1String("importer"))
        return tr("Adds passwords to your database");
    if (type == QLatin1String("database-source"))
        return tr("Stores your encrypted database elsewhere (it only receives encrypted data)");
    if (type == QLatin1String("password-generator"))
        return tr("Generates passwords: its output becomes your passwords");
    if (type == QLatin1String("store-feed"))
        return tr("Provides additional plugin lists");
    return tr("Unknown plugin type: %1").arg(type);
}

QStringList PluginsViewModel::riskWarnings(const QStringList &types, const QStringList &permissions,
                                           const QStringList &networkHosts) {
    QStringList warnings;
    if (!permissions.contains(QStringLiteral("network")))
        return warnings;
    if (types.contains(QStringLiteral("database-source")))
        warnings << tr("This plugin sends your encrypted database over the network.");
    if (types.contains(QStringLiteral("password-generator")))
        warnings << tr("This plugin creates your passwords and can send data over the network.");
    if (types.contains(QStringLiteral("importer")))
        warnings << tr("This plugin adds passwords to your database and can send data over the network.");
    if (hasAnyServer(networkHosts))
        warnings << tr("This plugin can connect to ANY server.");
    return warnings;
}

QString PluginsViewModel::verificationText(PluginVerification verification) {
    switch (verification) {
    case PluginVerification::Verified:
        return tr("Verified");
    case PluginVerification::Unverified:
        return tr("Not officially verified");
    case PluginVerification::Revoked:
        return tr("Withdrawn");
    case PluginVerification::Offline:
        break;
    }
    return tr("Could not be verified (offline)");
}

QString PluginsViewModel::describe(const PluginResult &result) {
    using Code = PluginResult::Code;
    const auto withDetail = [&](const QString &text) {
        return result.message.isEmpty() ? text : text + QStringLiteral(" (") + result.message + QLatin1Char(')');
    };
    switch (result.code) {
    case Code::Ok:
        return QString();
    case Code::TooLarge:
        return tr("The plugin file is too large.");
    case Code::HashMismatch:
        return tr("The downloaded file is not the one named in the plugin list. Nothing was installed.");
    case Code::Revoked:
        return tr("This plugin was withdrawn and cannot be used.");
    case Code::InvalidPackage:
        return withDetail(tr("This is not a valid plugin file."));
    case Code::Unsupported:
        return withDetail(tr("This plugin does not work with this version of Ease Pass."));
    case Code::Rejected:
        return withDetail(tr("The plugin was rejected because it asks for something plugins may not do."));
    case Code::NotApproved:
        return tr("The plugin was not installed because not every permission was approved.");
    case Code::NotInstalled:
        return tr("This plugin is not installed.");
    case Code::Disabled:
        return tr("This plugin is disabled.");
    case Code::Tampered:
        return tr("The plugin file was changed after it was installed. It will not be loaded.");
    case Code::Io:
        return withDetail(tr("The plugin could not be stored."));
    }
    return result.message;
}

QString PluginsViewModel::describe(const PluginError &error) {
    switch (error.kind) {
    case PluginError::Kind::None:
        return QString();
    case PluginError::Kind::Timeout:
        return tr("The plugin did not answer in time.");
    case PluginError::Kind::Trap:
    case PluginError::Kind::Unusable:
        return tr("The plugin crashed.");
    case PluginError::Kind::Plugin:
        if (error.code == QLatin1String("cancelled"))
            return tr("Cancelled.");
        if (error.code == QLatin1String("not-found"))
            return tr("The plugin did not find it.");
        if (error.code == QLatin1String("unavailable"))
            return tr("The plugin's service is not available right now.");
        if (error.code == QLatin1String("not-allowed"))
            return tr("The plugin was not allowed to do that.");
        if (error.code == QLatin1String("too-large"))
            return tr("The data is too large for the plugin.");
        if (error.code == QLatin1String("invalid-argument"))
            return error.message.isEmpty() ? tr("The plugin did not accept the input.")
                                           : tr("The plugin did not accept the input: %1").arg(error.message);
        return error.message.isEmpty() ? tr("The plugin reported an error (%1).").arg(error.code)
                                       : tr("The plugin reported an error: %1").arg(error.message);
    case PluginError::Kind::Rejected:
    case PluginError::Kind::Instantiate:
    case PluginError::Kind::Host:
        break;
    }
    return tr("The plugin could not be used: %1").arg(error.message);
}

// --- Construction ---------------------------------------------------------------------------

PluginsViewModel::PluginsViewModel(Config config, QObject *parent)
    : QObject(parent), m_config(std::move(config)), m_opener([](const QUrl &url) { return QDesktopServices::openUrl(url); }),
      m_pickBridge(std::make_shared<PickBridge>()) {
    if (m_config.os.isEmpty())
        m_config.os = PluginManifest::currentOs();
    if (!m_config.catalogUrl.isValid() || m_config.catalogUrl.isEmpty())
        m_config.catalogUrl = QUrl(DefaultCatalogUrl);
    if (!m_config.network)
        m_config.network = std::make_shared<QtNetworkClient>();

    CatalogClient::Options options;
    options.url = m_config.catalogUrl;
    options.cacheDirectory = m_config.pluginsDirectory;
    m_catalog = new CatalogClient(m_config.network, options, this);
    connect(m_catalog, &CatalogClient::changed, this, &PluginsViewModel::storeChanged);

    PluginEnvironment environment;
    environment.pluginsDirectory = m_config.pluginsDirectory;
    environment.appVersion = m_config.appVersion;
    environment.limits = m_config.limits;
    environment.clock = m_config.clock;
    environment.resolver = m_config.resolver;
    // Plugin texts are untrusted: debug level only, so they do not end up in the log file by default.
    environment.log = [](const QString &pluginId, const QString &level, const QString &message) {
        if (level == QLatin1String("error") || level == QLatin1String("warning"))
            qCWarning(lcPluginLog).noquote() << pluginId << level << message;
        else
            qCDebug(lcPluginLog).noquote() << pluginId << level << message;
    };
    if (m_config.leakCheck)
        environment.leakCheck = PluginManager::leakCheckWith(m_config.leakCheck);
    QPointer<AppSettings> settings(m_config.settings);
    environment.removeStorageOnUninstall = [settings] {
        return settings ? settings->removePluginSettingsOnUninstall() : true;
    };
    QPointer<PluginsViewModel> self(this);
    const std::shared_ptr<PickBridge> bridge = m_pickBridge;
    environment.pickFile = [self, bridge](const QString &, const QString &pluginName, const QString &title,
                                          const QStringList &extensions) -> std::optional<PickedFile> {
        const auto request = std::make_shared<PickRequest>();
        // qApp as context: the call must be delivered even if the view model is already gone.
        QMetaObject::invokeMethod(
            qApp,
            [self, request, pluginName, title, extensions] {
                if (self)
                    self->beginFilePick(request, pluginName, title, extensions);
                else
                    request->done.release();
            },
            Qt::QueuedConnection);
        while (!request->done.tryAcquire(1, 100)) {
            if (bridge->closed)
                return std::nullopt;
        }
        QString path;
        {
            QMutexLocker lock(&request->mutex);
            path = request->path;
        }
        if (path.isEmpty())
            return std::nullopt;
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return std::nullopt;
        // One byte more than allowed is enough for the host function to answer "too-large".
        PickedFile picked;
        picked.name = QFileInfo(path).fileName();
        picked.data = file.read(PluginLimits::MaxByteListSize + 1);
        return picked;
    };

    m_manager = new PluginManager(environment, this);
    m_manager->setCatalog(m_catalog);
    connect(m_manager, &PluginManager::pluginsChanged, this, [this] {
        emit installedChanged();
        emit storeChanged();
        refreshRemoteDatabases();
    });
    if (m_config.service) // imported items are secrets: gone when the database is locked
        connect(m_config.service, &DatabaseService::closed, this, &PluginsViewModel::cancelImport);

    QTimer::singleShot(0, this, [this] {
        applyRevocations(); // from the cached catalog; no network at startup
        m_manager->startInitializers();
        // The login page offers the databases of plugins: ask the installed ones now.
        refreshRemoteDatabases();
    });
}

PluginsViewModel::~PluginsViewModel() {
    // Plugins waiting for a file dialog must return before their threads are joined.
    m_pickBridge->closed = true;
    for (const auto &request : std::as_const(m_picks))
        request->done.release();
    delete m_manager;
    m_manager = nullptr;
}

void PluginsViewModel::beginBusy() {
    if (++m_busy == 1)
        emit busyChanged();
}

void PluginsViewModel::endBusy() {
    if (--m_busy == 0)
        emit busyChanged();
}

// --- Lists ----------------------------------------------------------------------------------

QString PluginsViewModel::iconSource(const QString &id) const {
    const std::optional<InstalledPlugin> record = m_manager->plugin(id);
    if (!record)
        return QString();
    const QString key = QString::fromLatin1(record->sha256);
    const auto cached = m_iconCache.constFind(key);
    if (cached != m_iconCache.constEnd())
        return *cached;
    QString source;
    QByteArray bytes = m_manager->icon(id);
    if (!bytes.isEmpty()) {
        // Untrusted image: PNG only, bounded size, and re-encoded before QML sees it.
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer, "png");
        const QSize size = reader.size();
        if (size.isValid() && size.width() <= 1024 && size.height() <= 1024) {
            QImage image = reader.read();
            if (!image.isNull()) {
                if (image.width() > 128 || image.height() > 128)
                    image = image.scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                QByteArray png;
                QBuffer out(&png);
                out.open(QIODevice::WriteOnly);
                if (image.save(&out, "PNG"))
                    source = QStringLiteral("data:image/png;base64,") + QString::fromLatin1(png.toBase64());
            }
        }
    }
    m_iconCache.insert(key, source);
    return source;
}

QVariantList PluginsViewModel::installed() const {
    QVariantList list;
    for (const InstalledPlugin &plugin : m_manager->installedPlugins()) {
        // A plugin that is revoked now is shown as such, whatever was true when it was installed.
        const PluginVerification verification =
            m_catalog->verify(plugin.sha256) == PluginVerification::Revoked ? PluginVerification::Revoked
                                                                           : plugin.verification;
        list << QVariantMap{
            {QStringLiteral("id"), plugin.id},
            {QStringLiteral("name"), plugin.name},
            {QStringLiteral("version"), plugin.version},
            {QStringLiteral("author"), plugin.author},
            {QStringLiteral("description"), plugin.description},
            {QStringLiteral("homepage"), plugin.homepage},
            {QStringLiteral("icon"), iconSource(plugin.id)},
            {QStringLiteral("types"), typeTexts(plugin.types)},
            {QStringLiteral("permissions"), permissionTexts(plugin.permissions, plugin.networkHosts)},
            {QStringLiteral("warnings"), riskWarnings(plugin.types, plugin.permissions, plugin.networkHosts)},
            {QStringLiteral("verification"), verificationValue(verification)},
            {QStringLiteral("verificationText"), verificationText(verification)},
            {QStringLiteral("enabled"), plugin.enabled},
            {QStringLiteral("usesNetwork"), plugin.permissions.contains(QStringLiteral("network"))},
            {QStringLiteral("allowPrivateNetwork"), plugin.allowPrivateNetwork},
        };
    }
    return list;
}

QVariantList PluginsViewModel::store() const {
    QVariantList list;
    for (const CatalogEntry &entry : m_catalog->entries()) {
        if (entry.revoked)
            continue;
        QString note;
        if (!entry.supportsOs(m_config.os)) {
            QStringList names;
            for (const QString &os : entry.os)
                names << osName(os);
            note = tr("%1 only").arg(names.join(QStringLiteral(", ")));
        } else if (entry.apiVersion != PluginInspector::ApiVersion) {
            note = tr("Needs a newer version of Ease Pass");
        } else if (!entry.minAppVersion.isEmpty() && !m_config.appVersion.isEmpty() &&
                   PluginManifest::compareVersions(m_config.appVersion, entry.minAppVersion) < 0) {
            note = tr("Needs Ease Pass %1 or newer").arg(entry.minAppVersion);
        }
        const bool compatible = note.isEmpty();
        if (!compatible && !m_showIncompatible)
            continue;
        const std::optional<InstalledPlugin> record = m_manager->plugin(entry.id);
        list << QVariantMap{
            {QStringLiteral("id"), entry.id},
            {QStringLiteral("name"), entry.name},
            {QStringLiteral("description"), entry.description},
            {QStringLiteral("author"), entry.author},
            {QStringLiteral("version"), entry.version},
            {QStringLiteral("feed"), entry.feed},
            {QStringLiteral("compatible"), compatible},
            {QStringLiteral("note"), note},
            {QStringLiteral("installed"), record.has_value()},
            {QStringLiteral("installedVersion"), record ? record->version : QString()},
            {QStringLiteral("updateAvailable"),
             record && record->sha256 != entry.sha256 &&
                 PluginManifest::compareVersions(entry.version, record->version) > 0},
            {QStringLiteral("sensitive"), entry.sensitive},
        };
    }
    return list;
}

void PluginsViewModel::setShowIncompatible(bool show) {
    if (m_showIncompatible == show)
        return;
    m_showIncompatible = show;
    emit storeChanged();
}

PluginsViewModel::CatalogStatus PluginsViewModel::catalogStatus() const {
    if (m_catalog->isRefreshing())
        return CatalogStatus::Loading;
    switch (m_catalog->state()) {
    case CatalogClient::State::Live:
        return CatalogStatus::Live;
    case CatalogClient::State::Cached:
        return CatalogStatus::Cached;
    case CatalogClient::State::Empty:
        break;
    }
    return m_refreshFailed ? CatalogStatus::Unavailable : CatalogStatus::Unknown;
}

int PluginsViewModel::countOfType(const QString &type) const { return int(enabledOfType(type).size()); }

QList<InstalledPlugin> PluginsViewModel::enabledOfType(const QString &type) const {
    QList<InstalledPlugin> result;
    for (const InstalledPlugin &plugin : m_manager->installedPlugins()) {
        if (plugin.enabled && plugin.types.contains(type))
            result << plugin;
    }
    return result;
}

void PluginsViewModel::applyRevocations() {
    for (const InstalledPlugin &plugin : m_manager->installedPlugins()) {
        if (!plugin.enabled || m_catalog->verify(plugin.sha256) != PluginVerification::Revoked)
            continue;
        m_manager->unload(plugin.id);
        m_manager->setEnabled(plugin.id, false);
        emit message(Warning, tr("The plugin \"%1\" was withdrawn and has been disabled.").arg(plugin.name));
    }
}

void PluginsViewModel::refreshStore() {
    // store-feed plugins name additional catalogs; ask them first (they answer quickly or not at all).
    const QList<InstalledPlugin> feeds = enabledOfType(QStringLiteral("store-feed"));
    struct Collect {
        int open = 0;
        QList<CatalogFeed> sources;
    };
    const auto collect = std::make_shared<Collect>();
    const auto finish = [this, collect] {
        m_catalog->setFeeds(collect->sources);
        m_catalog->refresh([this](bool ok, const QString &) {
            m_refreshFailed = !ok;
            if (ok)
                applyRevocations();
            emit installedChanged(); // verification of installed plugins may have changed
            emit storeChanged();
        });
    };
    for (const InstalledPlugin &plugin : feeds) {
        const std::shared_ptr<PluginWorker> worker = m_manager->load(plugin.id);
        if (!worker)
            continue;
        ++collect->open;
        PluginStoreFeed(worker).feedSources(this, [collect, finish](const QList<PluginFeedSource> &sources,
                                                                    const PluginError &error) {
            if (error.ok()) {
                for (const PluginFeedSource &source : sources)
                    collect->sources << CatalogFeed{source.name, source.url};
            }
            if (--collect->open == 0)
                finish();
        });
    }
    if (collect->open == 0)
        finish();
}

// --- Install --------------------------------------------------------------------------------

void PluginsViewModel::installFromFile(const QUrl &file) {
    if (m_busy > 0 || m_step != InstallStep::None)
        return;
    const QString path = file.isLocalFile() ? file.toLocalFile() : file.toString();
    QFile source(path);
    if (!source.open(QIODevice::ReadOnly)) {
        emit message(Error, tr("The file could not be read."));
        return;
    }
    const QByteArray bytes = source.read(PluginPackage::MaxPackageBytes + 1);
    beginBusy();
    // The catalog decides "verified": always ask for the current one first.
    m_catalog->refresh([this, bytes](bool ok, const QString &) {
        m_refreshFailed = !ok;
        if (ok)
            applyRevocations();
        endBusy();
        prepare(bytes, {}, {});
    });
}

void PluginsViewModel::installFromStore(const QString &id, const QString &feed) {
    if (m_busy > 0 || m_step != InstallStep::None)
        return;
    std::optional<CatalogEntry> entry;
    for (const CatalogEntry &candidate : m_catalog->entries()) {
        if (candidate.id == id && candidate.feed == feed && !candidate.revoked) {
            entry = candidate;
            break;
        }
    }
    if (!entry) {
        emit message(Error, tr("This plugin is not in the plugin list any more."));
        return;
    }
    beginBusy();
    const QByteArray expected = entry->sha256;
    m_catalog->download(*entry, [this, expected, feed](const QByteArray &bytes, const QString &error) {
        endBusy();
        if (!error.isEmpty()) {
            emit message(Error, tr("The plugin could not be downloaded."));
            return;
        }
        prepare(bytes, expected, feed);
    });
}

void PluginsViewModel::prepare(const QByteArray &packageBytes, const QByteArray &expectedSha256,
                               const QString &feed) {
    InstallCandidate candidate;
    const PluginResult result = m_manager->prepareInstall(packageBytes, &candidate, expectedSha256);
    if (!result.ok()) {
        emit message(Error, describe(result));
        return;
    }
    setPending(std::move(candidate), feed);
}

void PluginsViewModel::setPending(InstallCandidate candidate, const QString &feed) {
    m_candidate = std::move(candidate);
    const PluginManifest &manifest = m_candidate.package.manifest;
    const PluginInspection &inspection = m_candidate.inspection;

    // The catalog is authoritative for display: warn if it lists this very file for other systems.
    QString osWarning;
    if (const auto entry = m_catalog->officialEntryByHash(m_candidate.package.sha256);
        entry && !entry->supportsOs(m_config.os)) {
        QStringList names;
        for (const QString &os : entry->os)
            names << osName(os);
        osWarning = tr("The plugin list names this plugin for %1 only. It may not work here.")
                        .arg(names.join(QStringLiteral(", ")));
    }

    m_pending = QVariantMap{
        {QStringLiteral("id"), manifest.id},
        {QStringLiteral("name"), manifest.name},
        {QStringLiteral("version"), manifest.version},
        {QStringLiteral("author"), manifest.author},
        // Computed from the component, never from the manifest:
        {QStringLiteral("types"), typeTexts(inspection.types)},
        {QStringLiteral("permissions"), permissionTexts(inspection.permissions, manifest.networkHosts)},
        {QStringLiteral("warnings"), riskWarnings(inspection.types, inspection.permissions, manifest.networkHosts)},
        {QStringLiteral("verification"), verificationValue(m_candidate.verification)},
        {QStringLiteral("verificationText"), verificationText(m_candidate.verification)},
        {QStringLiteral("isUpdate"), m_candidate.isUpdate},
        {QStringLiteral("installedVersion"), m_candidate.installedVersion},
        {QStringLiteral("osWarning"), osWarning},
        {QStringLiteral("feed"), feed},
    };
    m_step = m_candidate.verification == PluginVerification::Verified ? InstallStep::Permissions
                                                                      : InstallStep::UnverifiedWarning;
    emit pendingInstallChanged();
}

void PluginsViewModel::clearPending() {
    if (m_step == InstallStep::None && m_pending.isEmpty())
        return;
    m_candidate = InstallCandidate();
    m_pending.clear();
    m_step = InstallStep::None;
    emit pendingInstallChanged();
}

void PluginsViewModel::confirmUnverified() {
    if (m_step != InstallStep::UnverifiedWarning)
        return;
    m_step = InstallStep::Permissions;
    emit pendingInstallChanged();
}

void PluginsViewModel::approveInstall() {
    if (m_step != InstallStep::Permissions)
        return;
    const QString name = m_candidate.package.manifest.name;
    const bool update = m_candidate.isUpdate;
    if (update && refuseWhileDatabaseOpen(m_candidate.package.manifest.id)) {
        clearPending();
        return;
    }
    m_manager->unload(m_candidate.package.manifest.id); // an update replaces the running plugin
    const PluginResult result = m_manager->install(m_candidate, m_candidate.inspection.permissions);
    clearPending();
    if (!result.ok()) {
        emit message(Error, describe(result));
        return;
    }
    emit message(Success, update ? tr("\"%1\" was updated.").arg(name) : tr("\"%1\" was installed.").arg(name));
}

void PluginsViewModel::cancelInstall() { clearPending(); }

// --- Installed plugins ----------------------------------------------------------------------

void PluginsViewModel::requestUninstall(const QString &id) {
    const std::optional<InstalledPlugin> plugin = m_manager->plugin(id);
    if (!plugin)
        return;
    if (!m_config.dialogs) {
        uninstall(id);
        return;
    }
    const bool removeData = !m_config.settings || m_config.settings->removePluginSettingsOnUninstall();
    const QString text = tr("Do you really want to uninstall \"%1\"?").arg(plugin->name) + QLatin1Char('\n') +
                         (removeData ? tr("Its settings and files will be deleted too.")
                                     : tr("Its settings and files will be kept."));
    m_config.dialogs->confirm(tr("Uninstall plugin"), text, tr("Uninstall"), [this, id](DialogService::Result r) {
        if (r == DialogService::Result::Primary)
            uninstall(id);
    });
}

bool PluginsViewModel::uninstall(const QString &id) {
    if (refuseWhileDatabaseOpen(id))
        return false;
    const std::optional<InstalledPlugin> plugin = m_manager->plugin(id);
    const PluginResult result = m_manager->uninstall(id);
    if (!result.ok()) {
        emit message(Error, describe(result));
        return false;
    }
    if (m_configPluginId == id)
        closeProviderConfig();
    emit message(Success, tr("\"%1\" was uninstalled.").arg(plugin ? plugin->name : id));
    return true;
}

bool PluginsViewModel::setEnabled(const QString &id, bool enabled) {
    if (!enabled && refuseWhileDatabaseOpen(id)) {
        emit installedChanged(); // the switch in the list goes back
        return false;
    }
    const std::optional<InstalledPlugin> plugin = m_manager->plugin(id);
    PluginResult result;
    if (enabled && plugin && m_catalog->verify(plugin->sha256) == PluginVerification::Revoked)
        result = {PluginResult::Code::Revoked, {}};
    else
        result = m_manager->setEnabled(id, enabled);
    if (!enabled)
        m_manager->unload(id);
    if (!result.ok()) {
        emit message(Error, describe(result));
        emit installedChanged(); // the switch in the list goes back
        return false;
    }
    return true;
}

bool PluginsViewModel::setAllowPrivateNetwork(const QString &id, bool allow) {
    const PluginResult result = m_manager->setAllowPrivateNetwork(id, allow);
    if (!result.ok()) {
        emit message(Error, describe(result));
        return false;
    }
    m_manager->unload(id); // takes effect with the next start of the plugin
    return true;
}

bool PluginsViewModel::openWebsite(const QString &id) {
    const std::optional<InstalledPlugin> plugin = m_manager->plugin(id);
    if (!plugin)
        return false;
    const QUrl url(plugin->homepage, QUrl::StrictMode);
    const QString scheme = url.scheme().toLower();
    if (!url.isValid() || url.host().isEmpty() ||
        !(scheme == QLatin1String("https") || scheme == QLatin1String("http")))
        return false;
    if (!m_opener || !m_opener(url))
        return false;
    emit urlOpened(url.toString());
    return true;
}

// --- File dialog ----------------------------------------------------------------------------

void PluginsViewModel::beginFilePick(const std::shared_ptr<PickRequest> &request, const QString &pluginName,
                                     const QString &title, const QStringList &extensions) {
    const int id = m_nextPickId++;
    m_picks.insert(id, request);
    QStringList filters;
    if (!extensions.isEmpty()) {
        QStringList patterns;
        for (const QString &extension : extensions)
            patterns << QStringLiteral("*.") + extension;
        filters << tr("Files for the plugin (%1)").arg(patterns.join(QLatin1Char(' ')));
    }
    filters << tr("All files (*)");
    emit filePickRequested(id, pluginName, title, filters);
}

void PluginsViewModel::resolveFilePick(int requestId, const QUrl &file) {
    const std::shared_ptr<PickRequest> request = m_picks.take(requestId);
    if (!request)
        return;
    {
        QMutexLocker lock(&request->mutex);
        request->path = file.isEmpty() ? QString() : file.isLocalFile() ? file.toLocalFile() : file.toString();
    }
    request->done.release();
}

// --- Import ---------------------------------------------------------------------------------

QStringList PluginsViewModel::importCandidates() const {
    QStringList names;
    for (const PasswordItem &item : m_importItems)
        names << item.displayName;
    return names;
}

void PluginsViewModel::loadImportSources() {
    const quint64 generation = ++m_importGeneration;
    m_importSources.clear();
    emit importSourcesChanged();
    for (const InstalledPlugin &plugin : enabledOfType(QStringLiteral("importer"))) {
        PluginResult result;
        const std::shared_ptr<PluginWorker> worker = m_manager->load(plugin.id, &result);
        if (!worker) {
            emit message(Error, plugin.name + QStringLiteral(": ") + describe(result));
            continue;
        }
        beginBusy();
        PluginImporter(worker).sources(this, [this, generation, plugin](const QList<PluginImportSource> &sources,
                                                                        const PluginError &error) {
            endBusy();
            if (generation != m_importGeneration)
                return;
            if (!error.ok()) {
                emit message(Error, plugin.name + QStringLiteral(": ") + describe(error));
                return;
            }
            for (const PluginImportSource &source : sources) {
                m_importSources << QVariantMap{
                    {QStringLiteral("pluginId"), plugin.id},
                    {QStringLiteral("pluginName"), plugin.name},
                    {QStringLiteral("sourceId"), source.id},
                    {QStringLiteral("name"), source.name},
                    {QStringLiteral("description"), source.description},
                };
            }
            emit importSourcesChanged();
        });
    }
}

void PluginsViewModel::importFrom(const QString &pluginId, const QString &sourceId) {
    if (!m_config.service || !m_config.service->isOpen()) {
        emit message(Error, DatabaseOpResult::describe(DatabaseOpStatus::NoDatabaseOpen));
        return;
    }
    PluginResult result;
    const std::shared_ptr<PluginWorker> worker = m_manager->load(pluginId, &result);
    if (!worker) {
        emit message(Error, describe(result));
        return;
    }
    cancelImport();
    const quint64 generation = ++m_importGeneration;
    beginBusy();
    PluginImporter(worker).importPasswords(sourceId, this, [this, generation](const QList<PasswordItem> &items,
                                                                             const PluginError &error) {
        endBusy();
        if (generation != m_importGeneration || !m_config.service->isOpen())
            return; // locked or cancelled meanwhile: drop the items
        if (!error.ok()) {
            if (!(error.kind == PluginError::Kind::Plugin && error.code == QLatin1String("cancelled")))
                emit message(Error, describe(error));
            return;
        }
        if (items.isEmpty()) {
            emit message(Info, tr("The plugin found no passwords."));
            return;
        }
        m_importItems = items;
        emit importCandidatesChanged();
        emit importReady();
    });
}

bool PluginsViewModel::importSelected(const QVariantList &indexes, bool overrideExisting) {
    QList<PasswordItem> items;
    for (const QVariant &index : indexes) {
        const int i = index.toInt();
        if (i >= 0 && i < m_importItems.size())
            items << m_importItems.at(i);
    }
    if (items.isEmpty()) {
        emit message(Warning, tr("No passwords selected"));
        return false;
    }
    DatabaseService *service = m_config.service;
    const bool ok = service && service->importItems(items, overrideExisting ? DatabaseService::ImportMode::Override
                                                                             : DatabaseService::ImportMode::Append);
    if (!ok) {
        emit message(Error, DatabaseOpResult::describe(service && service->isOpen() ? DatabaseOpStatus::ReadOnly
                                                                                     : DatabaseOpStatus::NoDatabaseOpen));
        return false;
    }
    cancelImport();
    emit message(Success, tr("%n password(s) imported", nullptr, int(items.size())));
    return true;
}

void PluginsViewModel::cancelImport() {
    ++m_importGeneration; // a running import is ignored when it comes back
    if (m_importItems.isEmpty())
        return;
    m_importItems.clear();
    emit importCandidatesChanged();
}

// --- Remote database settings ---------------------------------------------------------------

void PluginsViewModel::loadProviders() {
    const quint64 generation = ++m_providerGeneration;
    m_providers.clear();
    emit providersChanged();
    for (const InstalledPlugin &plugin : enabledOfType(QStringLiteral("database-source"))) {
        PluginResult result;
        const std::shared_ptr<PluginWorker> worker = m_manager->load(plugin.id, &result);
        if (!worker) {
            emit message(Error, plugin.name + QStringLiteral(": ") + describe(result));
            continue;
        }
        beginBusy();
        PluginDatabaseProvider(worker).providerName(this, [this, generation, plugin](const QString &name,
                                                                                    const PluginError &error) {
            endBusy();
            if (generation != m_providerGeneration)
                return;
            // A provider that cannot tell its name is still listed, under the plugin's name.
            m_providers << QVariantMap{
                {QStringLiteral("pluginId"), plugin.id},
                {QStringLiteral("name"), error.ok() && !name.trimmed().isEmpty() ? name.left(80) : plugin.name},
            };
            emit providersChanged();
        });
    }
}

QVariantList PluginsViewModel::configFieldsFromSchema(const QByteArray &schemaJson, const QByteArray &configJson,
                                                      QString *error) {
    const auto fail = [&](const QString &text) {
        if (error)
            *error = text;
        return QVariantList();
    };
    const QJsonDocument schemaDocument = QJsonDocument::fromJson(schemaJson);
    if (!schemaDocument.isObject() || schemaDocument.object().value(QStringLiteral("type")).toString() !=
                                          QLatin1String("object"))
        return fail(tr("The plugin's settings description is not valid."));
    const QJsonObject schema = schemaDocument.object();
    const QJsonObject properties = schema.value(QStringLiteral("properties")).toObject();
    if (properties.size() > MaxConfigFields)
        return fail(tr("The plugin has too many settings."));
    const QJsonObject current = QJsonDocument::fromJson(configJson).object();
    QStringList required;
    for (const QJsonValue &name : schema.value(QStringLiteral("required")).toArray())
        required << name.toString();

    QVariantList fields;
    for (auto it = properties.constBegin(); it != properties.constEnd(); ++it) {
        const QJsonObject property = it.value().toObject();
        const QString type = property.value(QStringLiteral("type")).toString();
        QString kind;
        QVariant value;
        const QJsonValue stored = current.contains(it.key()) ? current.value(it.key())
                                                             : property.value(QStringLiteral("default"));
        QStringList options;
        if (type == QLatin1String("string")) {
            for (const QJsonValue &option : property.value(QStringLiteral("enum")).toArray()) {
                if (option.isString())
                    options << option.toString();
            }
            kind = !options.isEmpty() ? QStringLiteral("choice")
                   : property.value(QStringLiteral("format")).toString() == QLatin1String("password")
                       ? QStringLiteral("password")
                       : QStringLiteral("string");
            value = stored.toString();
        } else if (type == QLatin1String("boolean")) {
            kind = QStringLiteral("boolean");
            value = stored.toBool();
        } else if (type == QLatin1String("integer")) {
            kind = QStringLiteral("integer");
            value = stored.toInteger();
        } else if (type == QLatin1String("number")) {
            kind = QStringLiteral("number");
            value = stored.toDouble();
        } else {
            return fail(tr("The plugin has a setting (\"%1\") that Ease Pass cannot show.").arg(it.key().left(80)));
        }
        const QString title = property.value(QStringLiteral("title")).toString();
        QVariantMap field{
            {QStringLiteral("key"), it.key()},
            {QStringLiteral("title"), title.isEmpty() ? it.key() : title.left(120)},
            {QStringLiteral("description"), property.value(QStringLiteral("description")).toString().left(500)},
            {QStringLiteral("kind"), kind},
            {QStringLiteral("value"), value},
            {QStringLiteral("options"), options},
            {QStringLiteral("required"), required.contains(it.key())},
            {QStringLiteral("maxLength"), property.value(QStringLiteral("maxLength")).toInt(4096)},
        };
        if (property.value(QStringLiteral("minimum")).isDouble())
            field.insert(QStringLiteral("minimum"), property.value(QStringLiteral("minimum")).toDouble());
        if (property.value(QStringLiteral("maximum")).isDouble())
            field.insert(QStringLiteral("maximum"), property.value(QStringLiteral("maximum")).toDouble());
        fields << field;
    }
    return fields;
}

QByteArray PluginsViewModel::configJsonFromValues(const QVariantList &fields, const QVariantMap &values,
                                                  QString *error) {
    QJsonObject object;
    for (const QVariant &entry : fields) {
        const QVariantMap field = entry.toMap();
        const QString key = field.value(QStringLiteral("key")).toString();
        const QString kind = field.value(QStringLiteral("kind")).toString();
        const QVariant value = values.contains(key) ? values.value(key) : field.value(QStringLiteral("value"));
        if (kind == QLatin1String("boolean")) {
            object.insert(key, value.toBool());
        } else if (kind == QLatin1String("integer") || kind == QLatin1String("number")) {
            bool ok = false;
            const double number = value.toString().trimmed().replace(QLatin1Char(','), QLatin1Char('.')).toDouble(&ok);
            const bool whole = kind == QLatin1String("number") || number == qRound64(number);
            if (!ok || !whole || (field.contains(QStringLiteral("minimum")) &&
                                  number < field.value(QStringLiteral("minimum")).toDouble()) ||
                (field.contains(QStringLiteral("maximum")) && number > field.value(QStringLiteral("maximum")).toDouble())) {
                if (error)
                    *error = tr("\"%1\" needs a valid number.").arg(field.value(QStringLiteral("title")).toString());
                return QByteArray();
            }
            if (kind == QLatin1String("integer"))
                object.insert(key, qint64(qRound64(number)));
            else
                object.insert(key, number);
        } else {
            const QString text = value.toString();
            if (text.isEmpty() && field.value(QStringLiteral("required")).toBool()) {
                if (error)
                    *error = tr("\"%1\" must not be empty.").arg(field.value(QStringLiteral("title")).toString());
                return QByteArray();
            }
            object.insert(key, text);
        }
    }
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

void PluginsViewModel::openProviderConfig(const QString &pluginId) {
    PluginResult result;
    const std::shared_ptr<PluginWorker> worker = m_manager->load(pluginId, &result);
    if (!worker) {
        emit message(Error, describe(result));
        return;
    }
    closeProviderConfig();
    const quint64 generation = ++m_configGeneration;
    beginBusy();
    PluginDatabaseProvider(worker).configSchema(this, [this, worker, pluginId, generation](const QString &schema,
                                                                                          const PluginError &error) {
        if (!error.ok() || generation != m_configGeneration) {
            endBusy();
            if (!error.ok())
                emit message(Error, describe(error));
            return;
        }
        PluginDatabaseProvider(worker).config(this, [this, pluginId, schema, generation](const QString &config,
                                                                                         const PluginError &error) {
            endBusy();
            if (generation != m_configGeneration)
                return;
            if (!error.ok()) {
                emit message(Error, describe(error));
                return;
            }
            QString problem;
            const QVariantList fields = configFieldsFromSchema(schema.toUtf8(), config.toUtf8(), &problem);
            if (!problem.isEmpty()) {
                emit message(Error, problem);
                return;
            }
            m_configFields = fields;
            m_configPluginId = pluginId;
            emit configChanged();
            emit configReady();
        });
    });
}

void PluginsViewModel::saveProviderConfig(const QVariantMap &values) {
    if (m_configPluginId.isEmpty())
        return;
    QString problem;
    const QByteArray json = configJsonFromValues(m_configFields, values, &problem);
    if (json.isEmpty()) {
        emit message(Error, problem);
        emit configSaved(false);
        return;
    }
    PluginResult result;
    const std::shared_ptr<PluginWorker> worker = m_manager->load(m_configPluginId, &result);
    if (!worker) {
        emit message(Error, describe(result));
        emit configSaved(false);
        return;
    }
    beginBusy();
    PluginDatabaseProvider(worker).setConfig(QString::fromUtf8(json), this, [this](const PluginError &error) {
        endBusy();
        if (!error.ok()) {
            emit message(Error, describe(error));
            emit configSaved(false);
            return;
        }
        emit message(Success, tr("Settings saved"));
        emit configSaved(true);
        refreshRemoteDatabases(); // the settings decide which databases there are
    });
}

void PluginsViewModel::closeProviderConfig() {
    ++m_configGeneration;
    if (m_configPluginId.isEmpty() && m_configFields.isEmpty())
        return;
    m_configFields.clear();
    m_configPluginId.clear();
    emit configChanged();
}

// --- Databases and generators of plugins ------------------------------------------------------

QList<RemoteDatabase> PluginsViewModel::remoteDatabases() const {
    QList<RemoteDatabase> result;
    for (const RemoteEntry &entry : m_remote)
        result << entry.database;
    return result;
}

QString PluginsViewModel::openDatabasePlugin() const {
    if (!m_config.service || !m_config.service->isOpen())
        return QString();
    for (const RemoteEntry &entry : m_remote) {
        if (entry.database.source.get() == m_config.service->source())
            return entry.pluginId;
    }
    return QString();
}

bool PluginsViewModel::refuseWhileDatabaseOpen(const QString &pluginId) {
    if (pluginId.isEmpty() || openDatabasePlugin() != pluginId)
        return false;
    emit message(Error, tr("The open database is stored with this plugin. Close the database first."));
    return true;
}

void PluginsViewModel::refreshRemoteDatabases() {
    const quint64 generation = ++m_remoteGeneration;
    m_remoteIncoming.clear();
    const QList<InstalledPlugin> plugins = enabledOfType(QStringLiteral("database-source"));
    m_remotePending = int(plugins.size());
    if (plugins.isEmpty()) {
        finishRemoteRefresh();
        return;
    }
    emit remoteDatabasesChanged(); // remoteLoading
    for (const InstalledPlugin &plugin : plugins) {
        PluginResult result;
        const std::shared_ptr<PluginWorker> worker = m_manager->load(plugin.id, &result);
        if (generation != m_remoteGeneration)
            return; // loading changed the installed plugins (e.g. a tampered file): a new refresh runs
        if (!worker) {
            emit message(Warning, plugin.name + QStringLiteral(": ") + describe(result));
            if (--m_remotePending == 0)
                finishRemoteRefresh();
            continue;
        }
        PluginDatabaseProvider(worker).listDatabases(
            this, [this, generation, plugin](const QList<std::shared_ptr<PluginDatabaseSource>> &sources,
                                             const PluginError &error) {
                if (generation != m_remoteGeneration)
                    return;
                if (!error.ok())
                    emit message(Warning, tr("%1 could not list its databases. %2").arg(plugin.name, describe(error)));
                // Names and descriptions are the plugin's words: shortened, one line, and always
                // shown together with the plugin's name.
                for (const auto &source : sources.mid(0, MaxDatabasesPerPlugin)) {
                    const QString name = source->databaseName().simplified().left(80);
                    RemoteEntry entry;
                    entry.pluginId = plugin.id;
                    entry.database.key = RemoteDatabase::KeyPrefix + plugin.id + QLatin1Char('/') +
                                         QString::fromLatin1(QUrl::toPercentEncoding(source->databaseId()));
                    entry.database.name = name.isEmpty() ? plugin.name : tr("%1 (%2)").arg(name, plugin.name);
                    entry.database.description = source->sourceDescription().simplified().left(200);
                    entry.database.source = source;
                    m_remoteIncoming << entry;
                }
                if (--m_remotePending == 0)
                    finishRemoteRefresh();
            });
    }
}

void PluginsViewModel::finishRemoteRefresh() {
    m_remotePending = 0;
    // The open database keeps its object: the pages recognize the open database by it.
    const IDatabaseSource *open =
        m_config.service && m_config.service->isOpen() ? m_config.service->source() : nullptr;
    bool closeDatabase = false;
    for (const RemoteEntry &old : std::as_const(m_remote)) {
        if (!open || old.database.source.get() != open)
            continue;
        bool listed = false;
        for (RemoteEntry &entry : m_remoteIncoming) {
            if (entry.database.key == old.database.key) {
                entry.database.source = old.database.source;
                listed = true;
            }
        }
        if (listed)
            continue;
        const std::optional<InstalledPlugin> plugin = m_manager->plugin(old.pluginId);
        if (plugin && plugin->enabled)
            m_remoteIncoming << old; // not listed this time (e.g. offline): stays while it is open
        else
            closeDatabase = true; // e.g. the plugin was revoked
    }
    m_remote = m_remoteIncoming;
    m_remoteIncoming.clear();
    emit remoteDatabasesChanged();
    if (closeDatabase) {
        m_config.service->lock();
        emit message(Warning, tr("The database was closed because its plugin is no longer available."));
    }
}

QList<PasswordGeneratorViewModel::ExternalGenerator> PluginsViewModel::passwordGenerators() {
    QList<PasswordGeneratorViewModel::ExternalGenerator> result;
    for (const InstalledPlugin &plugin : enabledOfType(QStringLiteral("password-generator"))) {
        PasswordGeneratorViewModel::ExternalGenerator generator;
        generator.id = plugin.id;
        generator.name = plugin.name;
        QPointer<PluginsViewModel> self(this);
        const QString id = plugin.id;
        generator.generate = [self, id](int length, const QString &characters, bool verifyNotLeaked,
                                        PasswordGeneratorViewModel::ExternalGenerator::Done done) {
            PluginResult result;
            const std::shared_ptr<PluginWorker> worker = self ? self->m_manager->load(id, &result) : nullptr;
            if (!worker) {
                done(QString(), self ? describe(result) : tr("This plugin is not installed."));
                return;
            }
            PluginPasswordGenerator(worker).generate(length, characters, verifyNotLeaked, self,
                                                     [done](const QString &password, const PluginError &error) {
                                                         if (error.ok())
                                                             done(password, QString());
                                                         else
                                                             done(QString(), describe(error));
                                                     });
        };
        result << generator;
    }
    return result;
}

} // namespace EasePass::App
