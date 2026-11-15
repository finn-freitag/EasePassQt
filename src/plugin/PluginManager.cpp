#include "PluginManager.h"

#include "FilePermissions.h"
#include "LeakCheckService.h"
#include "PluginInspector.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QSaveFile>
#include <QMutex>
#include <QPointer>
#include <QSemaphore>

Q_LOGGING_CATEGORY(lcPlugin, "easepass.plugin")

namespace EasePass::Plugin {

namespace {

constexpr int ApiVersion = PluginInspector::ApiVersion;
constexpr int LeakCheckTimeoutMs = 15'000;

PluginResult failure(PluginResult::Code code, const QString &message) { return {code, message}; }

QString verificationName(PluginVerification verification) {
    switch (verification) {
    case PluginVerification::Verified:
        return QStringLiteral("verified");
    case PluginVerification::Unverified:
        return QStringLiteral("unverified");
    case PluginVerification::Revoked:
        return QStringLiteral("revoked");
    case PluginVerification::Offline:
        break;
    }
    return QStringLiteral("offline");
}

PluginVerification verificationFromName(const QString &name) {
    for (PluginVerification v : {PluginVerification::Verified, PluginVerification::Unverified,
                                 PluginVerification::Revoked, PluginVerification::Offline}) {
        if (verificationName(v) == name)
            return v;
    }
    return PluginVerification::Offline;
}

QStringList toStringList(const QJsonValue &value) {
    QStringList list;
    for (const QJsonValue &item : value.toArray())
        list << item.toString();
    return list;
}

QJsonObject toJson(const InstalledPlugin &plugin) {
    return {{QStringLiteral("id"), plugin.id},
            {QStringLiteral("name"), plugin.name},
            {QStringLiteral("version"), plugin.version},
            {QStringLiteral("description"), plugin.description},
            {QStringLiteral("author"), plugin.author},
            {QStringLiteral("homepage"), plugin.homepage},
            {QStringLiteral("sha256"), QString::fromLatin1(plugin.sha256)},
            {QStringLiteral("types"), QJsonArray::fromStringList(plugin.types)},
            {QStringLiteral("permissions"), QJsonArray::fromStringList(plugin.permissions)},
            {QStringLiteral("approved_permissions"), QJsonArray::fromStringList(plugin.approvedPermissions)},
            {QStringLiteral("network_hosts"), QJsonArray::fromStringList(plugin.networkHosts)},
            {QStringLiteral("enabled"), plugin.enabled},
            {QStringLiteral("allow_private_network"), plugin.allowPrivateNetwork},
            {QStringLiteral("verification"), verificationName(plugin.verification)},
            {QStringLiteral("installed_at"), plugin.installedAt.toUTC().toString(Qt::ISODate)}};
}

std::optional<InstalledPlugin> fromJson(const QJsonObject &object) {
    InstalledPlugin plugin;
    plugin.id = object.value(QStringLiteral("id")).toString();
    plugin.sha256 = object.value(QStringLiteral("sha256")).toString().toLatin1();
    // The id becomes part of file paths: never accept one that is not a plain plugin id.
    if (!PluginManifest::isValidId(plugin.id) || plugin.sha256.size() != 64)
        return std::nullopt;
    plugin.name = object.value(QStringLiteral("name")).toString();
    plugin.version = object.value(QStringLiteral("version")).toString();
    plugin.description = object.value(QStringLiteral("description")).toString();
    plugin.author = object.value(QStringLiteral("author")).toString();
    plugin.homepage = object.value(QStringLiteral("homepage")).toString();
    plugin.types = toStringList(object.value(QStringLiteral("types")));
    plugin.permissions = toStringList(object.value(QStringLiteral("permissions")));
    plugin.approvedPermissions = toStringList(object.value(QStringLiteral("approved_permissions")));
    plugin.networkHosts = toStringList(object.value(QStringLiteral("network_hosts")));
    plugin.enabled = object.value(QStringLiteral("enabled")).toBool(true);
    plugin.allowPrivateNetwork = object.value(QStringLiteral("allow_private_network")).toBool(false);
    plugin.verification = verificationFromName(object.value(QStringLiteral("verification")).toString());
    plugin.installedAt = QDateTime::fromString(object.value(QStringLiteral("installed_at")).toString(), Qt::ISODate);
    return plugin;
}

bool writeOwnerOnly(const QString &path, const QByteArray &data) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    Core::restrictToOwner(file);
    return file.write(data) == data.size() && file.commit();
}

/// Checks that are the same at install time and at load time.
PluginResult checkPackage(const PluginPackage &package, const PluginInspection &inspection,
                          const QString &appVersion) {
    const PluginManifest &manifest = package.manifest;
    if (manifest.apiVersion != ApiVersion)
        return failure(PluginResult::Code::Unsupported,
                       QStringLiteral("the plugin needs plugin API version %1").arg(manifest.apiVersion));
    if (!manifest.supportsCurrentOs())
        return failure(PluginResult::Code::Unsupported,
                       QStringLiteral("the plugin does not support this operating system"));
    if (!manifest.minAppVersion.isEmpty() && !appVersion.isEmpty() &&
        PluginManifest::compareVersions(appVersion, manifest.minAppVersion) < 0)
        return failure(PluginResult::Code::Unsupported,
                       QStringLiteral("the plugin needs Ease Pass %1 or newer").arg(manifest.minAppVersion));
    if (!inspection.accepted())
        return failure(PluginResult::Code::Rejected, inspection.rejected.join(QStringLiteral("; ")));
    if (manifest.permissions) {
        QStringList declared = *manifest.permissions, computed = inspection.permissions;
        declared.sort();
        computed.sort();
        if (declared != computed)
            return failure(PluginResult::Code::Rejected,
                           QStringLiteral("the manifest lists other permissions than the plugin really uses"));
    }
    return {};
}

} // namespace

