#include "PluginUiTests.h"

#include "../plugin/PluginTestSupport.h"
#include "AppContext.h"
#include "AppSettings.h"
#include "AppTestSupport.h"
#include "DialogService.h"
#include "PluginAdapters.h"
#include "PasswordGeneratorViewModel.h"
#include "DatabaseManagementViewModel.h"
#include "DatabaseController.h"
#include "ISettingsStore.h"
#include "PluginCatalog.h"
#include "PluginsViewModel.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QBuffer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <atomic>
#include <thread>

// Plugin UI logic (steps.txt T28): install flows with all verification states, the store
// list with operating-system filter, the data of the permission dialog, uninstall, import
// through importer plugins and the settings form of database-source plugins. Real
// PluginManager and CatalogClient on a temporary directory; the network is a fake.

using namespace EasePass::App;
using namespace EasePass::Core;
using namespace EasePass::Plugin;
using namespace PluginTest;
using Step = PluginsViewModel::InstallStep;

namespace {

const QString CatalogUrl = QStringLiteral("https://catalog.example.test/PluginList.json");
const QString ImporterId = QStringLiteral("org.easepass.conformance.importer");
const QString NetworkId = QStringLiteral("org.easepass.conformance.network");
const QString DatabaseId = QStringLiteral("org.easepass.conformance.database-source");
const QString FeedId = QStringLiteral("org.easepass.conformance.store-feed");
const QString AboutId = QStringLiteral("org.easepass.conformance.about");

struct Fixture {
    QTemporaryDir directory;
    AppTest::FakeClock clock;
    std::shared_ptr<FakeHttp> http = std::make_shared<FakeHttp>();
    std::shared_ptr<InMemorySettingsStore> store = std::make_shared<InMemorySettingsStore>();
    AppSettings settings{store};
    DatabaseService service{AppTest::kFastKdf};
    DialogService dialogs;
    QString os = QStringLiteral("linux");
    std::unique_ptr<PluginsViewModel> vm;
    QStringList messages; ///< "<severity>|<text>"
    QStringList openedUrls;
    int fileCounter = 0;

    Fixture() { reopen(); }
    ~Fixture() {
        vm.reset();
        service.shutdown();
    }

    /// New view model on the same directory (like an application restart).
    void reopen() {
        vm.reset();
        PluginsViewModel::Config config;
        config.pluginsDirectory = directory.filePath(QStringLiteral("plugins"));
        config.catalogUrl = QUrl(CatalogUrl);
        config.network = http;
        config.settings = &settings;
        config.service = &service;
        config.dialogs = &dialogs;
        config.clock = &clock;
        config.appVersion = QStringLiteral("1.4.0");
        config.os = os;
        config.limits.callTimeoutMs = 10'000;
        config.resolver = testResolver;
        vm = std::make_unique<PluginsViewModel>(config);
        vm->setUrlOpener([this](const QUrl &url) {
            openedUrls << url.toString();
            return true;
        });
        QObject::connect(vm.get(), &PluginsViewModel::message, vm.get(), [this](int severity, const QString &text) {
            messages << QString::number(severity) + QLatin1Char('|') + text;
        });
    }

    void serveCatalog(const QList<QByteArray> &entries) { http->replies[CatalogUrl] = {200, catalogJson(entries), {}}; }
    void catalogOffline() { http->replies[CatalogUrl] = {0, {}, QStringLiteral("no network")}; }

    QUrl writeFile(const QByteArray &bytes, const QString &suffix = QStringLiteral(".epplugin")) {
        const QString path = directory.filePath(QStringLiteral("file%1%2").arg(++fileCounter).arg(suffix));
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size())
            qFatal("cannot write %s", qPrintable(path));
        return QUrl::fromLocalFile(path);
    }

    bool idle() const { return !vm->busy(); }

    /// installFromFile and wait until the dialog step is known (or the install failed).
    Step installFile(const QByteArray &package) {
        const qsizetype before = messages.size();
        vm->installFromFile(writeFile(package));
        waitUntil([&] { return idle() && (vm->installStep() != Step::None || messages.size() > before); });
        return vm->installStep();
    }
    Step installStore(const QString &id, const QString &feed = {}) {
        const qsizetype before = messages.size();
        vm->installFromStore(id, feed);
        waitUntil([&] { return idle() && (vm->installStep() != Step::None || messages.size() > before); });
        return vm->installStep();
    }
    /// Installs a package through the whole flow, whatever the verification.
    bool installed(const QByteArray &package) {
        Step step = installFile(package);
        if (step == Step::UnverifiedWarning) {
            vm->confirmUnverified();
            step = vm->installStep();
        }
        if (step != Step::Permissions)
            return false;
        vm->approveInstall();
        return vm->installStep() == Step::None;
    }
    bool refreshStore() {
        vm->refreshStore();
        return waitUntil([&] {
            return vm->catalogStatus() != PluginsViewModel::CatalogStatus::Loading &&
                   vm->catalogStatus() != PluginsViewModel::CatalogStatus::Unknown;
        });
    }

    QVariantMap pending() const { return vm->pendingInstall(); }
    QVariantMap installedItem(const QString &id) const { return find(vm->installed(), id); }
    QVariantMap storeItem(const QString &id) const { return find(vm->store(), id); }
    QStringList storeIds() const {
        QStringList ids;
        for (const QVariant &item : vm->store())
            ids << item.toMap().value(QStringLiteral("id")).toString();
        return ids;
    }
    bool hasPlugin(const QString &id) const { return vm->manager()->plugin(id).has_value(); }
    QString lastMessage() const { return messages.isEmpty() ? QString() : messages.constLast(); }

    static QVariantMap find(const QVariantList &list, const QString &id) {
        for (const QVariant &item : list) {
            if (item.toMap().value(QStringLiteral("id")).toString() == id)
                return item.toMap();
        }
        return {};
    }
};

QByteArray hashOf(const QByteArray &package) { return PluginPackage::hash(package); }

/// A package around a conformance component with a manifest written by the "author".
QByteArray customPackage(const QString &component, const QString &id, const QString &members,
                         const QByteArray &icon = {}) {
    const QByteArray manifest = QStringLiteral(R"({"id": "%1", "api_version": 1, %2})").arg(id, members).toUtf8();
    return makePackage(manifest, PluginTest::component(component), icon);
}

const QString PlainMembers = QStringLiteral(R"("name": "Custom", "version": "1.0.0", "description": "")");

} // namespace

// --- Verification states (4.3) -------------------------------------------------------------

void PluginsViewModelTest::testInstallFromFileVerified() {
    Fixture f;
    const QByteArray package = conformancePackage(QStringLiteral("importer"));
    f.serveCatalog({catalogEntryJson(ImporterId, hashOf(package))});

    QSignalSpy pendingSpy(f.vm.get(), &PluginsViewModel::pendingInstallChanged);
    // Known hash: no "not verified" window, straight to the permission dialog.
    QCOMPARE(f.installFile(package), Step::Permissions);
    QCOMPARE(pendingSpy.count(), 1);
    QCOMPARE(f.http->requested, QStringList{CatalogUrl}); // the catalog was asked for this install
    const QVariantMap pending = f.pending();
    QCOMPARE(pending.value("verification").toInt(), int(PluginsViewModel::Verified));
    QCOMPARE(pending.value("verificationText").toString(), QStringLiteral("Verified"));
    QCOMPARE(pending.value("id").toString(), ImporterId);
    QCOMPARE(pending.value("name").toString(), QStringLiteral("Conformance: importer"));
    QCOMPARE(pending.value("version").toString(), QStringLiteral("1.0.0"));
    QCOMPARE(pending.value("isUpdate").toBool(), false);
    QVERIFY(!f.hasPlugin(ImporterId)); // nothing is installed before the user agrees

    f.vm->approveInstall();
    QCOMPARE(f.vm->installStep(), Step::None);
    QVERIFY(f.pending().isEmpty());
    QVERIFY(f.hasPlugin(ImporterId));
    QCOMPARE(f.vm->manager()->plugin(ImporterId)->verification, PluginVerification::Verified);
    QCOMPARE(f.lastMessage(), QStringLiteral("1|\"Conformance: importer\" was installed."));
    QCOMPARE(f.vm->importerCount(), 1);
    QCOMPARE(f.installedItem(ImporterId).value("verificationText").toString(), QStringLiteral("Verified"));
}

void PluginsViewModelTest::testInstallFromFileUnverified() {
    Fixture f;
    const QByteArray package = conformancePackage(QStringLiteral("importer"));
    f.serveCatalog({catalogEntryJson(QStringLiteral("org.example.other"), QByteArray(64, 'a'))});

    // Catalog reachable, hash unknown: the extra window comes first.
    QCOMPARE(f.installFile(package), Step::UnverifiedWarning);
    QCOMPARE(f.pending().value("verification").toInt(), int(PluginsViewModel::Unverified));
    QCOMPARE(f.pending().value("verificationText").toString(), QStringLiteral("Not officially verified"));

    // approveInstall() cannot skip the warning.
    f.vm->approveInstall();
    QCOMPARE(f.vm->installStep(), Step::UnverifiedWarning);
    QVERIFY(!f.hasPlugin(ImporterId));

    // Cancel at the warning: nothing happens.
    f.vm->cancelInstall();
    QCOMPARE(f.vm->installStep(), Step::None);
    QVERIFY(f.pending().isEmpty());
    QVERIFY(!f.hasPlugin(ImporterId));

    // Second try: continue, then cancel at the permission dialog.
    QCOMPARE(f.installFile(package), Step::UnverifiedWarning);
    f.vm->confirmUnverified();
    QCOMPARE(f.vm->installStep(), Step::Permissions);
    f.vm->cancelInstall();
    QVERIFY(!f.hasPlugin(ImporterId));

    // Third try: all the way.
    QVERIFY(f.installed(package));
    QVERIFY(f.hasPlugin(ImporterId));
    QCOMPARE(f.vm->manager()->plugin(ImporterId)->verification, PluginVerification::Unverified);
    QCOMPARE(f.installedItem(ImporterId).value("verification").toInt(), int(PluginsViewModel::Unverified));
}

