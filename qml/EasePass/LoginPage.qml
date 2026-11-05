import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// C#: Views/LoginPage - database picker, master password, optional second factor, async
// unlock with progress, error InfoBar, create/import database, daily tip.
// NO attempt limit (D6). `controller` is a DatabaseController (or a test fake); navigation
// after a successful unlock is done by AppController, the page only emits unlocked().
FocusScope {
    id: root
    required property var controller
    // Null-safe views of the controller (it can go away before this item during teardown).
    readonly property bool ctlBusy: controller ? controller.busy : false
    readonly property string ctlError: controller ? controller.errorMessage : ""
    readonly property bool ctlNeedsSecondFactor: controller ? controller.needsSecondFactor : false
    property string dailyTip: ""
    signal unlocked()
    signal copyRequested(string text)

    readonly property string selectedPath: databaseBox.currentIndex >= 0 && controller && controller.knownDatabases
                                           ? controller.knownDatabases[databaseBox.currentIndex] : ""
    readonly property bool canLogin: selectedPath.length > 0 && passwordBox.text.length > 0
                                     && !ctlBusy
                                     && (!ctlNeedsSecondFactor || secondFactorBox.text.length > 0)

    // Name shown in the database box; a database from the command line that is not one of the
    // known databases is opened temporarily (C#: " (Temp)").
    function databaseLabel(path) {
        if (!controller) return path
        const name = controller.displayNameForPath(path)
        return controller.isTemporaryPath !== undefined && controller.isTemporaryPath(path)
               ? name + qsTr(" (Temp)") : name
    }
    function login() {
        if (!canLogin)
            return false
        return controller.open(selectedPath, passwordBox.text, secondFactorBox.text)
    }
    function selectPath(path) {
        const i = controller.knownDatabases ? controller.knownDatabases.indexOf(path) : -1
        if (i >= 0)
            databaseBox.currentIndex = i
    }

    Accessible.name: qsTr("Login")
    Keys.onReturnPressed: login()
    Keys.onEnterPressed: login()

    Component.onCompleted: {
        selectPath(controller.lastDatabase)
        if (databaseBox.currentIndex < 0 && controller.knownDatabases && controller.knownDatabases.length > 0)
            databaseBox.currentIndex = 0
        passwordBox.forceActiveFocus()
    }

    Connections {
        target: root.controller
        function onOpenFailed(message) {
            passwordBox.selectAll()
            if (root.ctlNeedsSecondFactor)
                secondFactorBox.forceActiveFocus()
            else
                passwordBox.forceActiveFocus()
        }
        function onIsOpenChanged() {
            if (root.controller.isOpen) {
                passwordBox.text = ""
                secondFactorBox.text = ""
                root.unlocked()
            }
        }
        function onKnownDatabasesChanged() { root.selectPath(root.controller.lastDatabase) }
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: Math.min(400, parent.width - 2 * Theme.spacingL)
            x: (parent.width - width) / 2
            spacing: Theme.spacingM

            Item { Layout.preferredHeight: Theme.spacingXL * 2 }

            Label {
                text: qsTr("Welcome back")
                font.pixelSize: Theme.fontTitle
                color: Theme.textPrimary
                Layout.alignment: Qt.AlignHCenter
            }
            Label {
                text: qsTr("Unlock your database to continue.")
                color: Theme.textSecondary
                Layout.alignment: Qt.AlignHCenter
            }

            ComboBox {
                id: databaseBox
                objectName: "databaseBox"
                model: root.controller ? root.controller.knownDatabases : []
                displayText: currentIndex >= 0 && root.controller ? root.databaseLabel(currentText) : qsTr("No database")
                delegate: ItemDelegate {
                    required property string modelData
                    required property int index
                    width: ListView.view ? ListView.view.width : implicitWidth
                    text: root.databaseLabel(modelData)
                    highlighted: databaseBox.highlightedIndex === index
                    ToolTip.visible: hovered
                    ToolTip.text: modelData
                    ToolTip.delay: 800
                }
                Accessible.name: qsTr("Database")
                Layout.fillWidth: true
            }

            EditPasswordBox {
                id: passwordBox
                objectName: "passwordBox"
                placeholderText: qsTr("Master password")
                focus: true
                enabled: !root.ctlBusy
                Layout.fillWidth: true
                onCopyRequested: (t) => root.copyRequested(t)
            }

            EditPasswordBox {
                id: secondFactorBox
                objectName: "secondFactorBox"
                visible: root.ctlNeedsSecondFactor
                placeholderText: qsTr("Second factor")
                enabled: !root.ctlBusy
                Layout.fillWidth: true
                onVisibleChanged: if (visible) forceActiveFocus()
            }

            RowLayout {
                spacing: Theme.spacing
                Layout.fillWidth: true
                Button {
                    id: loginButton
                    objectName: "loginButton"
                    text: qsTr("Login")
                    highlighted: true
                    enabled: root.canLogin
                    Layout.fillWidth: true
                    onClicked: root.login()
                }
                BusyIndicator {
                    objectName: "busyIndicator"
                    running: root.ctlBusy
                    visible: running
                    Accessible.name: qsTr("Unlocking")
                    Layout.preferredWidth: Theme.controlHeight
                    Layout.preferredHeight: Theme.controlHeight
                }
            }

            InfoBar {
                objectName: "errorBar"
                managed: true
                open: root.ctlError.length > 0
                severity: Theme.Severity.Error
                title: qsTr("Error")
                message: root.ctlError
                Layout.fillWidth: true
                onClosed: root.controller.clearError()
            }

            RowLayout {
                spacing: Theme.spacing
                Layout.fillWidth: true
                Button {
                    objectName: "createButton"
                    text: qsTr("Create database")
                    icon.source: Theme.icon("add")
                    icon.color: Theme.textPrimary
                    enabled: !root.ctlBusy
                    Layout.fillWidth: true
                    onClicked: createDialog.open()
                }
                Button {
                    objectName: "importButton"
                    text: qsTr("Import database")
                    icon.source: Theme.icon("folder")
                    icon.color: Theme.textPrimary
                    enabled: !root.ctlBusy
                    Layout.fillWidth: true
                    onClicked: importFlow.start()
                }
            }

            DailyTipBox {
                objectName: "dailyTip"
                tip: root.dailyTip
                Layout.fillWidth: true
                Layout.topMargin: Theme.spacingL
            }
        }
    }

    CreateDatabaseDialog {
        id: createDialog
        objectName: "createDialog"
        parent: Overlay.overlay
        controller: root.controller
        onClosed: root.selectPath(root.controller.lastDatabase)
    }
    ImportDatabaseFlow {
        id: importFlow
        objectName: "importFlow"
        controller: root.controller
        onImported: (path) => root.selectPath(path)
    }
}