PluginManager::PluginManager(PluginEnvironment environment, QObject *parent)
    : QObject(parent), m_environment(std::move(environment)) {
    readRecords();
}

PluginManager::~PluginManager() {
    // No plugin thread outlives the manager: a thread that is still compiling or running a
    // plugin when the process ends crashes it.
    for (const std::weak_ptr<PluginWorker> &started : std::as_const(m_started)) {
        if (const std::shared_ptr<PluginWorker> worker = started.lock())
            worker->shutdown();
    }
}

QString PluginManager::packagePath(const QString &id) const {
    return QDir(m_environment.pluginsDirectory).filePath(QStringLiteral("packages/%1.epplugin").arg(id));
}

QString PluginManager::storageDirectory(const QString &id) const {
    return QDir(m_environment.pluginsDirectory).filePath(QStringLiteral("storage/%1").arg(id));
}

void PluginManager::readRecords() {
    m_plugins.clear();
    QFile file(QDir(m_environment.pluginsDirectory).filePath(QStringLiteral("installed.json")));
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonArray records = QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("plugins")).toArray();
    for (const QJsonValue &record : records) {
        const std::optional<InstalledPlugin> installed = fromJson(record.toObject());
        if (installed && !plugin(installed->id))
            m_plugins << *installed;
    }
}

bool PluginManager::writeRecords(QString *error) const {
    QJsonArray records;
    for (const InstalledPlugin &installed : m_plugins)
        records << toJson(installed);
    const QJsonObject root{{QStringLiteral("schema"), 1}, {QStringLiteral("plugins"), records}};
    if (!QDir().mkpath(m_environment.pluginsDirectory) ||
        !writeOwnerOnly(QDir(m_environment.pluginsDirectory).filePath(QStringLiteral("installed.json")),
                        QJsonDocument(root).toJson())) {
        if (error)
            *error = QStringLiteral("cannot write the plugin list");
        return false;
    }
    return true;
}

QList<InstalledPlugin> PluginManager::installedPlugins() const { return m_plugins; }

std::optional<InstalledPlugin> PluginManager::plugin(const QString &id) const {
    for (const InstalledPlugin &installed : m_plugins) {
        if (installed.id == id)
            return installed;
    }
    return std::nullopt;
}