void PluginsViewModelTest::testInstallFromFileOffline() {
    Fixture f;
    const QByteArray package = conformancePackage(QStringLiteral("importer"));
    f.catalogOffline();

    // Catalog unreachable: a DIFFERENT state and text than "not in the list".
    QCOMPARE(f.installFile(package), Step::UnverifiedWarning);
    QCOMPARE(f.pending().value("verification").toInt(), int(PluginsViewModel::Offline));
    QCOMPARE(f.pending().value("verificationText").toString(), QStringLiteral("Could not be verified (offline)"));
    QVERIFY(PluginsViewModel::verificationText(PluginVerification::Offline) !=
            PluginsViewModel::verificationText(PluginVerification::Unverified));

    f.vm->confirmUnverified();
    f.vm->approveInstall();
    QVERIFY(f.hasPlugin(ImporterId));
    QCOMPARE(f.vm->manager()->plugin(ImporterId)->verification, PluginVerification::Offline);

    // A hash that WOULD be verified is still "offline" while only the cached list is known.
    Fixture g;
    g.serveCatalog({catalogEntryJson(ImporterId, hashOf(package))});
    QVERIFY(g.refreshStore());
    QCOMPARE(g.vm->catalogStatus(), PluginsViewModel::CatalogStatus::Live);
    g.catalogOffline();
    QCOMPARE(g.installFile(package), Step::UnverifiedWarning);
    QCOMPARE(g.pending().value("verification").toInt(), int(PluginsViewModel::Offline));
    QCOMPARE(g.vm->catalogStatus(), PluginsViewModel::CatalogStatus::Cached);
}

void PluginsViewModelTest::testInstallFromFileRevoked() {
    Fixture f;
    const QByteArray package = conformancePackage(QStringLiteral("importer"));
    f.serveCatalog({catalogEntryJson(ImporterId, hashOf(package), QStringLiteral(R"("revoked": true)"))});

    // Revoked: blocked, no dialog at all.
    QCOMPARE(f.installFile(package), Step::None);
    QVERIFY(f.pending().isEmpty());
    QVERIFY(!f.hasPlugin(ImporterId));
    QCOMPARE(f.lastMessage(), QStringLiteral("3|This plugin was withdrawn and cannot be used."));

    // Still blocked when the catalog is unreachable later (revocation comes from the cache).
    f.catalogOffline();
    QCOMPARE(f.installFile(package), Step::None);
    QVERIFY(!f.hasPlugin(ImporterId));
    QVERIFY(f.lastMessage().contains(QLatin1String("withdrawn")));
}

void PluginsViewModelTest::testInstallFromFileProblems() {
    Fixture f;
    f.serveCatalog({});

    // Not a package.
    QCOMPARE(f.installFile("this is not a zip file"), Step::None);
    QVERIFY2(f.lastMessage().startsWith(QLatin1String("3|This is not a valid plugin file.")), qPrintable(f.lastMessage()));

    // A component with a forbidden import.
    const QByteArray forbidden = makePackage(manifestJson(QStringLiteral("org.example.forbidden")),
                                             fixture(QStringLiteral("negative/forbidden-import.wasm")));
    QCOMPARE(f.installFile(forbidden), Step::None);
    QVERIFY2(f.lastMessage().startsWith(QLatin1String("3|The plugin was rejected")), qPrintable(f.lastMessage()));

    // Missing file.
    const qsizetype before = f.messages.size();
    f.vm->installFromFile(QUrl::fromLocalFile(f.directory.filePath(QStringLiteral("missing.epplugin"))));
    QCOMPARE(f.messages.size(), before + 1);
    QCOMPARE(f.lastMessage(), QStringLiteral("3|The file could not be read."));
    QVERIFY(f.vm->installed().isEmpty());

    // A second install cannot start while one is waiting for the user.
    QCOMPARE(f.installFile(conformancePackage(QStringLiteral("about"))), Step::UnverifiedWarning);
    const QVariantMap pending = f.pending();
    f.vm->installFromFile(f.writeFile(conformancePackage(QStringLiteral("importer"))));
    QVERIFY(waitUntil([&] { return f.idle(); }));
    QCOMPARE(f.pending(), pending);
}

// --- Store ----------------------------------------------------------------------------------

void PluginsViewModelTest::testInstallFromStore() {
    Fixture f;
    const QByteArray package = conformancePackage(QStringLiteral("importer"));
    f.serveCatalog({catalogEntryJson(ImporterId, hashOf(package))});
    f.http->replies[downloadUrl(ImporterId)] = {200, package, {}};

    QCOMPARE(f.vm->catalogStatus(), PluginsViewModel::CatalogStatus::Unknown);
    QVERIFY(f.vm->store().isEmpty());
    QVERIFY(f.refreshStore());
    QCOMPARE(f.vm->catalogStatus(), PluginsViewModel::CatalogStatus::Live);
    QVariantMap item = f.storeItem(ImporterId);
    QCOMPARE(item.value("name").toString(), QStringLiteral("Catalog ") + ImporterId);
    QCOMPARE(item.value("author").toString(), QStringLiteral("Catalog author"));
    QCOMPARE(item.value("installed").toBool(), false);
    QCOMPARE(item.value("compatible").toBool(), true);

    QCOMPARE(f.installStore(ImporterId), Step::Permissions); // hash matches the list: verified
    QCOMPARE(f.pending().value("verification").toInt(), int(PluginsViewModel::Verified));
    f.vm->approveInstall();
    QVERIFY(f.hasPlugin(ImporterId));
    item = f.storeItem(ImporterId);
    QCOMPARE(item.value("installed").toBool(), true);
    QCOMPARE(item.value("installedVersion").toString(), QStringLiteral("1.0.0"));
    QCOMPARE(item.value("updateAvailable").toBool(), false);

    // An id that is not in the list.
    QCOMPARE(f.installStore(QStringLiteral("org.example.unknown")), Step::None);
    QCOMPARE(f.lastMessage(), QStringLiteral("3|This plugin is not in the plugin list any more."));
}

void PluginsViewModelTest::testStoreHashMismatchAborts() {
    Fixture f;
    const QByteArray listed = conformancePackage(QStringLiteral("importer"));
    f.serveCatalog({catalogEntryJson(ImporterId, hashOf(listed))});
    QVERIFY(f.refreshStore());

    // The server delivers something else than the list promises (here: another valid plugin).
    f.http->replies[downloadUrl(ImporterId)] = {200, conformancePackage(QStringLiteral("network")), {}};
    QCOMPARE(f.installStore(ImporterId), Step::None);
    QVERIFY(f.pending().isEmpty());
    QVERIFY(f.vm->installed().isEmpty());
    QCOMPARE(f.lastMessage(),
             QStringLiteral("3|The downloaded file is not the one named in the plugin list. Nothing was installed."));

    // Download fails.
    f.http->replies[downloadUrl(ImporterId)] = {404, {}, {}};
    QCOMPARE(f.installStore(ImporterId), Step::None);
    QCOMPARE(f.lastMessage(), QStringLiteral("3|The plugin could not be downloaded."));
    QVERIFY(f.vm->installed().isEmpty());
}

void PluginsViewModelTest::testStoreFromCachedListIsNotVerified() {
    Fixture f;
    const QByteArray package = conformancePackage(QStringLiteral("importer"));
    f.serveCatalog({catalogEntryJson(ImporterId, hashOf(package))});
    f.http->replies[downloadUrl(ImporterId)] = {200, package, {}};
    QVERIFY(f.refreshStore());

    // Restart; the catalog cannot be reached, but the download server can.
    f.catalogOffline();
    f.reopen();
    QVERIFY(f.refreshStore());
    QCOMPARE(f.vm->catalogStatus(), PluginsViewModel::CatalogStatus::Cached);
    QCOMPARE(f.storeIds(), QStringList{ImporterId}); // the last known list is shown

    // Installing from it works (the hash still has to match), but it counts as "offline".
    QCOMPARE(f.installStore(ImporterId), Step::UnverifiedWarning);
    QCOMPARE(f.pending().value("verification").toInt(), int(PluginsViewModel::Offline));

    // Never fetched and no cache: unavailable.
    Fixture g;
    g.catalogOffline();
    QVERIFY(g.refreshStore());
    QCOMPARE(g.vm->catalogStatus(), PluginsViewModel::CatalogStatus::Unavailable);
    QVERIFY(g.vm->store().isEmpty());
}

