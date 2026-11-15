import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// Dialogs of the plugin install flow (steps.txt T28, 4.2, 4.3) and the file dialog a plugin
// may ask for. `plugins` is a PluginsViewModel (or a test fake).
//   installStep 1: "not officially verified" / "could not verify (offline)"
//   installStep 2: what the plugin is and what it may do (C#: SensitiveDataDialog). Every
//                  line comes from plugins.pendingInstall, which the host computed from the
//                  plugin's code - never from what the plugin says about itself.
Item {
    id: root
    required property var plugins
    // Tests switch the real file dialogs off and answer through finishPick().
    property bool fileDialogsEnabled: true

    readonly property int stepUnverified: 1
    readonly property int stepPermissions: 2
    readonly property int verificationOffline: 2
    readonly property int step: plugins ? plugins.installStep : 0
    readonly property var pending: plugins && plugins.pendingInstall ? plugins.pendingInstall : ({})
    function list(value) { return value ? value : [] }

    function chooseInstallFile() { installFileLoader.active = fileDialogsEnabled }

    onStepChanged: {
        if (step === stepUnverified) unverifiedDialog.open(); else unverifiedDialog.close()
        if (step === stepPermissions) permissionDialog.open(); else permissionDialog.close()
    }

    // --- "Not officially verified" ----------------------------------------------------
    ContentDialog {
        id: unverifiedDialog
        objectName: "pluginUnverifiedDialog"
        parent: Overlay.overlay
        readonly property bool offline: root.pending.verification === root.verificationOffline
        title: offline ? qsTr("This plugin could not be verified") : qsTr("This plugin is not officially verified")
        primaryButtonText: qsTr("Continue anyway")
        closeButtonText: qsTr("Cancel")
        closeOnPrimary: false
        onPrimaryClicked: root.plugins.confirmUnverified()
        onCloseClicked: root.plugins.cancelInstall()

        Label {
            objectName: "unverifiedName"
            text: (root.pending.name || "") + " " + (root.pending.version || "")
            font.weight: Font.DemiBold
            color: Theme.textPrimary
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        Label {
            objectName: "unverifiedText"
            text: unverifiedDialog.offline
                  ? qsTr("The official plugin list could not be reached, so Ease Pass cannot tell whether this file is a known plugin. Check your internet connection and try again, or continue only if you trust where the file comes from.")
                  : qsTr("This file is not in the official plugin list. Nobody has checked it for Ease Pass. Continue only if you trust where the file comes from.")
            color: Theme.textPrimary
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        Label {
            objectName: "unverifiedFeed"
            visible: (root.pending.feed || "").length > 0
            text: qsTr("It comes from the additional plugin list \"%1\".").arg(root.pending.feed || "")
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }

    // --- Computed plugin type and permissions ---------------------------------------------
    ContentDialog {
        id: permissionDialog
        objectName: "pluginPermissionDialog"
        parent: Overlay.overlay
        title: root.pending.isUpdate ? qsTr("Update plugin?") : qsTr("Install plugin?")
        primaryButtonText: root.pending.isUpdate ? qsTr("Update") : qsTr("Install")
        closeButtonText: qsTr("Cancel")
        closeOnPrimary: false
        onPrimaryClicked: root.plugins.approveInstall()
        onCloseClicked: root.plugins.cancelInstall()

        Label {
            objectName: "permissionName"
            text: (root.pending.name || "") + " " + (root.pending.version || "")
            font.weight: Font.DemiBold
            color: Theme.textPrimary
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        Label {
            objectName: "permissionAuthor"
            visible: (root.pending.author || "").length > 0
            text: qsTr("By %1").arg(root.pending.author || "")
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        Label {
            objectName: "permissionVerification"
            text: root.pending.verificationText || ""
            color: root.pending.verification === 0 ? Theme.successForeground : Theme.warningForeground
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        Label {
            objectName: "permissionUpdateNote"
            visible: root.pending.isUpdate === true
            text: qsTr("Replaces the installed version %1.").arg(root.pending.installedVersion || "")
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Label {
            text: qsTr("What this plugin does")
            font.weight: Font.DemiBold
            color: Theme.textPrimary
            Layout.topMargin: Theme.spacing
        }
        Repeater {
            objectName: "typeRepeater"
            model: root.list(root.pending.types)
            Label {
                required property string modelData
                objectName: "typeRow"
                text: "• " + modelData
                color: Theme.textPrimary
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }

        Label {
            text: qsTr("What this plugin is allowed to do")
            font.weight: Font.DemiBold
            color: Theme.textPrimary
            Layout.topMargin: Theme.spacing
        }
        Repeater {
            objectName: "permissionRepeater"
            model: root.list(root.pending.permissions)
            Label {
                required property string modelData
                objectName: "permissionRow"
                text: "• " + modelData
                color: Theme.textPrimary
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }
        Label {
            objectName: "noPermissions"
            visible: root.list(root.pending.permissions).length === 0
            text: qsTr("Nothing beyond its own calculations: no files, no network, no storage.")
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Repeater {
            objectName: "warningRepeater"
            model: root.list(root.pending.warnings)
            InfoBar {
                required property string modelData
                objectName: "warningRow"
                severity: Theme.Severity.Warning
                message: modelData
                closable: false
                Layout.fillWidth: true
            }
        }
        InfoBar {
            objectName: "osWarning"
            visible: open
            open: (root.pending.osWarning || "").length > 0
            severity: Theme.Severity.Warning
            message: root.pending.osWarning || ""
            closable: false
            Layout.fillWidth: true
        }
    }

    // --- Install from a local file --------------------------------------------------------
    Loader {
        id: installFileLoader
        active: false
        sourceComponent: FileDialog {
            title: qsTr("Install plugin from file")
            nameFilters: [qsTr("Ease Pass plugin (*.epplugin)"), qsTr("All files (*)")]
            fileMode: FileDialog.OpenFile
            onAccepted: { root.plugins.installFromFile(selectedFile); installFileLoader.active = false }
            onRejected: installFileLoader.active = false
        }
        onLoaded: item.open()
    }

    // --- File dialog on behalf of a plugin (permission "pick-file") -----------------------
    // One request at a time; further requests wait in `pickQueue`.
    property var pickQueue: []
    property var currentPick: null
    function nextPick() {
        if (currentPick !== null || pickQueue.length === 0)
            return
        const queue = pickQueue.slice()
        currentPick = queue.shift()
        pickQueue = queue
        pickLoader.active = fileDialogsEnabled
    }
    function finishPick(file) {
        const pick = currentPick
        pickLoader.active = false
        currentPick = null
        if (pick !== null)
            root.plugins.resolveFilePick(pick.id, file)
        nextPick()
    }
    Connections {
        target: root.plugins
        function onFilePickRequested(requestId, pluginName, title, nameFilters) {
            const queue = root.pickQueue.slice()
            queue.push({ id: requestId, pluginName: pluginName, title: title, filters: nameFilters })
            root.pickQueue = queue
            root.nextPick()
        }
    }
    Loader {
        id: pickLoader
        active: false
        sourceComponent: FileDialog {
            // The title names the plugin: the user must see WHO asks for a file.
            title: qsTr("%1 asks for a file").arg(root.currentPick ? root.currentPick.pluginName : "")
                   + (root.currentPick && root.currentPick.title.length > 0 ? " - " + root.currentPick.title : "")
            nameFilters: root.currentPick ? root.currentPick.filters : []
            fileMode: FileDialog.OpenFile
            onAccepted: root.finishPick(selectedFile)
            onRejected: root.finishPick("")
        }
        onLoaded: item.open()
    }
}
