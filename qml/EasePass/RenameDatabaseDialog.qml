import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// C#: Dialogs/RenameDatabaseDialog (never wired up in C#): new file name in the same folder.
ContentDialog {
    id: root
    required property var manager
    property string path: ""
    property string errorText: ""

    function openFor(p) {
        path = p
        open()
    }

    title: qsTr("Rename database")
    primaryButtonText: qsTr("Rename")
    closeButtonText: qsTr("Cancel")
    closeOnPrimary: false
    primaryButtonEnabled: nameBox.text.trim().length > 0 && manager && nameBox.text.trim() !== manager.nameForPath(path)

    onAboutToShow: {
        nameBox.text = manager.nameForPath(path)
        errorText = ""
        nameBox.forceActiveFocus()
        nameBox.selectAll()
    }
    onPrimaryClicked: {
        errorText = manager.rename(path, nameBox.text.trim())
        if (errorText.length === 0)
            close()
    }

    LabeledTextBox {
        id: nameBox
        objectName: "renameName"
        placeholderText: qsTr("Database name")
        infoLabel: ".epdb"
        Layout.fillWidth: true
        Keys.onReturnPressed: root.clickPrimary()
        Keys.onEnterPressed: root.clickPrimary()
    }
    InfoBar {
        objectName: "renameError"
        visible: open
        open: root.errorText.length > 0
        severity: Theme.Severity.Error
        message: root.errorText
        closable: false
        Layout.fillWidth: true
    }
}
