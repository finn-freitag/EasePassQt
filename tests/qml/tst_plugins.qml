import QtQuick
import QtQuick.Controls
import QtTest
import EasePass

// Plugin UI (steps.txt T28): install dialogs, plugin manager page, Settings section.
// "PluginUi" drives the QML with a fake view model (exact data in, exact rows out);
// "PluginUiReal" runs the same QML on the real PluginsViewModel with the conformance
// plugins (skipped when the app is built without the plugin host).
Item {
    id: top
    width: 1100; height: 1000

    // Fake PluginsViewModel: records the calls of the UI.
    component FakePlugins: QtObject {
        property var installed: []
        property var store: []
        property bool showIncompatible: false
        property int catalogStatus: 2
        property bool busy: false
        property int installStep: 0
        property var pendingInstall: ({})
        property int importerCount: 0
        property int providerCount: 0
        property var importSources: []
        property var importCandidates: []
        property var providers: []
        property var configFields: []
        property string configPluginId: ""
        property var calls: []
        signal filePickRequested(int requestId, string pluginName, string title, var nameFilters)
        signal message(int severity, string text)
        signal importReady()
        signal configReady()
        signal configSaved(bool ok)
        function record(call) { const c = calls.slice(); c.push(call); calls = c }
        function refreshStore() { record("refreshStore") }
        function installFromFile(file) { record("installFromFile " + file) }
        function installFromStore(id, feed) { record("installFromStore " + id + " [" + feed + "]") }
        function confirmUnverified() { record("confirmUnverified"); installStep = 2 }
        function approveInstall() { record("approveInstall"); installStep = 0 }
        function cancelInstall() { record("cancelInstall"); installStep = 0 }
        function requestUninstall(id) { record("requestUninstall " + id) }
        function setEnabled(id, on) { record("setEnabled " + id + " " + on); return true }
        function setAllowPrivateNetwork(id, on) { record("setAllowPrivateNetwork " + id + " " + on); return true }
        function openWebsite(id) { record("openWebsite " + id); return true }
        function resolveFilePick(id, file) { record("resolveFilePick " + id + " [" + file + "]") }
        function loadImportSources() { record("loadImportSources") }
        function importFrom(pluginId, sourceId) { record("importFrom " + pluginId + " " + sourceId) }
        function importSelected(indexes, overrideExisting) { record("importSelected " + indexes + " " + overrideExisting); return true }
        function cancelImport() { record("cancelImport") }
        function loadProviders() { record("loadProviders") }
        function openProviderConfig(id) { record("openProviderConfig " + id) }
        function saveProviderConfig(values) { record("saveProviderConfig " + JSON.stringify(values)) }
        function closeProviderConfig() { record("closeProviderConfig") }
    }

    Component { id: fakeC; FakePlugins {} }
    Component {
        id: dialogsC
        PluginDialogs { fileDialogsEnabled: false }
    }
    Component { id: pageC; PluginsPage { anchors.fill: parent } }
    Component { id: importC; PluginImportDialog {} }
    Component { id: configC; PluginConfigDialog {} }
    Component {
        id: settingsC
        SettingsPage {
            anchors.fill: parent
            viewModel: testContext.settingsViewModel
        }
    }
    Component { id: spyC; SignalSpy {} }

    // --- shared helpers -------------------------------------------------------------------
    function rows(test, parent, repeaterName) {
        const rep = test.findChild(parent, repeaterName)
        const texts = []
        for (let i = 0; i < rep.count; ++i)
            texts.push(rep.itemAt(i).text !== undefined ? rep.itemAt(i).text : rep.itemAt(i).message)
        return texts
    }
    function bullets(list) { return list.map((t) => "• " + t) }

    TestCase {
        id: tc
        name: "PluginUi"
        when: windowShown

        function init() { failOnWarning(/.?/) }

        function make(component, props) {
            const o = createTemporaryObject(component, top, props || {})
            verify(o)
            return o
        }
        function fake(props) { return make(fakeC, props) }
        function last(plugins) { return plugins.calls.length > 0 ? plugins.calls[plugins.calls.length - 1] : "" }
        function clickVisible(page, it, scrollName) {
            const flick = findChild(page, scrollName || "pluginsScroll").contentItem
            const p = it.mapToItem(flick.contentItem, 0, 0)
            flick.contentY = Math.max(0, Math.min(p.y - 20, Math.max(0, flick.contentHeight - flick.height)))
            wait(0)
            mouseClick(it)
        }

        // --- install dialogs ------------------------------------------------------------
        function test_permissionDialogShowsExactlyTheComputedRows() {
            const plugins = fake()
            const d = make(dialogsC, { plugins: plugins })
            const dialog = findChild(d, "pluginPermissionDialog")
            verify(!dialog.visible)

            plugins.pendingInstall = {
                id: "org.example.p", name: "Harmless notes", version: "2.0.0", author: "Somebody",
                types: ["Adds passwords to your database"],
                permissions: ["Connects to: api.example.org:443 (any protocol)",
                              "Keeps its own private settings and files"],
                warnings: ["This plugin adds passwords to your database and can send data over the network."],
                verification: 1, verificationText: "Not officially verified",
                isUpdate: false, installedVersion: "", osWarning: "", feed: ""
            }
            plugins.installStep = 2
            tryCompare(dialog, "opened", true)
            compare(dialog.title, "Install plugin?")
            compare(findChild(dialog, "permissionName").text, "Harmless notes 2.0.0")
            compare(findChild(dialog, "permissionAuthor").text, "By Somebody")
            compare(findChild(dialog, "permissionVerification").text, "Not officially verified")
            // One row per computed value, in order, and nothing else.
            compare(top.rows(tc, dialog, "typeRepeater"), top.bullets(plugins.pendingInstall.types))
            compare(top.rows(tc, dialog, "permissionRepeater"), top.bullets(plugins.pendingInstall.permissions))
            compare(top.rows(tc, dialog, "warningRepeater"), plugins.pendingInstall.warnings)
            verify(!findChild(dialog, "noPermissions").visible)
            verify(!findChild(dialog, "osWarning").visible)
            verify(!findChild(dialog, "permissionUpdateNote").visible)

            mouseClick(findChild(dialog, "primaryButton"))
            compare(last(plugins), "approveInstall")
            tryCompare(dialog, "visible", false)
        }

        function test_permissionDialogWithoutPermissionsAndAsUpdate() {
            const plugins = fake()
            const d = make(dialogsC, { plugins: plugins })
            const dialog = findChild(d, "pluginPermissionDialog")
            plugins.pendingInstall = {
                id: "org.example.p", name: "Lists", version: "1.1.0", author: "",
                types: ["Provides additional plugin lists"], permissions: [], warnings: [],
                verification: 0, verificationText: "Verified", isUpdate: true, installedVersion: "1.0.0",
                osWarning: "The plugin list names this plugin for Windows only. It may not work here.", feed: ""
            }
            plugins.installStep = 2
            tryCompare(dialog, "opened", true)
            compare(dialog.title, "Update plugin?")
            compare(dialog.primaryButtonText, "Update")
            compare(findChild(dialog, "permissionRepeater").count, 0)
            verify(findChild(dialog, "noPermissions").visible)
            verify(!findChild(dialog, "permissionAuthor").visible)
            verify(findChild(dialog, "permissionUpdateNote").visible)
            compare(findChild(dialog, "permissionUpdateNote").text, "Replaces the installed version 1.0.0.")
            verify(findChild(dialog, "osWarning").visible)
            compare(findChild(dialog, "osWarning").message, plugins.pendingInstall.osWarning)

            // Cancel (button or Escape) drops the install.
            mouseClick(findChild(dialog, "closeButton"))
            compare(last(plugins), "cancelInstall")
            tryCompare(dialog, "visible", false)
        }

        function test_unverifiedDialogComesFirst() {
            const plugins = fake()
            const d = make(dialogsC, { plugins: plugins })
            const warning = findChild(d, "pluginUnverifiedDialog")
            const permission = findChild(d, "pluginPermissionDialog")
            plugins.pendingInstall = { name: "Thing", version: "1.0.0", types: [], permissions: ["x"], warnings: [],
                                       verification: 1, verificationText: "Not officially verified", feed: "" }
            plugins.installStep = 1
            tryCompare(warning, "opened", true)
            verify(!permission.visible)
            compare(warning.title, "This plugin is not officially verified")
            compare(findChild(warning, "unverifiedName").text, "Thing 1.0.0")
            verify(findChild(warning, "unverifiedText").text.indexOf("not in the official plugin list") >= 0)
            verify(!findChild(warning, "unverifiedFeed").visible)

            // Continue -> the permission dialog follows; the warning closes.
            mouseClick(findChild(warning, "primaryButton"))
            compare(last(plugins), "confirmUnverified")
            tryCompare(permission, "opened", true)
            tryCompare(warning, "visible", false)
            compare(top.rows(tc, permission, "permissionRepeater"), ["• x"])
            mouseClick(findChild(permission, "closeButton"))
            tryCompare(permission, "visible", false)
        }

        function test_offlineHasItsOwnMessage() {
            const plugins = fake()
            const d = make(dialogsC, { plugins: plugins })
            const warning = findChild(d, "pluginUnverifiedDialog")
            plugins.pendingInstall = { name: "Thing", version: "1.0.0", types: [], permissions: [], warnings: [],
                                       verification: 2, verificationText: "Could not be verified (offline)",
                                       feed: "Community" }
            plugins.installStep = 1
            tryCompare(warning, "opened", true)
            compare(warning.title, "This plugin could not be verified")
            verify(findChild(warning, "unverifiedText").text.indexOf("could not be reached") >= 0)
            verify(findChild(warning, "unverifiedFeed").visible)
            verify(findChild(warning, "unverifiedFeed").text.indexOf("\"Community\"") >= 0)

            mouseClick(findChild(warning, "closeButton"))
            compare(last(plugins), "cancelInstall")
            tryCompare(warning, "visible", false)
            compare(plugins.calls.indexOf("confirmUnverified"), -1)
        }

        function test_filePickRequestsAreAnsweredOneByOne() {
            const plugins = fake()
            const d = make(dialogsC, { plugins: plugins })
            plugins.filePickRequested(7, "Importer", "Choose a CSV file", ["Files for the plugin (*.csv)"])
            plugins.filePickRequested(8, "Other", "", ["All files (*)"])
            compare(d.currentPick.id, 7)
            compare(d.currentPick.pluginName, "Importer")
            compare(d.pickQueue.length, 1)

            d.finishPick("file:///tmp/a.csv")
            compare(last(plugins), "resolveFilePick 7 [file:///tmp/a.csv]")
            compare(d.currentPick.id, 8)
            d.finishPick("")                       // cancelled
            compare(last(plugins), "resolveFilePick 8 []")
            compare(d.currentPick, null)
            compare(d.pickQueue.length, 0)
        }

        // --- plugin manager page ----------------------------------------------------------
        readonly property var installedSample: [{
            id: "org.example.one", name: "One", version: "1.2.3", author: "Ada", description: "First plugin",
            homepage: "https://example.org", icon: "", types: ["Adds passwords to your database"],
            permissions: ["Asks you to choose a file (every time)"], warnings: [],
            verification: 0, verificationText: "Verified", enabled: true, usesNetwork: false, allowPrivateNetwork: false
        }, {
            id: "org.example.two", name: "Two", version: "0.1.0", author: "", description: "",
            homepage: "", icon: "", types: ["Stores your encrypted database elsewhere (it only receives encrypted data)"],
            permissions: ["Connects to: example.org:443 (any protocol)"],
            warnings: ["This plugin sends your encrypted database over the network."],
            verification: 1, verificationText: "Not officially verified", enabled: false, usesNetwork: true,
            allowPrivateNetwork: false
        }]

        function test_pageListsInstalledPlugins() {
            const plugins = fake({ installed: installedSample })
            const page = make(pageC, { plugins: plugins })
            waitForRendering(page)
            compare(plugins.calls, ["refreshStore"])          // the list is loaded when the page opens
            verify(!findChild(page, "noPluginsLabel").visible)
            const rep = findChild(page, "installedRepeater")
            compare(rep.count, 2)

            const one = rep.itemAt(0)
            compare(findChild(one, "pluginName").text, "One  1.2.3")
            compare(findChild(one, "pluginAuthor").text, "By Ada")
            compare(findChild(one, "pluginVerification").text, "Verified")
            verify(findChild(one, "enabledSwitch").checked)
            verify(findChild(one, "websiteButton").visible)
            verify(!findChild(one, "privateNetworkSwitch").visible)
            verify(!findChild(one, "pluginIcon").visible)

            const two = rep.itemAt(1)
            verify(!findChild(two, "pluginAuthor").visible)
            verify(!findChild(two, "websiteButton").visible)
            verify(!findChild(two, "enabledSwitch").checked)
            verify(findChild(two, "privateNetworkSwitch").visible)

            clickVisible(page, findChild(one, "websiteButton"))
            compare(last(plugins), "openWebsite org.example.one")
            clickVisible(page, findChild(one, "enabledSwitch"))
            compare(last(plugins), "setEnabled org.example.one false")
            clickVisible(page, findChild(two, "privateNetworkSwitch"))
            compare(last(plugins), "setAllowPrivateNetwork org.example.two true")
            // Uninstall goes through the confirmation of the view model.
            clickVisible(page, findChild(two, "uninstallButton"))
            compare(last(plugins), "requestUninstall org.example.two")
        }

        function test_pageEmptyAndInstallFromFile() {
            const plugins = fake()
            const page = make(pageC, { plugins: plugins })
            waitForRendering(page)
            verify(findChild(page, "noPluginsLabel").visible)
            compare(findChild(page, "installedRepeater").count, 0)
            const spy = createTemporaryObject(spyC, top, { target: page, signalName: "installFromFileRequested" })
            mouseClick(findChild(page, "installFromFileButton"))
            compare(spy.count, 1)
            plugins.busy = true
            verify(!findChild(page, "installFromFileButton").enabled)
            verify(findChild(page, "pluginsBusy").running)

            const back = createTemporaryObject(spyC, top, { target: page, signalName: "backRequested" })
            mouseClick(findChild(page, "backButton"))
            compare(back.count, 1)
        }

        function test_storeListAndOsToggle() {
            const plugins = fake({ store: [
                { id: "org.example.a", name: "A", description: "Good", author: "Ada", version: "1.0.0", feed: "",
                  compatible: true, note: "", installed: false, installedVersion: "", updateAvailable: false, sensitive: false },
                { id: "org.example.b", name: "B", description: "", author: "", version: "2.0.0", feed: "Community",
                  compatible: true, note: "", installed: true, installedVersion: "1.0.0", updateAvailable: true, sensitive: true },
                { id: "org.example.c", name: "C", description: "", author: "", version: "1.0.0", feed: "",
                  compatible: true, note: "", installed: true, installedVersion: "1.0.0", updateAvailable: false, sensitive: false },
                { id: "org.example.w", name: "W", description: "", author: "", version: "1.0.0", feed: "",
                  compatible: false, note: "Windows only", installed: false, installedVersion: "", updateAvailable: false, sensitive: false }
            ] })
            const page = make(pageC, { plugins: plugins })
            waitForRendering(page)
            const rep = findChild(page, "storeRepeater")
            compare(rep.count, 4)
            compare(findChild(page, "catalogStatusLabel").text, "")

            const a = findChild(rep.itemAt(0), "storeInstallButton")
            compare(a.text, "Install")
            verify(a.enabled)
            clickVisible(page, a)
            compare(last(plugins), "installFromStore org.example.a []")

            const b = findChild(rep.itemAt(1), "storeInstallButton")
            compare(b.text, "Update")
            clickVisible(page, b)
            compare(last(plugins), "installFromStore org.example.b [Community]")

            const c = findChild(rep.itemAt(2), "storeInstallButton")
            compare(c.text, "Installed")
            verify(!c.enabled)

            // Incompatible: shown with the reason, cannot be installed.
            const w = rep.itemAt(3)
            verify(!findChild(w, "storeInstallButton").enabled)
            verify(findChild(w, "storeNote").visible)
            compare(findChild(w, "storeNote").text, "Windows only")
            verify(!findChild(rep.itemAt(0), "storeNote").visible)

            // The toggle writes the view model's property (which filters the list).
            const toggle = findChild(page, "showIncompatibleSwitch")
            verify(!toggle.checked)
            clickVisible(page, toggle)
            compare(plugins.showIncompatible, true)
            clickVisible(page, toggle)
            compare(plugins.showIncompatible, false)

            clickVisible(page, findChild(page, "refreshStoreButton"))
            compare(last(plugins), "refreshStore")
        }

        function test_storeStatusTexts() {
            const plugins = fake({ catalogStatus: 1 })
            const page = make(pageC, { plugins: plugins })
            const label = findChild(page, "catalogStatusLabel")
            compare(label.text, "Loading the plugin list...")
            verify(!findChild(page, "refreshStoreButton").enabled)
            plugins.catalogStatus = 3
            verify(label.text.indexOf("cannot be verified") >= 0)
            plugins.catalogStatus = 4
            compare(label.text, "The plugin list could not be loaded.")
            plugins.catalogStatus = 2
            compare(label.text, "No plugins available.")
            verify(findChild(page, "refreshStoreButton").enabled)
        }

        // --- settings section -------------------------------------------------------------
        function test_settingsSectionOnlyWithPlugins() {
            // Built without the plugin host: no plugin section at all.
            const without = make(settingsC)
            waitForRendering(without)
            verify(!findChild(without, "pluginsCard").visible)
            verify(!findChild(without, "pluginImportCard").visible)
            verify(!findChild(without, "pluginDatabaseCard").visible)
            verify(!findChild(without, "removePluginDataSwitch").visible)
            without.destroy()

            const plugins = fake()
            const page = make(settingsC, { plugins: plugins })
            waitForRendering(page)
            verify(findChild(page, "pluginsCard").visible)
            // Nothing installed: the actions explain what is missing and stay disabled.
            verify(!findChild(page, "pluginImportButton").enabled)
            verify(!findChild(page, "pluginSettingsButton").enabled)
            compare(findChild(page, "pluginImportCard").description, "Needs a plugin that can import passwords")

            plugins.importerCount = 1
            plugins.providerCount = 1
            verify(!findChild(page, "pluginImportButton").enabled)        // no database open
            compare(findChild(page, "pluginImportCard").description, "Log in to import passwords")
            verify(findChild(page, "pluginSettingsButton").enabled)        // works without a database
            page.databaseOpen = true
            verify(findChild(page, "pluginImportButton").enabled)

            const manage = createTemporaryObject(spyC, top, { target: page, signalName: "managePluginsRequested" })
            const imp = createTemporaryObject(spyC, top, { target: page, signalName: "pluginImportRequested" })
            const cfg = createTemporaryObject(spyC, top, { target: page, signalName: "pluginSettingsRequested" })
            clickVisible(page, findChild(page, "managePluginsButton"), "settingsScroll")
            clickVisible(page, findChild(page, "pluginImportButton"), "settingsScroll")
            clickVisible(page, findChild(page, "pluginSettingsButton"), "settingsScroll")
            compare(manage.count, 1)
            compare(imp.count, 1)
            compare(cfg.count, 1)

            const sw = findChild(page, "removePluginDataSwitch")
            compare(sw.checked, testContext.settings.removePluginSettingsOnUninstall)
            const before = sw.checked
            clickVisible(page, sw, "settingsScroll")
            compare(testContext.settings.removePluginSettingsOnUninstall, !before)
            clickVisible(page, sw, "settingsScroll")
            compare(testContext.settings.removePluginSettingsOnUninstall, before)
        }

        // --- import and provider dialogs with the fake --------------------------------------
        function test_importDialogListsSources() {
            const plugins = fake()
            const d = make(importC, { plugins: plugins })
            d.open()
            compare(plugins.calls, ["loadImportSources"])
            const sources = findChild(d, "pluginImportSourceDialog")
            tryCompare(sources, "opened", true)
            verify(findChild(sources, "noImportSources").visible)

            plugins.importSources = [
                { pluginId: "org.example.i", pluginName: "Importer", sourceId: "csv", name: "CSV file", description: "From a file" },
                { pluginId: "org.example.i", pluginName: "Importer", sourceId: "fixed", name: "Fixed", description: "" }
            ]
            const rep = findChild(sources, "importSourceRepeater")
            compare(rep.count, 2)
            compare(rep.itemAt(0).header, "CSV file")
            compare(rep.itemAt(0).description, "Importer - From a file")
            compare(rep.itemAt(1).description, "Importer")
            verify(!findChild(sources, "noImportSources").visible)
            mouseClick(findChild(rep.itemAt(1), "importSourceButton"))
            compare(last(plugins), "importFrom org.example.i fixed")

            // The plugin answered: selection dialog with the candidates.
            plugins.importCandidates = ["Mail", "Shop", "Bank"]
            plugins.importReady()
            const select = findChild(d, "pluginImportSelectDialog")
            tryCompare(select, "opened", true)
            tryCompare(sources, "visible", false)
            compare(findChild(select, "selectionCount").text, "3 / 3")
            tryVerify(() => select.itemAt(1) !== null)
            mouseClick(select.itemAt(1))
            mouseClick(findChild(select, "primaryButton"))
            compare(last(plugins), "importSelected 0,2 false")
            tryCompare(select, "visible", false)
        }

        function test_configDialogDrawsFields() {
            const plugins = fake()
            const d = make(configC, { plugins: plugins })
            d.open()
            compare(plugins.calls, ["loadProviders"])
            const providers = findChild(d, "pluginProviderDialog")
            tryCompare(providers, "opened", true)
            verify(findChild(providers, "noProviders").visible)
            plugins.providers = [{ pluginId: "org.example.db", name: "My server" }]
            const rep = findChild(providers, "providerRepeater")
            compare(rep.count, 1)
            compare(rep.itemAt(0).header, "My server")
            mouseClick(findChild(rep.itemAt(0), "providerSettingsButton"))
            compare(last(plugins), "openProviderConfig org.example.db")

            plugins.configPluginId = "org.example.db"
            plugins.configFields = [
                { key: "host", title: "Server", description: "Name of the server", kind: "string", value: "old",
                  options: [], required: true, maxLength: 50 },
                { key: "mode", title: "Mode", description: "", kind: "choice", value: "ftps",
                  options: ["sftp", "ftps"], required: false, maxLength: 4096 },
                { key: "passive", title: "Passive mode", description: "", kind: "boolean", value: false,
                  options: [], required: false, maxLength: 4096 },
                { key: "secret", title: "Password", description: "", kind: "password", value: "",
                  options: [], required: false, maxLength: 4096 }
            ]
            plugins.configReady()
            const config = findChild(d, "pluginConfigDialog")
            tryCompare(config, "opened", true)
            tryCompare(providers, "visible", false)
            const fields = findChild(config, "configRepeater")
            compare(fields.count, 4)

            const host = findChild(fields.itemAt(0), "configText")
            compare(host.text, "old")
            const choice = findChild(fields.itemAt(1), "configChoice")
            compare(choice.currentText, "ftps")
            const passive = findChild(fields.itemAt(2), "configSwitch")
            verify(!passive.checked)
            verify(findChild(fields.itemAt(3), "configPassword") !== null)

            host.forceActiveFocus()
            host.selectAll()
            keyClick(Qt.Key_Delete)
            for (const ch of "new")
                keyClick(ch)
            mouseClick(passive)
            mouseClick(findChild(config, "primaryButton"))
            // Only what the user changed is sent; the dialog stays open until the plugin agreed.
            compare(JSON.parse(last(plugins).substring("saveProviderConfig ".length)), { host: "new", passive: true })
            verify(config.visible)
            plugins.configSaved(false)
            verify(config.visible)
            plugins.configSaved(true)
            tryCompare(config, "visible", false)
            compare(last(plugins), "closeProviderConfig")
        }
    }

    // --- the same QML on the real view model and real plugins ----------------------------------
    TestCase {
        id: real
        name: "PluginUiReal"
        when: windowShown

        readonly property var plugins: testContext.plugins

        function init() {
            if (!plugins)
                skip("built without the plugin host")
            testContext.resetPlugins()
            failOnWarning(/.?/)
        }
        function cleanup() {
            if (plugins)
                testContext.resetPlugins()
        }
        function make(component, props) {
            const o = createTemporaryObject(component, top, props)
            verify(o)
            return o
        }
        /// Installs a conformance package through the two dialogs.
        function install(dialogs, name) {
            plugins.installFromFile(testContext.pluginFixture(name))
            // The test network has no plugin list: "could not verify (offline)".
            const warning = findChild(dialogs, "pluginUnverifiedDialog")
            tryCompare(warning, "opened", true, 10000)
            compare(warning.title, "This plugin could not be verified")
            mouseClick(findChild(warning, "primaryButton"))
            const permission = findChild(dialogs, "pluginPermissionDialog")
            tryCompare(permission, "opened", true)
            return permission
        }

        function test_installShowsWhatTheComponentReallyNeeds() {
            const dialogs = make(dialogsC, { plugins: plugins })
            const permission = install(dialogs, "network")
            compare(findChild(permission, "permissionName").text, "Conformance: network 1.0.0")
            compare(findChild(permission, "permissionVerification").text, "Could not be verified (offline)")
            compare(top.rows(real, permission, "typeRepeater"), ["• Adds passwords to your database"])
            compare(top.rows(real, permission, "permissionRepeater"),
                    ["• Connects to: allowed.conformance.test:*, *.wild.conformance.test:*, "
                     + "port.conformance.test:8080, 192.0.2.10:* (any protocol)"])
            compare(top.rows(real, permission, "warningRepeater"),
                    ["This plugin adds passwords to your database and can send data over the network."])
            compare(plugins.installed.length, 0)

            mouseClick(findChild(permission, "primaryButton"))
            tryCompare(permission, "visible", false)
            compare(plugins.installed.length, 1)

            // The manager page shows it with the same computed lines.
            const page = make(pageC, { plugins: plugins })
            waitForRendering(page)
            const card = findChild(page, "installedRepeater").itemAt(0)
            compare(findChild(card, "pluginName").text, "Conformance: network  1.0.0")
            compare(findChild(card, "pluginAuthor").text, "By Ease Pass plugin spec")
            compare(findChild(card, "pluginVerification").text, "Could not be verified (offline)")
            verify(findChild(card, "privateNetworkSwitch").visible)
            tryCompare(findChild(page, "catalogStatusLabel"), "text", "The plugin list could not be loaded.")
        }

        function test_cancelInstallsNothing() {
            const dialogs = make(dialogsC, { plugins: plugins })
            const permission = install(dialogs, "importer")
            compare(top.rows(real, permission, "permissionRepeater"), ["• Asks you to choose a file (every time)"])
            mouseClick(findChild(permission, "closeButton"))
            tryCompare(permission, "visible", false)
            compare(plugins.installed.length, 0)
            compare(plugins.installStep, 0)
        }

        function test_importThroughPlugin() {
            const dialogs = make(dialogsC, { plugins: plugins })
            mouseClick(findChild(install(dialogs, "importer"), "primaryButton"))
            tryCompare(plugins, "importerCount", 1)
            verify(testContext.openEmpty())

            const d = make(importC, { plugins: plugins })
            d.open()
            const sources = findChild(d, "pluginImportSourceDialog")
            tryCompare(sources, "opened", true)
            const rep = findChild(sources, "importSourceRepeater")
            tryCompare(rep, "count", 2, 20000)
            compare(rep.itemAt(0).header, "Fixed items")
            tryCompare(plugins, "busy", false)
            mouseClick(findChild(rep.itemAt(0), "importSourceButton"))

            const select = findChild(d, "pluginImportSelectDialog")
            tryCompare(select, "opened", true, 20000)
            compare(findChild(select, "selectionCount").text, "2 / 2")
            tryVerify(() => select.itemAt(0) !== null)
            mouseClick(select.itemAt(0))                       // skip "Example"
            mouseClick(findChild(select, "primaryButton"))
            tryCompare(select, "visible", false)
            compare(testContext.itemCount, 1)
            compare(testContext.nameOf(0), "Ünïcödé ✓")
        }

        function test_providerSettingsForm() {
            const dialogs = make(dialogsC, { plugins: plugins })
            mouseClick(findChild(install(dialogs, "database-source"), "primaryButton"))
            tryCompare(plugins, "providerCount", 1)

            const d = make(configC, { plugins: plugins })
            d.open()
            const providers = findChild(d, "pluginProviderDialog")
            const rep = findChild(providers, "providerRepeater")
            tryCompare(rep, "count", 1, 20000)
            compare(rep.itemAt(0).header, "Conformance: database source")
            tryCompare(plugins, "busy", false)
            mouseClick(findChild(rep.itemAt(0), "providerSettingsButton"))

            const config = findChild(d, "pluginConfigDialog")
            tryCompare(config, "opened", true, 20000)
            const fields = findChild(config, "configRepeater")
            compare(fields.count, 2)
            const name = findChild(fields.itemAt(0), "configText")
            compare(name.text, "Conformance database")
            name.forceActiveFocus()
            name.selectAll()
            keyClick(Qt.Key_Delete)
            for (const ch of "Mine")
                keyClick(ch)
            mouseClick(findChild(config, "primaryButton"))
            tryCompare(config, "visible", false, 20000)

            // Stored by the plugin: the form shows the new value next time.
            plugins.openProviderConfig("org.easepass.conformance.database-source")
            tryCompare(config, "opened", true, 20000)
            tryCompare(findChild(config, "configRepeater"), "count", 2)
            compare(findChild(findChild(config, "configRepeater").itemAt(0), "configText").text, "Mine")
            mouseClick(findChild(config, "closeButton"))
            tryCompare(config, "visible", false)
        }
    }
}