QByteArray PluginManager::icon(const QString &id) const {
    const std::optional<InstalledPlugin> installed = plugin(id);
    QFile file(packagePath(id));
    if (!installed || !file.open(QIODevice::ReadOnly))
        return {};
    const QByteArray bytes = file.readAll();
    if (PluginPackage::hash(bytes) != installed->sha256)
        return {};
    const std::optional<PluginPackage> package = PluginPackage::parse(bytes, nullptr);
    return package ? package->icon : QByteArray();
}

PluginResult PluginManager::prepareInstall(const QByteArray &packageBytes, InstallCandidate *candidate,
                                           const QByteArray &expectedSha256) const {
    using Code = PluginResult::Code;
    if (packageBytes.size() > PluginPackage::MaxPackageBytes)
        return failure(Code::TooLarge, QStringLiteral("the package is larger than 16 MiB"));

    // The hash comes first: nothing of the file is parsed before it is known and checked.
    const QByteArray sha256 = PluginPackage::hash(packageBytes);
    if (!expectedSha256.isEmpty() && sha256 != expectedSha256.toLower())
        return failure(Code::HashMismatch, QStringLiteral("the downloaded file does not match the plugin list"));
    const PluginVerification verification = m_catalog ? m_catalog->verify(sha256) : PluginVerification::Offline;
    if (verification == PluginVerification::Revoked)
        return failure(Code::Revoked, QStringLiteral("this plugin was withdrawn and cannot be installed"));

    QString parseError;
    const std::optional<PluginPackage> package = PluginPackage::parse(packageBytes, &parseError);
    if (!package)
        return failure(Code::InvalidPackage, parseError);

    const PluginInspection inspection = PluginInspector::inspect(package->wasm);
    const PluginResult checked = checkPackage(*package, inspection, m_environment.appVersion);
    if (!checked.ok())
        return checked;

    candidate->packageBytes = packageBytes;
    candidate->package = *package;
    candidate->inspection = inspection;
    candidate->verification = verification;
    const std::optional<InstalledPlugin> existing = plugin(package->manifest.id);
    candidate->isUpdate = existing.has_value();
    candidate->installedVersion = existing ? existing->version : QString();
    return {};
}

PluginResult PluginManager::install(const InstallCandidate &candidate, const QStringList &approvedPermissions) {
    using Code = PluginResult::Code;
    // Never trust the candidate: it may be stale or built by hand. Check the bytes again.
    InstallCandidate checked;
    const PluginResult prepared = prepareInstall(candidate.packageBytes, &checked, candidate.package.sha256);
    if (!prepared.ok())
        return prepared;
    for (const QString &permission : std::as_const(checked.inspection.permissions)) {
        if (!approvedPermissions.contains(permission))
            return failure(Code::NotApproved,
                           QStringLiteral("the permission '%1' was not approved").arg(permission));
    }

    const PluginManifest &manifest = checked.package.manifest;
    unload(manifest.id);
    const QString path = packagePath(manifest.id);
    if (!QDir().mkpath(QFileInfo(path).absolutePath()) || !writeOwnerOnly(path, checked.packageBytes))
        return failure(Code::Io, QStringLiteral("cannot store the plugin package"));

    InstalledPlugin record;
    if (const std::optional<InstalledPlugin> existing = plugin(manifest.id)) {
        // An update keeps the user's switches, but never a network override for new hosts.
        record.enabled = existing->enabled;
        record.allowPrivateNetwork = existing->allowPrivateNetwork && existing->networkHosts == manifest.networkHosts;
    }
    record.id = manifest.id;
    record.name = manifest.name;
    record.version = manifest.version;
    record.description = manifest.description;
    record.author = manifest.author;
    record.homepage = manifest.homepage;
    record.sha256 = checked.package.sha256;
    record.types = checked.inspection.types;
    record.permissions = checked.inspection.permissions;
    record.approvedPermissions = checked.inspection.permissions; // exactly what was computed and confirmed
    record.networkHosts = manifest.networkHosts;
    record.verification = checked.verification;
    record.installedAt = QDateTime::currentDateTimeUtc();

    m_plugins.removeIf([&](const InstalledPlugin &p) { return p.id == record.id; });
    m_plugins << record;
    QString error;
    if (!writeRecords(&error))
        return failure(Code::Io, error);
    emit pluginsChanged();
    return {};
}