void PluginsViewModelTest::testOsFilteringAndToggle() {
    Fixture f; // runs as "linux"
    const auto hash = [](char c) { return QByteArray(64, c); };
    f.serveCatalog({
        catalogEntryJson(QStringLiteral("org.example.everywhere"), hash('1')),
        catalogEntryJson(QStringLiteral("org.example.windows"), hash('2'), QStringLiteral(R"("os": ["windows"])")),
        catalogEntryJson(QStringLiteral("org.example.unix"), hash('3'), QStringLiteral(R"("os": ["linux", "macos"])")),
        catalogEntryJson(QStringLiteral("org.example.apple"), hash('4'), QStringLiteral(R"("os": ["windows", "macos"])")),
        catalogEntryJson(QStringLiteral("org.example.empty-os"), hash('5'), QStringLiteral(R"("os": [])")),
        catalogEntryJson(QStringLiteral("org.example.future-api"), hash('6'), QStringLiteral(R"("api_version": 2)")),
        catalogEntryJson(QStringLiteral("org.example.future-app"), hash('7'),
                         QStringLiteral(R"("min_app_version": "99.0.0")")),
        catalogEntryJson(QStringLiteral("org.example.revoked"), hash('8'), QStringLiteral(R"("revoked": true)")),
    });
    QVERIFY(f.refreshStore());

    // Default: incompatible plugins are hidden.
    QVERIFY(!f.vm->showIncompatible());
    QCOMPARE(f.storeIds(), QStringList({QStringLiteral("org.example.everywhere"), QStringLiteral("org.example.unix"),
                                        QStringLiteral("org.example.empty-os")}));
    for (const QVariant &item : f.vm->store()) {
        QCOMPARE(item.toMap().value("compatible").toBool(), true);
        QCOMPARE(item.toMap().value("note").toString(), QString());
    }

    // Toggle: they are listed, marked incompatible, with the reason. Revoked ones never show.
    QSignalSpy storeSpy(f.vm.get(), &PluginsViewModel::storeChanged);
    f.vm->setShowIncompatible(true);
    QCOMPARE(storeSpy.count(), 1);
    QCOMPARE(f.vm->store().size(), 7);
    QVERIFY(!f.storeIds().contains(QStringLiteral("org.example.revoked")));
    QCOMPARE(f.storeItem(QStringLiteral("org.example.windows")).value("compatible").toBool(), false);
    QCOMPARE(f.storeItem(QStringLiteral("org.example.windows")).value("note").toString(), QStringLiteral("Windows only"));
    QCOMPARE(f.storeItem(QStringLiteral("org.example.apple")).value("note").toString(),
             QStringLiteral("Windows, macOS only"));
    QCOMPARE(f.storeItem(QStringLiteral("org.example.future-api")).value("compatible").toBool(), false);
    QCOMPARE(f.storeItem(QStringLiteral("org.example.future-api")).value("note").toString(),
             QStringLiteral("Needs a newer version of Ease Pass"));
    QCOMPARE(f.storeItem(QStringLiteral("org.example.future-app")).value("note").toString(),
             QStringLiteral("Needs Ease Pass 99.0.0 or newer"));
    QCOMPARE(f.storeItem(QStringLiteral("org.example.unix")).value("compatible").toBool(), true);

    f.vm->setShowIncompatible(false);
    QCOMPARE(f.vm->store().size(), 3);

    // The same list on Windows.
    f.os = QStringLiteral("windows");
    f.reopen();
    QVERIFY(f.refreshStore());
    QCOMPARE(f.storeIds(), QStringList({QStringLiteral("org.example.everywhere"), QStringLiteral("org.example.windows"),
                                        QStringLiteral("org.example.apple"), QStringLiteral("org.example.empty-os")}));
    f.vm->setShowIncompatible(true);
    QCOMPARE(f.storeItem(QStringLiteral("org.example.unix")).value("note").toString(), QStringLiteral("Linux, macOS only"));
}

void PluginsViewModelTest::testOsWarningForLocalFile() {
    Fixture f;
    // The package itself does not name a system, but the catalog lists this file for Windows.
    const QByteArray package = conformancePackage(QStringLiteral("about"));
    f.serveCatalog({catalogEntryJson(AboutId, hashOf(package), QStringLiteral(R"("os": ["windows"])"))});
    QCOMPARE(f.installFile(package), Step::Permissions);
    QCOMPARE(f.pending().value("osWarning").toString(),
             QStringLiteral("The plugin list names this plugin for Windows only. It may not work here."));
    f.vm->cancelInstall();

    f.serveCatalog({catalogEntryJson(AboutId, hashOf(package), QStringLiteral(R"("os": ["linux"])"))});
    QCOMPARE(f.installFile(package), Step::Permissions);
    QCOMPARE(f.pending().value("osWarning").toString(), QString());
    f.vm->cancelInstall();

    // A package whose own manifest excludes this system is refused.
    const QByteArray windowsOnly =
        customPackage(QStringLiteral("about"), QStringLiteral("org.example.win"),
                      PlainMembers + QStringLiteral(R"(, "os": ["windows"])"));
    QCOMPARE(f.installFile(windowsOnly), Step::None);
    QVERIFY2(f.lastMessage().startsWith(QLatin1String("3|This plugin does not work with this version of Ease Pass.")),
             qPrintable(f.lastMessage()));
}

// --- Permission dialog data (4.2) ------------------------------------------------------------

void PluginsViewModelTest::testDialogShowsComputedPermissions() {
    Fixture f;
    f.serveCatalog({});

    // The manifest LIES: harmless name and description, no `permissions` member. The
    // component really is an importer that uses the network.
    const QString lie = QStringLiteral("Offline notes. Needs no permissions and never uses the network.");
    const QByteArray liar = customPackage(
        QStringLiteral("network"), QStringLiteral("org.example.liar"),
        QStringLiteral(R"("name": "Harmless notes", "version": "2.0.0", "author": "Somebody", "description": "%1", )"
                       R"("network_hosts": ["api.example.org:443", "*.example.net:*"])").arg(lie));
    QCOMPARE(f.installFile(liar), Step::UnverifiedWarning);
    f.vm->confirmUnverified();
    QVariantMap pending = f.pending();

    const QStringList hosts{QStringLiteral("api.example.org:443"), QStringLiteral("*.example.net:*")};
    QCOMPARE(pending.value("types").toStringList(), QStringList{QStringLiteral("Adds passwords to your database")});
    QCOMPARE(pending.value("permissions").toStringList(),
             QStringList{QStringLiteral("Connects to: api.example.org:443, *.example.net:* (any protocol)")});
    QCOMPARE(pending.value("warnings").toStringList(),
             QStringList{QStringLiteral(
                 "This plugin adds passwords to your database and can send data over the network.")});
    // Exactly what the host computed from the component:
    InstallCandidate candidate;
    QVERIFY(f.vm->manager()->prepareInstall(liar, &candidate).ok());
    QCOMPARE(candidate.inspection.permissions, QStringList{QStringLiteral("network")});
    QCOMPARE(candidate.inspection.types, QStringList{QStringLiteral("importer")});
    QCOMPARE(pending.value("permissions").toStringList().size(), candidate.inspection.permissions.size());
    // The plugin's own description is in none of the dialog's values.
    for (auto it = pending.constBegin(); it != pending.constEnd(); ++it)
        QVERIFY2(!it.value().toStringList().join(QLatin1Char(' ')).contains(QLatin1String("Needs no permissions")) &&
                     !it.value().toString().contains(QLatin1String("Needs no permissions")),
                 qPrintable(it.key()));
    QCOMPARE(pending.value("name").toString(), QStringLiteral("Harmless notes"));
    QCOMPARE(pending.value("author").toString(), QStringLiteral("Somebody"));
    f.vm->cancelInstall();

    // A manifest that DECLARES other permissions than the component uses is rejected.
    const QByteArray wrongDeclaration =
        customPackage(QStringLiteral("network"), QStringLiteral("org.example.wrong"),
                      PlainMembers + QStringLiteral(R"(, "permissions": ["storage"])"));
    QCOMPARE(f.installFile(wrongDeclaration), Step::None);
    QVERIFY2(f.lastMessage().startsWith(QLatin1String("3|The plugin was rejected")), qPrintable(f.lastMessage()));

    // "Any server" gets its own warning; no declared server is said plainly.
    const QByteArray anywhere = customPackage(QStringLiteral("network"), QStringLiteral("org.example.anywhere"),
                                              PlainMembers + QStringLiteral(R"(, "network_hosts": ["*:443"])"));
    QCOMPARE(f.installFile(anywhere), Step::UnverifiedWarning);
    QCOMPARE(f.pending().value("warnings").toStringList(),
             QStringList({QStringLiteral("This plugin adds passwords to your database and can send data over the network."),
                          QStringLiteral("This plugin can connect to ANY server.")}));
    f.vm->cancelInstall();
    const QByteArray nowhere = customPackage(QStringLiteral("network"), QStringLiteral("org.example.nowhere"), PlainMembers);
    QCOMPARE(f.installFile(nowhere), Step::UnverifiedWarning);
    QCOMPARE(f.pending().value("permissions").toStringList(),
             QStringList{QStringLiteral("Uses the network, but names no server it may connect to")});
    f.vm->cancelInstall();

    // The other conformance plugins: one line per computed type and permission.
    struct Expectation {
        const char *name;
        QStringList types;
        QStringList permissions;
    };
    const QList<Expectation> expectations{
        {"importer", {"Adds passwords to your database"}, {"Asks you to choose a file (every time)"}},
        {"database-source",
         {"Stores your encrypted database elsewhere (it only receives encrypted data)"},
         {"Keeps its own private settings and files"}},
        {"password-generator",
         {"Generates passwords: its output becomes your passwords"},
         {"Checks passwords against a breach service (sends a hash prefix to a third party)"}},
        {"store-feed", {"Provides additional plugin lists"}, {}},
        {"about", {"Shows information about itself", "Runs when Ease Pass starts"}, {}},
    };
    for (const Expectation &expected : expectations) {
        QCOMPARE(f.installFile(conformancePackage(QLatin1String(expected.name))), Step::UnverifiedWarning);
        pending = f.pending();
        QCOMPARE(pending.value("types").toStringList(), expected.types);
        QCOMPARE(pending.value("permissions").toStringList(), expected.permissions);
        QVERIFY(pending.value("warnings").toStringList().isEmpty());
        f.vm->cancelInstall();
    }
}

