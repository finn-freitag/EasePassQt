import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// C#: ManageDatabaseHelper.ImportDatabase - pick an .epdb file (portal-aware FileDialog),
// ask for its password (and second factor if needed), verify it and add it to the known
// databases. `controller` is a DatabaseController (or a test fake).
Item {
    id: root
    required property var controller
    // Null-safe views of the controller (it can go away before this item during teardown).
    readonly property bool ctlBusy: controller ? controller.busy : false
    readonly property string ctlError: controller ? controller.errorMessage : ""
    readonly property bool ctlNeedsSecondFactor: controller ? controller.needsSecondFactor : false
    property string selectedFile: ""
    signal imported(string path)

    function start() { fileDialogLoader.active = true }
    function askPassword(file) {
        selectedFile = controller.toLocalPath(file)
        passwordBox.text = ""
        secondFactorBox.text = ""
        controller.clearError()
        passwordDialog.open()
    }

    Loader {
        id: fileDialogLoader
        active: false
        sourceComponent: FileDialog {
            title: qsTr("Import database")
            nameFilters: [qsTr("Ease Pass database (*.epdb)"), qsTr("All files (*)")]
            fileMode: FileDialog.OpenFile
            onAccepted: { root.askPassword(selectedFile.toString()); fileDialogLoader.active = false }
            onRejected: fileDialogLoader.active = false
        }
        onLoaded: item.open()
    }

    ContentDialog {
        id: passwordDialog
        objectName: "importPasswordDialog"
        parent: Overlay.overlay
        title: qsTr("Enter the password of %1").arg(root.controller ? root.controller.displayNameForPath(root.selectedFile) : "")
        primaryButtonText: qsTr("Import")
        closeButtonText: qsTr("Cancel")
        primaryButtonEnabled: passwordBox.text.length > 0 && !root.ctlBusy
        closeOnPrimary: false
        onAboutToShow: passwordBox.forceActiveFocus()
        onPrimaryClicked: root.controller.importDatabase(root.selectedFile, passwordBox.text, secondFactorBox.text)

        EditPasswordBox {
            id: passwordBox
            objectName: "importPassword"
            placeholderText: qsTr("Password")
            Layout.fillWidth: true
            Keys.onReturnPressed: passwordDialog.clickPrimary()
            Keys.onEnterPressed: passwordDialog.clickPrimary()
        }
        EditPasswordBox {
            id: secondFactorBox
            objectName: "importSecondFactor"
            visible: root.ctlNeedsSecondFactor
            placeholderText: qsTr("Second factor")
            Layout.fillWidth: true
        }
        InfoBar {
            visible: open
            open: root.ctlError.length > 0 && passwordDialog.opened
            severity: Theme.Severity.Error
            message: root.ctlError
            closable: false
            Layout.fillWidth: true
        }
    }

    Connections {
        target: root.controller
        function onImported(path) {
            passwordDialog.close()
            root.imported(path)
        }
    }
}