PluginResult PluginManager::uninstall(const QString &id) {
    using Code = PluginResult::Code;
    if (!plugin(id))
        return failure(Code::NotInstalled, QStringLiteral("the plugin is not installed"));
    unload(id);
    m_plugins.removeIf([&](const InstalledPlugin &p) { return p.id == id; });
    QString error;
    const bool recordsWritten = writeRecords(&error);
    QFile::remove(packagePath(id));
    const bool removeStorage =
        !m_environment.removeStorageOnUninstall || m_environment.removeStorageOnUninstall();
    if (removeStorage)
        PluginStorage::removeAll(storageDirectory(id));
    emit pluginsChanged();
    return recordsWritten ? PluginResult() : failure(Code::Io, error);
}

PluginResult PluginManager::setEnabled(const QString &id, bool enabled) {
    for (InstalledPlugin &installed : m_plugins) {
        if (installed.id != id)
            continue;
        if (installed.enabled != enabled) {
            installed.enabled = enabled;
            if (!enabled)
                unload(id);
            QString error;
            if (!writeRecords(&error))
                return failure(PluginResult::Code::Io, error);
            emit pluginsChanged();
        }
        return {};
    }
    return failure(PluginResult::Code::NotInstalled, QStringLiteral("the plugin is not installed"));
}

PluginResult PluginManager::setAllowPrivateNetwork(const QString &id, bool allow) {
    for (InstalledPlugin &installed : m_plugins) {
        if (installed.id != id)
            continue;
        if (installed.allowPrivateNetwork != allow) {
            installed.allowPrivateNetwork = allow;
            unload(id); // the policy is fixed when a plugin is loaded
            QString error;
            if (!writeRecords(&error))
                return failure(PluginResult::Code::Io, error);
            emit pluginsChanged();
        }
        return {};
    }
    return failure(PluginResult::Code::NotInstalled, QStringLiteral("the plugin is not installed"));
}

void PluginManager::unload(const QString &id) {
    if (const std::shared_ptr<PluginWorker> worker = m_loaded.take(id))
        worker->shutdown();
}

std::shared_ptr<PluginWorker> PluginManager::load(const QString &id, PluginResult *result) {
    using Code = PluginResult::Code;
    PluginResult ignored;
    if (!result)
        result = &ignored;
    *result = {};
    if (const auto loaded = m_loaded.value(id))
        return loaded;

    const std::optional<InstalledPlugin> record = plugin(id);
    if (!record) {
        *result = failure(Code::NotInstalled, QStringLiteral("the plugin is not installed"));
        return nullptr;
    }
    if (!record->enabled) {
        *result = failure(Code::Disabled, QStringLiteral("the plugin is disabled"));
        return nullptr;
    }
    QFile file(packagePath(id));
    if (!file.open(QIODevice::ReadOnly) || file.size() > PluginPackage::MaxPackageBytes) {
        *result = failure(Code::Tampered, QStringLiteral("the plugin file is missing"));
        return nullptr;
    }
    const QByteArray bytes = file.readAll();

    // 1. The file must be exactly the one the user approved.
    const QByteArray sha256 = PluginPackage::hash(bytes);
    if (sha256 != record->sha256) {
        *result = failure(Code::Tampered, QStringLiteral("the plugin file was changed after it was installed"));
        return nullptr;
    }
    if (m_catalog && m_catalog->verify(sha256) == PluginVerification::Revoked) {
        *result = failure(Code::Revoked, QStringLiteral("this plugin was withdrawn"));
        return nullptr;
    }

    // 2. Parse and inspect again; never rely on what installed.json says the plugin may do.
    QString parseError;
    const std::optional<PluginPackage> package = PluginPackage::parse(bytes, &parseError);
    if (!package) {
        *result = failure(Code::InvalidPackage, parseError);
        return nullptr;
    }
    const PluginInspection inspection = PluginInspector::inspect(package->wasm);
    *result = checkPackage(*package, inspection, m_environment.appVersion);
    if (!result->ok())
        return nullptr;
    for (const QString &permission : inspection.permissions) {
        if (!record->approvedPermissions.contains(permission)) {
            *result = failure(Code::NotApproved,
                              QStringLiteral("the permission '%1' was not approved").arg(permission));
            return nullptr;
        }
    }

    // 3. Host services of this plugin: only what its computed permissions cover.
    const QString name = record->name;
    const PluginEnvironment &environment = m_environment;
    PluginWorker::Setup setup;
    setup.component = package->wasm;
    setup.limits = environment.limits;
    setup.approvedPermissions = inspection.permissions;
    setup.services.pluginId = id;
    setup.services.clock = environment.clock;
    setup.services.log = [id, sink = environment.log](const QString &level, const QString &message) {
        if (sink)
            sink(id, level, message);
        else
            qCInfo(lcPlugin).noquote() << QStringLiteral("[%1] %2: %3").arg(id, level, message);
    };
    if (inspection.permissions.contains(QStringLiteral("storage")))
        setup.services.storage = std::make_shared<PluginStorage>(storageDirectory(id));
    if (inspection.permissions.contains(QStringLiteral("pick-file")) && environment.pickFile) {
        setup.services.pickFile = [id, name, pick = environment.pickFile](const QString &title,
                                                                         const QStringList &extensions) {
            return pick(id, name, title, extensions);
        };
    }
    if (inspection.permissions.contains(QStringLiteral("network"))) {
        PluginNetwork::Policy policy;
        policy.allowedHosts = package->manifest.networkHosts;
        policy.allowPrivateAddresses = record->allowPrivateNetwork;
        policy.resolver = environment.resolver;
        policy.extraCaCertificates = environment.extraCaCertificates;
        setup.services.network = std::make_shared<PluginNetwork>(policy);
    }
    if (inspection.permissions.contains(QStringLiteral("leak-check")))
        setup.services.leakCheck = environment.leakCheck;

    const auto worker = std::make_shared<PluginWorker>(std::move(setup));
    m_loaded.insert(id, worker);
    m_started.removeIf([](const std::weak_ptr<PluginWorker> &started) { return started.expired(); });
    m_started << worker;
    return worker;
}

