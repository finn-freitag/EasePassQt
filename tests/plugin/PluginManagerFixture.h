#pragma once

#include "IClock.h"
#include "PluginManager.h"
#include "PluginTestSupport.h"

#include <QHash>
#include <QMutex>
#include <QTemporaryDir>

namespace PluginTest {

struct FakeCatalog : EasePass::Plugin::IPluginCatalog {
    QHash<QByteArray, EasePass::Plugin::PluginVerification> known;
    bool offline = false;
    EasePass::Plugin::PluginVerification verify(const QByteArray &sha256Hex) const override {
        if (offline)
            return EasePass::Plugin::PluginVerification::Offline;
        return known.value(sha256Hex, EasePass::Plugin::PluginVerification::Unverified);
    }
};

struct FixedClock : EasePass::Core::IClock {
    qint64 now = 1'700'000'000;
    qint64 currentSecsSinceEpoch() const override { return now; }
};

/// A PluginManager on a temporary directory with recording host callbacks.
struct ManagerFixture {
    QTemporaryDir directory;
    EasePass::Plugin::PluginEnvironment environment;
    FixedClock clock;
    bool removeStorage = true;
    std::unique_ptr<EasePass::Plugin::PluginManager> manager;

    ManagerFixture() {
        environment.pluginsDirectory = directory.filePath(QStringLiteral("plugins"));
        environment.appVersion = QStringLiteral("1.4.0");
        environment.clock = &clock;
        environment.resolver = testResolver;
        environment.removeStorageOnUninstall = [this] { return removeStorage; };
        environment.log = [this](const QString &id, const QString &level, const QString &message) {
            QMutexLocker lock(&m_mutex);
            m_log << id + QLatin1Char('|') + level + QLatin1Char('|') + message;
        };
        reopen();
    }

    /// New manager on the same directory (like an application restart). Call it after
    /// changing `environment`.
    void reopen() {
        manager.reset();
        manager = std::make_unique<EasePass::Plugin::PluginManager>(environment);
    }

    QStringList log() const {
        QMutexLocker lock(&m_mutex);
        return m_log;
    }

    /// prepareInstall + install with every computed permission approved.
    EasePass::Plugin::PluginResult install(const QByteArray &packageBytes) {
        EasePass::Plugin::InstallCandidate candidate;
        const EasePass::Plugin::PluginResult prepared = manager->prepareInstall(packageBytes, &candidate);
        if (!prepared.ok())
            return prepared;
        return manager->install(candidate, candidate.inspection.permissions);
    }

    /// Installs a conformance package and returns its running worker (null on failure).
    std::shared_ptr<EasePass::Plugin::PluginWorker> installAndLoad(const QString &name) {
        const EasePass::Plugin::PluginResult installed = install(conformancePackage(name));
        if (!installed.ok()) {
            qWarning("install failed: %s", qPrintable(installed.message));
            return nullptr;
        }
        EasePass::Plugin::PluginResult result;
        auto worker = manager->load(QStringLiteral("org.easepass.conformance.") + name, &result);
        if (!worker)
            qWarning("load failed: %s", qPrintable(result.message));
        return worker;
    }

private:
    mutable QMutex m_mutex;
    QStringList m_log;
};

} // namespace PluginTest
