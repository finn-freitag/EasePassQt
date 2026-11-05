import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// C#: Dialogs/CreateDatabaseDialog - name, folder and master password for a new database.
// openAfterCreate: open it right away (Register page / OOBE), else only add it to the list.
ContentDialog {
    id: root
    required property var controller
    // Null-safe views of the controller (it can go away before this item during teardown).
    readonly property bool ctlBusy: controller ? controller.busy : false
    readonly property string ctlError: controller ? controller.errorMessage : ""
    readonly property bool ctlNeedsSecondFactor: controller ? controller.needsSecondFactor : false
    property bool openAfterCreate: false
    property string folder: ""

    title: qsTr("Create database")
    primaryButtonText: qsTr("Create")
    closeButtonText: qsTr("Cancel")
    closeOnPrimary: false
    primaryButtonEnabled: nameField.text.trim().length > 0 && folder.length > 0
                          && passwordBox.text.length > 0 && !ctlBusy

    function create() {
        const path = controller.buildDatabasePath(folder, nameField.text.trim())
        if (openAfterCreate)
            controller.createAndOpen(path, passwordBox.text, repeatBox.text)
        else
            controller.createDatabase(path, passwordBox.text, repeatBox.text)
    }

    onAboutToShow: {
        nameField.text = ""
        passwordBox.text = ""
        repeatBox.text = ""
        if (folder.length === 0) {
            const def = controller.defaultDatabasePath()
            folder = def.substring(0, Math.max(def.lastIndexOf("/"), def.lastIndexOf("\\")))
        }
        controller.clearError()
        nameField.forceActiveFocus()
    }
    onPrimaryClicked: create()

    LabeledTextBox {
        id: nameField
        objectName: "databaseName"
        placeholderText: qsTr("Database name")
        infoLabel: ".epdb"
        Layout.fillWidth: true
    }
    RowLayout {
        Layout.fillWidth: true
        TextField {
            objectName: "databaseFolder"
            text: root.folder
            readOnly: true
            placeholderText: qsTr("Folder")
            Accessible.name: qsTr("Folder")
            Layout.fillWidth: true
        }
        IconButton {
            iconName: "folder"
            toolTip: qsTr("Choose folder")
            onClicked: folderLoader.active = true
        }
    }
    EditPasswordBox {
        id: passwordBox
        objectName: "createPassword"
        placeholderText: qsTr("Password")
        Layout.fillWidth: true
    }
    EditPasswordBox {
        id: repeatBox
        objectName: "createRepeat"
        placeholderText: qsTr("Repeat password")
        Layout.fillWidth: true
        Keys.onReturnPressed: root.clickPrimary()
        Keys.onEnterPressed: root.clickPrimary()
    }
    InfoBar {
        visible: open
        open: root.ctlError.length > 0 && root.opened
        severity: Theme.Severity.Error
        message: root.ctlError
        closable: false
        Layout.fillWidth: true
    }

    Loader {
        id: folderLoader
        active: false
        sourceComponent: FolderDialog {
            title: qsTr("Choose folder")
            onAccepted: { root.folder = root.controller.toLocalPath(selectedFolder.toString()); folderLoader.active = false }
            onRejected: folderLoader.active = false
        }
        onLoaded: item.open()
    }

    Connections {
        target: root.controller
        function onCreated(path) { root.close() }
    }
}