void PluginManager::startInitializers() {
    for (const InstalledPlugin &installed : std::as_const(m_plugins)) {
        if (!installed.enabled || !installed.types.contains(QStringLiteral("initializer")))
            continue;
        PluginResult result;
        const auto worker = load(installed.id, &result);
        if (!worker) {
            qCWarning(lcPlugin).noquote() << QStringLiteral("[%1] not loaded: %2").arg(installed.id, result.message);
            continue;
        }
        worker->start(this, [id = installed.id](const PluginError &error) {
            if (!error.ok())
                qCWarning(lcPlugin).noquote() << QStringLiteral("[%1] %2").arg(id, error.message);
        });
    }
}

std::function<LeakCheckOutcome(const QString &)> PluginManager::leakCheckWith(Core::LeakCheckService *service) {
    QPointer<Core::LeakCheckService> guard(service);
    return [guard](const QString &password) {
        struct Pending {
            QSemaphore done;
            QMutex mutex;
            LeakCheckOutcome outcome;
        };
        const auto pending = std::make_shared<Pending>();
        if (!guard)
            return LeakCheckOutcome();
        // The service lives on its own (UI) thread: ask there and wait here on the plugin thread.
        QMetaObject::invokeMethod(
            guard.data(),
            [guard, pending, password] {
                if (!guard) {
                    pending->done.release();
                    return;
                }
                guard->check(Core::SecretString(password), [pending](const Core::LeakCheckService::Result &result) {
                    QMutexLocker lock(&pending->mutex);
                    using Status = Core::LeakCheckService::Status;
                    pending->outcome.status = result.status == Status::Leaked      ? QStringLiteral("leaked")
                                              : result.status == Status::NotLeaked ? QStringLiteral("not-leaked")
                                                                                   : QStringLiteral("unknown");
                    pending->outcome.count = result.status == Status::Leaked ? quint64(qMax(0, result.count)) : 0;
                    pending->done.release();
                });
            },
            Qt::QueuedConnection);
        if (!pending->done.tryAcquire(1, LeakCheckTimeoutMs))
            return LeakCheckOutcome();
        QMutexLocker lock(&pending->mutex);
        return pending->outcome;
    };
}

} // namespace EasePass::Plugin
