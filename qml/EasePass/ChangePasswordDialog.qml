import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// C#: Dialogs/ChangePasswordDialog + Views/DialogPages/ChangePasswordPage - current, new and
// repeated master password. Validation errors (too short, wrong current password, no match)
// keep the dialog open and show the message inside it.
ContentDialog {
    id: root
    required property var manager
    property string errorText: ""
    readonly property bool working: manager ? manager.busy : false

    title: qsTr("Change password for %1").arg(manager ? manager.databaseName : "")
    primaryButtonText: qsTr("Change")
    closeButtonText: qsTr("Cancel")
    closeOnPrimary: false
    primaryButtonEnabled: !working && currentBox.text.length > 0 && newBox.text.length > 0

    onAboutToShow: {
        currentBox.text = ""
        newBox.text = ""
        repeatBox.text = ""
        errorText = ""
        currentBox.forceActiveFocus()
    }
    onPrimaryClicked: {
        errorText = ""
        manager.changePassword(currentBox.text, newBox.text, repeatBox.text)
    }
    onClosed: {
        currentBox.text = ""
        newBox.text = ""
        repeatBox.text = ""
    }

    Label {
        text: qsTr("Be sure to select a password that you'll always remember. If the password is lost, so are all your stored credentials, with no means of recovery. Additionally, ensure you back up your passwords either to an external offline drive or the cloud.")
        wrapMode: Text.WordWrap
        color: Theme.textSecondary
        Layout.fillWidth: true
    }
    EditPasswordBox {
        id: currentBox
        objectName: "currentPassword"
        placeholderText: qsTr("Current password")
        doubleClickToCopy: false
        Layout.fillWidth: true
    }
    EditPasswordBox {
        id: newBox
        objectName: "newPassword"
        placeholderText: qsTr("New password")
        doubleClickToCopy: false
        Layout.fillWidth: true
    }
    EditPasswordBox {
        id: repeatBox
        objectName: "repeatPassword"
        placeholderText: qsTr("Repeat new password")
        doubleClickToCopy: false
        Layout.fillWidth: true
        Keys.onReturnPressed: root.clickPrimary()
        Keys.onEnterPressed: root.clickPrimary()
    }
    ProgressBar {
        indeterminate: true
        visible: root.working
        Layout.fillWidth: true
    }
    InfoBar {
        objectName: "changePasswordError"
        visible: open
        open: root.errorText.length > 0
        severity: Theme.Severity.Error
        message: root.errorText
        closable: false
        Layout.fillWidth: true
    }

    Connections {
        target: root.manager
        enabled: root.visible
        function onPasswordChangeFinished(ok, message) {
            if (ok)
                root.close()
            else
                root.errorText = message
        }
    }
}
