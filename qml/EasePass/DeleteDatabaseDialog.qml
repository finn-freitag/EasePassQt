import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// C#: Dialogs/ConfirmDeleteDatabaseDialog. The delete button only becomes available once the
// database name has been typed (steps.txt T16: typed confirmation).
ContentDialog {
    id: root
    required property var manager
    property string path: ""
    property string errorText: ""
    readonly property string databaseName: manager ? manager.nameForPath(path) : ""
    signal deleted(string path)

    function openFor(p) {
        path = p
        open()
    }

    title: qsTr("Confirm delete database")
    primaryButtonText: qsTr("Delete")
    closeButtonText: qsTr("Close")
    closeOnPrimary: false
    primaryButtonEnabled: confirmBox.text === databaseName && databaseName.length > 0

    onAboutToShow: {
        confirmBox.text = ""
        errorText = ""
        confirmBox.forceActiveFocus()
    }
    onPrimaryClicked: {
        errorText = manager.deleteDatabase(path, confirmBox.text)
        if (errorText.length === 0) {
            root.deleted(path)
            close()
        }
    }

    Label {
        text: qsTr("Confirm to delete database:") + "\n" + root.databaseName + "\n" + root.path
        wrapMode: Text.WrapAnywhere
        color: Theme.textPrimary
        Layout.fillWidth: true
    }
    Label {
        text: qsTr("The file is removed permanently. Type the name of the database to confirm.")
        wrapMode: Text.WordWrap
        color: Theme.textSecondary
        Layout.fillWidth: true
    }
    TextField {
        id: confirmBox
        objectName: "deleteConfirmName"
        placeholderText: root.databaseName
        Accessible.name: qsTr("Database name")
        Layout.fillWidth: true
        Keys.onReturnPressed: root.clickPrimary()
        Keys.onEnterPressed: root.clickPrimary()
    }
    InfoBar {
        objectName: "deleteError"
        visible: open
        open: root.errorText.length > 0
        severity: Theme.Severity.Error
        message: root.errorText
        closable: false
        Layout.fillWidth: true
    }
}
