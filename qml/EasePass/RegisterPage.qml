import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// C#: Views/RegisterPage - first-run (OOBE) page: choose a master password for a new database
// at the default location, import an existing database, or create one with advanced options.
FocusScope {
    id: root
    required property var controller
    // Null-safe views of the controller (it can go away before this item during teardown).
    readonly property bool ctlBusy: controller ? controller.busy : false
    readonly property string ctlError: controller ? controller.errorMessage : ""
    readonly property bool ctlNeedsSecondFactor: controller ? controller.needsSecondFactor : false
    signal importFinished(string path)

    readonly property bool canRegister: passwordBox.text.length > 0 && repeatBox.text.length > 0
                                        && !ctlBusy

    function register() {
        if (!canRegister)
            return
        controller.createAndOpen(controller.defaultDatabasePath(), passwordBox.text, repeatBox.text)
    }

    Accessible.name: qsTr("Register")
    Component.onCompleted: passwordBox.forceActiveFocus()

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: Math.min(420, parent.width - 2 * Theme.spacingL)
            x: (parent.width - width) / 2
            spacing: Theme.spacingM

            Item { Layout.preferredHeight: Theme.spacingXL * 2 }

            Label {
                text: qsTr("Welcome to Ease Pass - Your open source password manager")
                font.pixelSize: Theme.fontSubtitle + 4
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                color: Theme.textPrimary
                Layout.fillWidth: true
            }
            Label {
                text: qsTr("Please choose a master password to begin using the password manager. Without this password, you will never be able to retrieve your saved passwords.")
                wrapMode: Text.WordWrap
                color: Theme.textSecondary
                Layout.fillWidth: true
                Layout.topMargin: Theme.spacingL
            }
            EditPasswordBox {
                id: passwordBox
                objectName: "registerPassword"
                placeholderText: qsTr("Enter password")
                focus: true
                Layout.fillWidth: true
                Keys.onReturnPressed: repeatBox.forceActiveFocus()
                Keys.onEnterPressed: repeatBox.forceActiveFocus()
            }
            EditPasswordBox {
                id: repeatBox
                objectName: "registerRepeat"
                placeholderText: qsTr("Repeat password")
                Layout.fillWidth: true
                Keys.onReturnPressed: root.register()
                Keys.onEnterPressed: root.register()
            }
            InfoBar {
                objectName: "errorBar"
                managed: true
                open: root.ctlError.length > 0
                severity: Theme.Severity.Error
                message: root.ctlError
                Layout.fillWidth: true
                onClosed: root.controller.clearError()
            }
            Button {
                objectName: "importLink"
                text: qsTr("I already have an existing database: Import here")
                flat: true
                Layout.alignment: Qt.AlignHCenter
                onClicked: importFlow.start()
            }
            Button {
                objectName: "advancedLink"
                text: qsTr("Advanced options")
                flat: true
                Layout.alignment: Qt.AlignHCenter
                onClicked: createDialog.open()
            }
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: Theme.spacingL
                Button {
                    objectName: "registerButton"
                    text: qsTr("Register")
                    highlighted: true
                    enabled: root.canRegister
                    onClicked: root.register()
                }
                BusyIndicator {
                    running: root.ctlBusy
                    visible: running
                    Layout.preferredWidth: Theme.controlHeight
                    Layout.preferredHeight: Theme.controlHeight
                }
            }
        }
    }

    CreateDatabaseDialog {
        id: createDialog
        objectName: "createDialog"
        parent: Overlay.overlay
        controller: root.controller
        openAfterCreate: true
    }
    ImportDatabaseFlow {
        id: importFlow
        objectName: "importFlow"
        controller: root.controller
        onImported: (path) => root.importFinished(path)
    }
}