void PluginsViewModelTest::testRiskTexts() {
    using VM = PluginsViewModel;
    const QStringList network{QStringLiteral("network")};
    QVERIFY(VM::riskWarnings({QStringLiteral("database-source")}, {QStringLiteral("storage")}, {}).isEmpty());
    QCOMPARE(VM::riskWarnings({QStringLiteral("database-source")}, network, {QStringLiteral("example.org:443")}),
             QStringList{QStringLiteral("This plugin sends your encrypted database over the network.")});
    QCOMPARE(VM::riskWarnings({QStringLiteral("password-generator")}, network, {}),
             QStringList{QStringLiteral("This plugin creates your passwords and can send data over the network.")});
    QCOMPARE(VM::riskWarnings({QStringLiteral("about")}, network, {QStringLiteral("*:*")}),
             QStringList{QStringLiteral("This plugin can connect to ANY server.")});
    QVERIFY(VM::riskWarnings({QStringLiteral("about")}, network, {QStringLiteral("*.example.org:*")}).isEmpty());
    // Unknown ids (a newer spec) are shown as such, never silently dropped.
    QCOMPARE(VM::permissionText(QStringLiteral("camera"), {}), QStringLiteral("Unknown permission: camera"));
    QCOMPARE(VM::typeText(QStringLiteral("theme")), QStringLiteral("Unknown plugin type: theme"));
}

// --- Installed plugins ----------------------------------------------------------------------

void PluginsViewModelTest::testInstalledList() {
    Fixture f;
    f.serveCatalog({});
    QImage image(300, 200, QImage::Format_ARGB32);
    image.fill(Qt::red);
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    QVERIFY(image.save(&buffer, "PNG"));

    const QByteArray withIcon = customPackage(
        QStringLiteral("network"), QStringLiteral("org.example.icon"),
        QStringLiteral(R"("name": "With icon", "version": "3.1.4", "author": "Ada", "description": "Does things", )"
                       R"("homepage": "https://example.org/plugin", "network_hosts": ["example.org:443"])"),
        png);
    QVERIFY(f.installed(withIcon));
    const QByteArray brokenIcon = customPackage(QStringLiteral("about"), QStringLiteral("org.example.broken"),
                                                PlainMembers, "this is not a PNG");
    QVERIFY(f.installed(brokenIcon));

    QCOMPARE(f.vm->installed().size(), 2);
    const QVariantMap item = f.installedItem(QStringLiteral("org.example.icon"));
    QCOMPARE(item.value("name").toString(), QStringLiteral("With icon"));
    QCOMPARE(item.value("version").toString(), QStringLiteral("3.1.4"));
    QCOMPARE(item.value("author").toString(), QStringLiteral("Ada"));
    QCOMPARE(item.value("description").toString(), QStringLiteral("Does things"));
    QCOMPARE(item.value("types").toStringList(), QStringList{QStringLiteral("Adds passwords to your database")});
    QCOMPARE(item.value("permissions").toStringList(),
             QStringList{QStringLiteral("Connects to: example.org:443 (any protocol)")});
    QCOMPARE(item.value("warnings").toStringList().size(), 1);
    QCOMPARE(item.value("enabled").toBool(), true);
    QCOMPARE(item.value("usesNetwork").toBool(), true);
    QCOMPARE(item.value("allowPrivateNetwork").toBool(), false);

    // The icon is re-encoded and scaled down before the UI gets it.
    const QString icon = item.value("icon").toString();
    QVERIFY(icon.startsWith(QLatin1String("data:image/png;base64,")));
    const QImage shown = QImage::fromData(QByteArray::fromBase64(icon.mid(22).toLatin1()), "PNG");
    QCOMPARE(shown.size(), QSize(128, 85));
    QCOMPARE(f.installedItem(QStringLiteral("org.example.broken")).value("icon").toString(), QString());
    QCOMPARE(f.installedItem(QStringLiteral("org.example.broken")).value("usesNetwork").toBool(), false);

    // Website: only http(s), through the opener.
    QVERIFY(f.vm->openWebsite(QStringLiteral("org.example.icon")));
    QCOMPARE(f.openedUrls, QStringList{QStringLiteral("https://example.org/plugin")});
    QVERIFY(!f.vm->openWebsite(QStringLiteral("org.example.broken"))); // has none
    QVERIFY(!f.vm->openWebsite(QStringLiteral("org.example.unknown")));
    QCOMPARE(f.openedUrls.size(), 1);

    // Local network switch.
    QVERIFY(f.vm->setAllowPrivateNetwork(QStringLiteral("org.example.icon"), true));
    QCOMPARE(f.installedItem(QStringLiteral("org.example.icon")).value("allowPrivateNetwork").toBool(), true);

    // Enable switch.
    QSignalSpy installedSpy(f.vm.get(), &PluginsViewModel::installedChanged);
    QVERIFY(f.vm->setEnabled(QStringLiteral("org.example.icon"), false));
    QVERIFY(installedSpy.count() >= 1);
    QCOMPARE(f.installedItem(QStringLiteral("org.example.icon")).value("enabled").toBool(), false);
    QCOMPARE(f.vm->importerCount(), 0); // disabled plugins offer nothing
    QVERIFY(f.vm->setEnabled(QStringLiteral("org.example.icon"), true));
    QCOMPARE(f.vm->importerCount(), 1);
    QVERIFY(!f.vm->setEnabled(QStringLiteral("org.example.unknown"), true));
    QCOMPARE(f.lastMessage(), QStringLiteral("3|This plugin is not installed."));

    // Everything survives a restart.
    f.reopen();
    QCOMPARE(f.vm->installed().size(), 2);
    QCOMPARE(f.installedItem(QStringLiteral("org.example.icon")).value("allowPrivateNetwork").toBool(), true);
}

void PluginsViewModelTest::testUninstallFlow() {
    Fixture f;
    f.serveCatalog({});
    QVERIFY(f.installed(conformancePackage(QStringLiteral("database-source"))));
    const QString storage = f.vm->manager()->storageDirectory(DatabaseId);
    const QString package = f.vm->manager()->packagePath(DatabaseId);
    QVERIFY(QDir().mkpath(storage));
    QFile data(QDir(storage).filePath(QStringLiteral("settings.json")));
    QVERIFY(data.open(QIODevice::WriteOnly));
    data.write("{}");
    data.close();
    QVERIFY(QFile::exists(package));

    // The question names the plugin and says what happens to its data.
    QSignalSpy dialogSpy(&f.dialogs, &DialogService::dialogRequested);
    f.vm->requestUninstall(DatabaseId);
    QCOMPARE(dialogSpy.count(), 1);
    QCOMPARE(dialogSpy.at(0).at(1).toString(), QStringLiteral("Uninstall plugin"));
    QCOMPARE(dialogSpy.at(0).at(2).toString(),
             QStringLiteral("Do you really want to uninstall \"Conformance: database source\"?\n"
                            "Its settings and files will be deleted too."));
    QCOMPARE(dialogSpy.at(0).at(3).toString(), QStringLiteral("Uninstall"));

    // Cancel: nothing is removed.
    QVERIFY(f.dialogs.respond(dialogSpy.at(0).at(0).toInt(), DialogService::Result::None));
    QVERIFY(f.hasPlugin(DatabaseId));
    QVERIFY(QFile::exists(package));
    QVERIFY(QFile::exists(data.fileName()));

    // Confirm: plugin, package and (by default) its data are gone.
    QSignalSpy installedSpy(f.vm.get(), &PluginsViewModel::installedChanged);
    f.vm->requestUninstall(DatabaseId);
    QVERIFY(f.dialogs.respond(dialogSpy.at(1).at(0).toInt(), DialogService::Result::Primary));
    QVERIFY(!f.hasPlugin(DatabaseId));
    QVERIFY(f.vm->installed().isEmpty());
    QVERIFY(installedSpy.count() >= 1);
    QVERIFY(!QFile::exists(package));
    QVERIFY(!QDir(storage).exists());
    QCOMPARE(f.lastMessage(), QStringLiteral("1|\"Conformance: database source\" was uninstalled."));
    QCOMPARE(f.vm->providerCount(), 0);

    // With the setting off, the data stays - and the question says so.
    f.settings.setRemovePluginSettingsOnUninstall(false);
    QVERIFY(f.installed(conformancePackage(QStringLiteral("database-source"))));
    QVERIFY(QDir().mkpath(storage));
    QVERIFY(data.open(QIODevice::WriteOnly));
    data.close();
    f.vm->requestUninstall(DatabaseId);
    QVERIFY(dialogSpy.at(2).at(2).toString().endsWith(QLatin1String("Its settings and files will be kept.")));
    QVERIFY(f.dialogs.respond(dialogSpy.at(2).at(0).toInt(), DialogService::Result::Primary));
    QVERIFY(!f.hasPlugin(DatabaseId));
    QVERIFY(QFile::exists(data.fileName()));

    // Unknown ids ask nothing.
    f.vm->requestUninstall(QStringLiteral("org.example.unknown"));
    QCOMPARE(dialogSpy.count(), 3);
    QVERIFY(!f.vm->uninstall(QStringLiteral("org.example.unknown")));
}

void PluginsViewModelTest::testRevocationDisablesInstalledPlugin() {
    Fixture f;
    const QByteArray package = conformancePackage(QStringLiteral("importer"));
    f.serveCatalog({catalogEntryJson(ImporterId, hashOf(package))});
    QVERIFY(f.installed(package));
    QCOMPARE(f.vm->importerCount(), 1);

    // A newer catalog withdraws the package.
    f.serveCatalog({catalogEntryJson(ImporterId, hashOf(package), QStringLiteral(R"("revoked": true)"))});
    QVERIFY(f.refreshStore());
    QVERIFY(waitUntil([&] { return !f.vm->manager()->plugin(ImporterId)->enabled; }));
    QCOMPARE(f.lastMessage(), QStringLiteral("2|The plugin \"Conformance: importer\" was withdrawn and has been disabled."));
    QCOMPARE(f.vm->importerCount(), 0);
    QCOMPARE(f.installedItem(ImporterId).value("verification").toInt(), int(PluginsViewModel::Revoked));
    QCOMPARE(f.installedItem(ImporterId).value("verificationText").toString(), QStringLiteral("Withdrawn"));
    QVERIFY(f.storeItem(ImporterId).isEmpty()); // and it is not offered any more

    // It cannot be switched on again, also after a restart without network.
    QVERIFY(!f.vm->setEnabled(ImporterId, true));
    QCOMPARE(f.lastMessage(), QStringLiteral("3|This plugin was withdrawn and cannot be used."));
    f.catalogOffline();
    f.reopen();
    QVERIFY(!f.vm->setEnabled(ImporterId, true));
    QVERIFY(!f.vm->manager()->plugin(ImporterId)->enabled);

    // A plugin installed while it was fine and revoked while the app was closed is disabled
    // at the next start from the cached list.
    Fixture g;
    g.serveCatalog({catalogEntryJson(ImporterId, hashOf(package))});
    QVERIFY(g.installed(package));
    QFile cache(g.directory.filePath(QStringLiteral("plugins/PluginList.json")));
    QVERIFY(cache.open(QIODevice::WriteOnly));
    cache.write(catalogJson({catalogEntryJson(ImporterId, hashOf(package), QStringLiteral(R"("revoked": true)"))}));
    cache.close();
    g.http->requested.clear();
    g.reopen();
    QVERIFY(waitUntil([&] { return !g.vm->manager()->plugin(ImporterId)->enabled; }));
    QVERIFY(g.http->requested.isEmpty()); // without any network request at startup
}

void PluginsViewModelTest::testUpdateFromStore() {
    Fixture f;
    const QByteArray v1 = customPackage(QStringLiteral("about"), QStringLiteral("org.example.app"),
                                        QStringLiteral(R"("name": "App", "version": "1.0.0", "description": "")"));
    const QByteArray v2 = customPackage(QStringLiteral("importer"), QStringLiteral("org.example.app"),
                                        QStringLiteral(R"("name": "App", "version": "1.1.0", "description": "")"));
    const QString id = QStringLiteral("org.example.app");
    f.serveCatalog({catalogEntryJson(id, hashOf(v1))});
    QVERIFY(f.installed(v1));
    QVERIFY(f.refreshStore());
    QCOMPARE(f.storeItem(id).value("updateAvailable").toBool(), false);

    f.serveCatalog({catalogEntryJson(id, hashOf(v2), {}, QStringLiteral("1.1.0"))});
    f.http->replies[downloadUrl(id)] = {200, v2, {}};
    QVERIFY(f.refreshStore());
    QCOMPARE(f.storeItem(id).value("installed").toBool(), true);
    QCOMPARE(f.storeItem(id).value("updateAvailable").toBool(), true);

    // The update asks again and shows the NEW permissions (v2 needs the file dialog).
    QCOMPARE(f.installStore(id), Step::Permissions);
    QCOMPARE(f.pending().value("isUpdate").toBool(), true);
    QCOMPARE(f.pending().value("installedVersion").toString(), QStringLiteral("1.0.0"));
    QCOMPARE(f.pending().value("permissions").toStringList(),
             QStringList{QStringLiteral("Asks you to choose a file (every time)")});
    f.vm->approveInstall();
    QCOMPARE(f.lastMessage(), QStringLiteral("1|\"App\" was updated."));
    QCOMPARE(f.vm->manager()->plugin(id)->version, QStringLiteral("1.1.0"));
    QCOMPARE(f.storeItem(id).value("updateAvailable").toBool(), false);
}

void PluginsViewModelTest::testStoreFeedPluginAddsCatalog() {
    Fixture f;
    const QString feedUrl = QStringLiteral("https://example.com/easepass/conformance/PluginList.json");
    const QByteArray importer = conformancePackage(QStringLiteral("importer"));
    f.serveCatalog({});
    f.http->replies[feedUrl] = {200, catalogJson({catalogEntryJson(ImporterId, hashOf(importer))}), {}};
    f.http->replies[downloadUrl(ImporterId)] = {200, importer, {}};

    // Without the plugin nobody asks that server.
    QVERIFY(f.refreshStore());
    QVERIFY(!f.http->requested.contains(feedUrl));
    QVERIFY(f.vm->store().isEmpty());

    QVERIFY(f.installed(conformancePackage(QStringLiteral("store-feed"))));
    f.http->requested.clear();
    QVERIFY(f.refreshStore());
    QVERIFY(waitUntil([&] { return f.vm->store().size() == 1; }));
    QCOMPARE(f.http->requested, QStringList({CatalogUrl, feedUrl}));
    QCOMPARE(f.storeItem(ImporterId).value("feed").toString(), QStringLiteral("Conformance catalog"));

    // A package from an additional list is never "verified": the warning names the list.
    QCOMPARE(f.installStore(ImporterId, QStringLiteral("Conformance catalog")), Step::UnverifiedWarning);
    QCOMPARE(f.pending().value("verification").toInt(), int(PluginsViewModel::Unverified));
    QCOMPARE(f.pending().value("feed").toString(), QStringLiteral("Conformance catalog"));
    // Without naming the feed the entry is not found (it is not in the official list).
    f.vm->cancelInstall();
    QCOMPARE(f.installStore(ImporterId), Step::None);

    // A disabled feed plugin is not asked.
    QVERIFY(f.vm->setEnabled(FeedId, false));
    QVERIFY(f.refreshStore());
    QVERIFY(waitUntil([&] { return f.vm->store().isEmpty(); }));
}

// --- Import passwords -----------------------------------------------------------------------

void PluginsViewModelTest::testImportThroughPlugin() {
    Fixture f;
    f.serveCatalog({});
    QVERIFY(f.installed(conformancePackage(QStringLiteral("importer"))));
    QVERIFY(AppTest::openItems(f.service, {AppTest::makeItem(QStringLiteral("Existing"))}));

    QVERIFY(f.vm->importSources().isEmpty());
    f.vm->loadImportSources();
    QVERIFY(waitUntil([&] { return f.vm->importSources().size() == 2 && f.idle(); }));
    const QVariantMap first = f.vm->importSources().at(0).toMap();
    QCOMPARE(first.value("pluginId").toString(), ImporterId);
    QCOMPARE(first.value("pluginName").toString(), QStringLiteral("Conformance: importer"));
    QCOMPARE(first.value("sourceId").toString(), QStringLiteral("fixed"));
    QCOMPARE(first.value("name").toString(), QStringLiteral("Fixed items"));
    QCOMPARE(f.vm->importSources().at(1).toMap().value("sourceId").toString(), QStringLiteral("csv-file"));

    // The plugin's items are only candidates until the user selected some.
    QSignalSpy readySpy(f.vm.get(), &PluginsViewModel::importReady);
    f.vm->importFrom(ImporterId, QStringLiteral("fixed"));
    QVERIFY(waitUntil([&] { return readySpy.count() == 1; }));
    QCOMPARE(f.vm->importCandidates(), QStringList({QStringLiteral("Example"), QStringLiteral("Ünïcödé ✓")}));
    QCOMPARE(f.service.items().size(), 1);

    QVERIFY(!f.vm->importSelected({}, false));
    QCOMPARE(f.lastMessage(), QStringLiteral("2|No passwords selected"));
    QVERIFY(f.vm->importSelected({1}, false)); // only the second one, appended
    QCOMPARE(f.service.items().size(), 2);
    QCOMPARE(f.service.items().at(1).displayName, QStringLiteral("Ünïcödé ✓"));
    QCOMPARE(f.service.items().at(1).password, QStringLiteral("pässwörd-🔑"));
    QCOMPARE(f.service.items().at(1).secret, QStringLiteral("JBSWY3DPEHPK3PXP"));
    QVERIFY(f.vm->importCandidates().isEmpty());
    QCOMPARE(f.lastMessage(), QStringLiteral("1|1 password(s) imported"));

    // Override replaces the database's items.
    f.vm->importFrom(ImporterId, QStringLiteral("fixed"));
    QVERIFY(waitUntil([&] { return readySpy.count() == 2; }));
    QVERIFY(f.vm->importSelected({0, 1}, true));
    QCOMPARE(f.service.items().size(), 2);
    QCOMPARE(f.service.items().at(0).displayName, QStringLiteral("Example"));

    // Errors of the plugin are messages, not candidates.
    f.vm->importFrom(ImporterId, QStringLiteral("test-error"));
    QVERIFY(waitUntil([&] { return f.idle() && f.lastMessage().startsWith(QLatin1String("3|")); }));
    QCOMPARE(f.lastMessage(), QStringLiteral("3|The plugin's service is not available right now."));
    f.vm->importFrom(ImporterId, QStringLiteral("test-trap"));
    QVERIFY(waitUntil([&] { return f.idle() && f.lastMessage() == QLatin1String("3|The plugin crashed."); }));
    QVERIFY(f.vm->importCandidates().isEmpty());
    QCOMPARE(readySpy.count(), 2);

    // Locking the database drops candidates that were not imported yet (they are secrets).
    f.vm->importFrom(ImporterId, QStringLiteral("fixed"));
    QVERIFY(waitUntil([&] { return readySpy.count() == 3; }));
    QSignalSpy closedSpy(&f.service, &DatabaseService::closed);
    f.service.close();
    QVERIFY(waitUntil([&] { return closedSpy.count() == 1; }));
    QVERIFY(f.vm->importCandidates().isEmpty());
    QVERIFY(!f.vm->importSelected({0}, false));

    // Without an open database nothing is asked from the plugin.
    f.vm->importFrom(ImporterId, QStringLiteral("fixed"));
    QVERIFY(f.idle());
    QVERIFY(f.lastMessage().startsWith(QLatin1String("3|")));
    QCOMPARE(readySpy.count(), 3);
}

void PluginsViewModelTest::testImportWithFileDialog() {
    Fixture f;
    f.serveCatalog({});
    QVERIFY(f.installed(conformancePackage(QStringLiteral("importer"))));
    QVERIFY(AppTest::openItems(f.service, {}));

    QSignalSpy pickSpy(f.vm.get(), &PluginsViewModel::filePickRequested);
    QSignalSpy readySpy(f.vm.get(), &PluginsViewModel::importReady);

    // The plugin asks for a file: the UI gets the request with the PLUGIN's name (from the
    // installed record) so the dialog can say who is asking.
    f.vm->importFrom(ImporterId, QStringLiteral("csv-file"));
    QVERIFY(waitUntil([&] { return pickSpy.count() == 1; }));
    QCOMPARE(pickSpy.at(0).at(1).toString(), QStringLiteral("Conformance: importer"));
    QCOMPARE(pickSpy.at(0).at(2).toString(), QStringLiteral("Choose a CSV file"));
    QCOMPARE(pickSpy.at(0).at(3).toStringList(),
             QStringList({QStringLiteral("Files for the plugin (*.csv)"), QStringLiteral("All files (*)")}));
    QVERIFY(f.vm->busy()); // the plugin waits for the user

    const QUrl csv = f.writeFile("Mail;carol;secret-1\nShop;dave;secret-2\n", QStringLiteral(".csv"));
    f.vm->resolveFilePick(pickSpy.at(0).at(0).toInt(), csv);
    QVERIFY(waitUntil([&] { return readySpy.count() == 1; }));
    QCOMPARE(f.vm->importCandidates(), QStringList({QStringLiteral("Mail"), QStringLiteral("Shop")}));
    QVERIFY(f.vm->importSelected({0, 1}, false));
    QCOMPARE(f.service.items().size(), 2);
    QCOMPARE(f.service.items().at(1).username, QStringLiteral("dave"));

    // Cancelling the dialog is not an error.
    const qsizetype messagesBefore = f.messages.size();
    f.vm->importFrom(ImporterId, QStringLiteral("csv-file"));
    QVERIFY(waitUntil([&] { return pickSpy.count() == 2; }));
    f.vm->resolveFilePick(pickSpy.at(1).at(0).toInt(), QUrl());
    QVERIFY(waitUntil([&] { return f.idle(); }));
    QCOMPARE(f.messages.size(), messagesBefore);
    QVERIFY(f.vm->importCandidates().isEmpty());

    // An answer for a request that does not exist is ignored.
    f.vm->resolveFilePick(9999, csv);

    // A plugin still waiting for the dialog does not block shutting down.
    f.vm->importFrom(ImporterId, QStringLiteral("csv-file"));
    QVERIFY(waitUntil([&] { return pickSpy.count() == 3; }));
    QElapsedTimer timer;
    timer.start();
    f.vm.reset();
    QVERIFY(timer.elapsed() < 5000);
}

// --- Remote database settings ---------------------------------------------------------------

void PluginsViewModelTest::testProviderConfigForm() {
    Fixture f;
    f.serveCatalog({});
    QVERIFY(f.installed(conformancePackage(QStringLiteral("database-source"))));
    QCOMPARE(f.vm->providerCount(), 1);

    f.vm->loadProviders();
    QVERIFY(waitUntil([&] { return f.vm->providers().size() == 1 && f.idle(); }));
    QCOMPARE(f.vm->providers().at(0).toMap().value("pluginId").toString(), DatabaseId);
    QCOMPARE(f.vm->providers().at(0).toMap().value("name").toString(), QStringLiteral("Conformance storage"));

    QSignalSpy readySpy(f.vm.get(), &PluginsViewModel::configReady);
    QSignalSpy savedSpy(f.vm.get(), &PluginsViewModel::configSaved);
    f.vm->openProviderConfig(DatabaseId);
    QVERIFY(waitUntil([&] { return readySpy.count() == 1; }));
    QCOMPARE(f.vm->configPluginId(), DatabaseId);
    QCOMPARE(f.vm->configFields().size(), 2);
    const QVariantMap name = f.vm->configFields().at(0).toMap();
    QCOMPARE(name.value("key").toString(), QStringLiteral("database_name"));
    QCOMPARE(name.value("kind").toString(), QStringLiteral("string"));
    QCOMPARE(name.value("value").toString(), QStringLiteral("Conformance database"));
    QCOMPARE(name.value("required").toBool(), true);
    QCOMPARE(name.value("maxLength").toInt(), 80);
    const QVariantMap readOnly = f.vm->configFields().at(1).toMap();
    QCOMPARE(readOnly.value("key").toString(), QStringLiteral("read_only"));
    QCOMPARE(readOnly.value("kind").toString(), QStringLiteral("boolean"));
    QCOMPARE(readOnly.value("value").toBool(), false);

    // Only the changed field is passed; the other keeps its value.
    f.vm->saveProviderConfig({{QStringLiteral("database_name"), QStringLiteral("My remote")}});
    QVERIFY(waitUntil([&] { return savedSpy.count() == 1; }));
    QCOMPARE(savedSpy.at(0).at(0).toBool(), true);
    QCOMPARE(f.lastMessage(), QStringLiteral("1|Settings saved"));

    f.vm->closeProviderConfig();
    QVERIFY(f.vm->configFields().isEmpty());
    QVERIFY(f.vm->configPluginId().isEmpty());
    f.vm->openProviderConfig(DatabaseId);
    QVERIFY(waitUntil([&] { return readySpy.count() == 2; }));
    QCOMPARE(f.vm->configFields().at(0).toMap().value("value").toString(), QStringLiteral("My remote"));

    // An empty required field is refused by the host before the plugin is asked.
    f.vm->saveProviderConfig({{QStringLiteral("database_name"), QString()}});
    QCOMPARE(savedSpy.count(), 2);
    QCOMPARE(savedSpy.at(1).at(0).toBool(), false);
    QCOMPARE(f.lastMessage(), QStringLiteral("3|\"Database name\" must not be empty."));

    // What the plugin refuses comes back as a message; the form stays open.
    f.vm->saveProviderConfig({{QStringLiteral("database_name"), QString(200, QLatin1Char('x'))}});
    QVERIFY(waitUntil([&] { return savedSpy.count() == 3; }));
    QCOMPARE(savedSpy.at(2).at(0).toBool(), false);
    QVERIFY2(f.lastMessage().startsWith(QLatin1String("3|The plugin did not accept the input")),
             qPrintable(f.lastMessage()));
    QCOMPARE(f.vm->configPluginId(), DatabaseId);

    // Uninstalling the plugin closes its form.
    QVERIFY(f.vm->uninstall(DatabaseId));
    QVERIFY(f.vm->configFields().isEmpty());
    f.vm->openProviderConfig(DatabaseId);
    QCOMPARE(f.lastMessage(), QStringLiteral("3|This plugin is not installed."));
}

void PluginsViewModelTest::testConfigFieldsFromSchema() {
    using VM = PluginsViewModel;
    const QByteArray schema = R"({
        "type": "object",
        "required": ["host"],
        "properties": {
            "host": {"type": "string", "title": "Server", "description": "Name of the server", "maxLength": 255},
            "port": {"type": "integer", "minimum": 1, "maximum": 65535, "default": 22},
            "secret": {"type": "string", "format": "password"},
            "mode": {"type": "string", "enum": ["sftp", "ftps"], "default": "sftp"},
            "passive": {"type": "boolean", "default": true},
            "timeout": {"type": "number", "default": 2.5}
        }})";
    QString error;
    const QVariantList fields = VM::configFieldsFromSchema(schema, R"({"host": "example.org", "passive": false})", &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(fields.size(), 6);
    QHash<QString, QVariantMap> byKey;
    for (const QVariant &field : fields)
        byKey.insert(field.toMap().value("key").toString(), field.toMap());
    QCOMPARE(byKey["host"].value("kind").toString(), QStringLiteral("string"));
    QCOMPARE(byKey["host"].value("title").toString(), QStringLiteral("Server"));
    QCOMPARE(byKey["host"].value("description").toString(), QStringLiteral("Name of the server"));
    QCOMPARE(byKey["host"].value("value").toString(), QStringLiteral("example.org"));
    QCOMPARE(byKey["host"].value("required").toBool(), true);
    QCOMPARE(byKey["host"].value("maxLength").toInt(), 255);
    QCOMPARE(byKey["port"].value("kind").toString(), QStringLiteral("integer"));
    QCOMPARE(byKey["port"].value("title").toString(), QStringLiteral("port")); // no title: the key
    QCOMPARE(byKey["port"].value("value").toInt(), 22);                         // default
    QCOMPARE(byKey["port"].value("maximum").toDouble(), 65535.0);
    QCOMPARE(byKey["secret"].value("kind").toString(), QStringLiteral("password"));
    QCOMPARE(byKey["mode"].value("kind").toString(), QStringLiteral("choice"));
    QCOMPARE(byKey["mode"].value("options").toStringList(), QStringList({QStringLiteral("sftp"), QStringLiteral("ftps")}));
    QCOMPARE(byKey["mode"].value("value").toString(), QStringLiteral("sftp"));
    QCOMPARE(byKey["passive"].value("kind").toString(), QStringLiteral("boolean"));
    QCOMPARE(byKey["passive"].value("value").toBool(), false); // stored value wins over the default
    QCOMPARE(byKey["timeout"].value("kind").toString(), QStringLiteral("number"));

    // Form values (text fields give strings) become typed JSON.
    QByteArray json = VM::configJsonFromValues(
        fields, {{"port", QStringLiteral("2222")}, {"timeout", QStringLiteral("1,5")}, {"secret", QStringLiteral("pw")}},
        &error);
    QJsonObject object = QJsonDocument::fromJson(json).object();
    QCOMPARE(object.value("host").toString(), QStringLiteral("example.org"));
    QVERIFY(object.value("port").isDouble());
    QCOMPARE(object.value("port").toInt(), 2222);
    QCOMPARE(object.value("timeout").toDouble(), 1.5);
    QCOMPARE(object.value("secret").toString(), QStringLiteral("pw"));
    QCOMPARE(object.value("mode").toString(), QStringLiteral("sftp"));
    QCOMPARE(object.value("passive").toBool(), false);
    QCOMPARE(object.size(), 6);

    // Values the form cannot turn into what the schema asks for.
    for (const QString &port : {QStringLiteral("abc"), QStringLiteral("0"), QStringLiteral("70000"), QStringLiteral("1.5"),
                                QString()}) {
        error.clear();
        QVERIFY2(VM::configJsonFromValues(fields, {{"port", port}}, &error).isEmpty(), qPrintable(port));
        QCOMPARE(error, QStringLiteral("\"port\" needs a valid number."));
    }
    error.clear();
    QVERIFY(VM::configJsonFromValues(fields, {{"host", QString()}}, &error).isEmpty());
    QCOMPARE(error, QStringLiteral("\"Server\" must not be empty."));

    // Schemas the host cannot draw are refused instead of shown half.
    for (const QByteArray &bad :
         {QByteArray("not json"), QByteArray(R"({"type": "array"})"), QByteArray(R"([])"),
          QByteArray(R"({"type": "object", "properties": {"list": {"type": "array"}}})"),
          QByteArray(R"({"type": "object", "properties": {"nested": {"type": "object"}}})")}) {
        error.clear();
        QVERIFY(VM::configFieldsFromSchema(bad, "{}", &error).isEmpty());
        QVERIFY2(!error.isEmpty(), bad.constData());
    }
    QString many = QStringLiteral(R"({"type": "object", "properties": {)");
    for (int i = 0; i <= VM::MaxConfigFields; ++i)
        many += QStringLiteral(R"(%1"f%2": {"type": "string"})").arg(i ? QStringLiteral(", ") : QString()).arg(i);
    error.clear();
    QVERIFY(VM::configFieldsFromSchema((many + QStringLiteral("}}")).toUtf8(), "{}", &error).isEmpty());
    QCOMPARE(error, QStringLiteral("The plugin has too many settings."));

    // An object without properties is an empty form, not an error.
    error.clear();
    QVERIFY(VM::configFieldsFromSchema(R"({"type": "object"})", "{}", &error).isEmpty());
    QVERIFY(error.isEmpty());
}

// --- Application ----------------------------------------------------------------------------

void PluginsViewModelTest::testAppContextOwnsPlugins() {
    QTemporaryDir directory;
    AppTest::FakeClock clock;
    auto http = std::make_shared<FakeHttp>();
    const QByteArray package = conformancePackage(QStringLiteral("about"));
    http->replies[CatalogUrl] = {200, catalogJson({catalogEntryJson(AboutId, hashOf(package))}), {}};
    {
        AppContext::Deps deps;
        deps.settingsStore = std::make_shared<InMemorySettingsStore>();
        deps.clipboard = std::make_shared<AppTest::FakeClipboard>();
        deps.networkClient = http;
        deps.clock = &clock;
        deps.kdf = AppTest::kFastKdf;
        deps.pluginsDirectory = directory.filePath(QStringLiteral("plugins"));
        deps.pluginCatalogUrl = QUrl(CatalogUrl);
        AppContext context(deps);
        PluginsViewModel *plugins = context.plugins();
        QVERIFY(plugins);
        QVERIFY(http->requested.isEmpty()); // no network just because the application started

        const QString path = directory.filePath(QStringLiteral("about.epplugin"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(package);
        file.close();
        plugins->installFromFile(QUrl::fromLocalFile(path));
        QVERIFY(waitUntil([&] { return plugins->installStep() == Step::Permissions; }));
        plugins->approveInstall();
        QCOMPARE(plugins->installed().size(), 1);
        QVERIFY(QFile::exists(directory.filePath(QStringLiteral("plugins/packages/") + AboutId + QStringLiteral(".epplugin"))));

        // The "remove plugin data" setting of the application is the one the manager uses.
        context.settings()->setRemovePluginSettingsOnUninstall(false);
        const QString storage = plugins->manager()->storageDirectory(AboutId);
        QVERIFY(QDir().mkpath(storage));
        QVERIFY(plugins->uninstall(AboutId));
        QVERIFY(QDir(storage).exists());
    }
    // A second application start finds the same state (nothing installed, data kept).
    AppContext::Deps deps;
    deps.settingsStore = std::make_shared<InMemorySettingsStore>();
    deps.clipboard = std::make_shared<AppTest::FakeClipboard>();
    deps.networkClient = http;
    deps.clock = &clock;
    deps.kdf = AppTest::kFastKdf;
    deps.pluginsDirectory = directory.filePath(QStringLiteral("plugins"));
    AppContext context(deps);
    QVERIFY(context.plugins()->installed().isEmpty());
}

// --- Plugins at work in the application -------------------------------------------------------

namespace {
const QString GeneratorId = QStringLiteral("org.easepass.conformance.password-generator");

struct AppFixture {
    QTemporaryDir directory;
    AppTest::FakeClock clock;
    std::shared_ptr<FakeHttp> http = std::make_shared<FakeHttp>();
    std::shared_ptr<InMemorySettingsStore> store = std::make_shared<InMemorySettingsStore>();
    std::unique_ptr<AppContext> context;
    QStringList messages;

    AppFixture() {
        http->replies[CatalogUrl] = {200, catalogJson({}), {}};
        start();
    }
    /// A new application on the same settings and plugin directory.
    void start() {
        context.reset();
        AppContext::Deps deps;
        deps.settingsStore = store;
        deps.clipboard = std::make_shared<AppTest::FakeClipboard>();
        deps.networkClient = http;
        deps.clock = &clock;
        deps.kdf = AppTest::kFastKdf;
        deps.pluginsDirectory = directory.filePath(QStringLiteral("plugins"));
        deps.pluginCatalogUrl = QUrl(CatalogUrl);
        context = std::make_unique<AppContext>(deps);
        QObject::connect(plugins(), &PluginsViewModel::message, plugins(), [this](int severity, const QString &text) {
            messages << QString::number(severity) + QLatin1Char('|') + text;
        });
    }
    PluginsViewModel *plugins() const { return context->plugins(); }
    DatabaseController *controller() const { return context->databaseController(); }

    bool install(const QString &conformance) {
        const QString path = directory.filePath(conformance + QStringLiteral(".epplugin"));
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            return false;
        file.write(conformancePackage(conformance));
        file.close();
        plugins()->installFromFile(QUrl::fromLocalFile(path));
        if (!waitUntil([&] { return !plugins()->busy() && plugins()->installStep() != Step::None; }))
            return false;
        if (plugins()->installStep() == Step::UnverifiedWarning)
            plugins()->confirmUnverified();
        plugins()->approveInstall();
        return plugins()->installStep() == Step::None;
    }
    /// The plugin database's entry on the database page (empty if it is not listed).
    QVariantMap remoteEntry() const {
        for (const QVariant &entry : context->databaseManagement()->databases()) {
            if (entry.toMap().value(QStringLiteral("remote")).toBool())
                return entry.toMap();
        }
        return {};
    }
    QString remoteKey() const {
        for (const QString &entry : controller()->knownDatabases()) {
            if (RemoteDatabase::isKey(entry))
                return entry;
        }
        return {};
    }
};
} // namespace

// A database-source plugin's database on the login page: listed, opened, changed, saved
// through the plugin and found again after a restart.
void PluginsViewModelTest::testPluginDatabaseInApp() {
    AppFixture f;
    QVERIFY(!f.controller()->hasRemoteProviders());
    QVERIFY(f.install(QStringLiteral("database-source")));
    QVERIFY(waitUntil([&] { return !f.remoteKey().isEmpty() && !f.plugins()->remoteLoading(); }));
    const QString key = f.remoteKey();
    QCOMPARE(key, QStringLiteral("plugin://") + DatabaseId + QStringLiteral("/main"));
    QVERIFY(f.controller()->hasRemoteProviders());
    // The name is the plugin's database name together with the plugin's own name.
    QCOMPARE(f.controller()->databaseLabel(key), QStringLiteral("Conformance database (Conformance: database source)"));
    QCOMPARE(f.controller()->databaseDescription(key), QStringLiteral("Plugin storage (conformance)"));
    QVERIFY(f.controller()->isFirstRun()); // no local database: still the Register page

    // Nothing stored yet: opening fails with a message instead of an empty database.
    QSignalSpy failed(f.controller(), &DatabaseController::openFailed);
    QVERIFY(f.controller()->open(key, AppTest::kPassword));
    QVERIFY(waitUntil([&] { return failed.count() == 1; }));
    QVERIFY(!f.controller()->isOpen());

    // Put an encrypted database there, the way a plugin's server would already have one.
    {
        const std::shared_ptr<IDatabaseSource> source = f.plugins()->remoteDatabases().at(0).source;
        const QByteArray bytes = AppTest::makeDbBytes({AppTest::makeItem("First")});
        std::atomic_int saved{-1};
        std::thread writer([&] { saved = source->saveBytes(bytes) ? 1 : 0; });
        QVERIFY(waitUntil([&] { return saved.load() >= 0; }));
        writer.join();
        QCOMPARE(saved.load(), 1);
    }
    QVERIFY(f.controller()->open(key, AppTest::kPassword));
    QVERIFY(waitUntil([&] { return f.controller()->isOpen(); }));
    DatabaseService *service = f.context->databaseService();
    QCOMPARE(service->itemCount(), 1);
    QCOMPARE(f.controller()->databaseName(), QStringLiteral("Conformance database"));
    QCOMPARE(f.controller()->lastDatabase(), key);
    QCOMPARE(f.controller()->openDatabase(), key);
    QCOMPARE(f.remoteEntry().value("path").toString(), key);
    QCOMPARE(f.remoteEntry().value("loaded").toBool(), true);
    QCOMPARE(f.context->databaseManagement()->loadedPath(), key);

    // While its database is open the plugin stays: no uninstall, no switching off.
    QVERIFY(!f.plugins()->uninstall(DatabaseId));
    QCOMPARE(f.messages.constLast(),
             QStringLiteral("3|The open database is stored with this plugin. Close the database first."));
    QVERIFY(!f.plugins()->setEnabled(DatabaseId, false));
    QVERIFY(f.plugins()->manager()->plugin(DatabaseId)->enabled);
    // Asking the plugin again keeps the open database recognizable.
    f.plugins()->refreshRemoteDatabases();
    QVERIFY(waitUntil([&] { return !f.plugins()->remoteLoading(); }));
    QCOMPARE(f.controller()->openDatabase(), key);

    // A change is saved through the plugin ...
    QVERIFY(service->addItem(AppTest::makeItem("Second")));
    service->saveNow();
    QVERIFY(waitUntil([&] { return !service->isDirty() && !service->isSaving(); }));
    f.controller()->lock();
    QVERIFY(waitUntil([&] { return !f.controller()->isOpen(); }));

    // ... and is there after a restart; the plugin's database is offered again by itself.
    f.start();
    QVERIFY(waitUntil([&] { return f.remoteKey() == key; }));
    QCOMPARE(f.controller()->lastDatabase(), key);
    QVERIFY(f.controller()->open(key, AppTest::kPassword));
    QVERIFY(waitUntil([&] { return f.controller()->isOpen(); }));
    QCOMPARE(f.context->databaseService()->itemCount(), 2);
    f.controller()->lock();
    QVERIFY(waitUntil([&] { return !f.controller()->isOpen(); }));

    // A read-only database (the plugin's setting) is opened read-only.
    QSignalSpy saved(f.plugins(), &PluginsViewModel::configSaved);
    f.plugins()->openProviderConfig(DatabaseId);
    QVERIFY(waitUntil([&] { return f.plugins()->configPluginId() == DatabaseId; }));
    f.plugins()->saveProviderConfig({{QStringLiteral("database_name"), QStringLiteral("Renamed")},
                                     {QStringLiteral("read_only"), true}});
    QVERIFY(waitUntil([&] { return saved.count() == 1; }));
    QVERIFY(saved.at(0).at(0).toBool());
    // Saving the settings asks the plugin again: the new name shows up without a restart.
    QVERIFY(waitUntil([&] { return f.controller()->databaseLabel(key).startsWith(QLatin1String("Renamed (")); }));
    QVERIFY(f.controller()->open(key, AppTest::kPassword));
    QVERIFY(waitUntil([&] { return f.controller()->isOpen(); }));
    QVERIFY(f.controller()->readOnly());
    QVERIFY(!f.context->databaseService()->addItem(AppTest::makeItem("Third")));
    f.controller()->lock();
    QVERIFY(waitUntil([&] { return !f.controller()->isOpen(); }));

    // Closed: the plugin can go, and its database is no longer offered.
    QVERIFY(f.plugins()->setEnabled(DatabaseId, false));
    QVERIFY(waitUntil([&] { return f.remoteKey().isEmpty(); }));
    QVERIFY(!f.controller()->hasRemoteProviders());
    QVERIFY(!f.controller()->open(key, AppTest::kPassword));
    QVERIFY(f.plugins()->setEnabled(DatabaseId, true));
    QVERIFY(waitUntil([&] { return f.remoteKey() == key; }));
    QVERIFY(f.plugins()->uninstall(DatabaseId));
    QVERIFY(waitUntil([&] { return f.remoteKey().isEmpty(); }));
    QVERIFY(f.remoteEntry().isEmpty());
}

// A password-generator plugin in the generator dialog's view model.
void PluginsViewModelTest::testPluginGeneratorInApp() {
    AppFixture f;
    PasswordGeneratorViewModel *generator = f.context->passwordGenerator();
    f.context->settings()->setVerifyNotLeaked(false);
    f.context->settings()->setPasswordLength(21);
    f.context->settings()->setPasswordChars(QStringLiteral("xyz789"));
    QCOMPARE(generator->generators().size(), 1);

    QVERIFY(f.install(QStringLiteral("password-generator")));
    QCOMPARE(generator->generators().size(), 2);
    const QVariantMap entry = generator->generators().at(1).toMap();
    QCOMPARE(entry.value("id").toString(), GeneratorId);
    QCOMPARE(entry.value("name").toString(), f.plugins()->manager()->plugin(GeneratorId)->name);
    QCOMPARE(generator->generatorId(), QString()); // installed, not selected

    generator->setGeneratorId(GeneratorId);
    QSignalSpy generated(generator, &PasswordGeneratorViewModel::generated);
    generator->generate();
    QVERIFY(generator->busy()); // the plugin works on its own thread
    QVERIFY(waitUntil([&] { return generated.count() == 1; }));
    QVERIFY2(generator->errorMessage().isEmpty(), qPrintable(generator->errorMessage()));
    const QString password = generator->password();
    QCOMPARE(password.length(), 21);
    for (const QChar c : password)
        QVERIFY2(QStringLiteral("xyz789").contains(c), qPrintable(password));

    // Switched off: the built-in generator is used again; switched on: the plugin again.
    QVERIFY(f.plugins()->setEnabled(GeneratorId, false));
    QCOMPARE(generator->generators().size(), 1);
    QCOMPARE(generator->generatorId(), QString());
    generator->generate();
    QCOMPARE(generator->password().length(), 21);
    QVERIFY(f.plugins()->setEnabled(GeneratorId, true));
    QCOMPARE(generator->generatorId(), GeneratorId);

    // The choice survives a restart.
    f.start();
    generator = f.context->passwordGenerator();
    QCOMPARE(generator->generatorId(), GeneratorId);

    // A plugin that cannot run is reported; no password from somewhere else appears instead.
    QFile package(f.plugins()->manager()->packagePath(GeneratorId));
    QVERIFY(package.open(QIODevice::WriteOnly | QIODevice::Append));
    package.write("x");
    package.close();
    QSignalSpy failed(generator, &PasswordGeneratorViewModel::failed);
    generator->generate();
    QVERIFY(waitUntil([&] { return failed.count() == 1; }));
    QVERIFY2(!generator->errorMessage().isEmpty(), qPrintable(generator->errorMessage()));
    QVERIFY(generator->password().isEmpty());
    QVERIFY(!generator->busy());
}
