import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// C#: Dialogs/EnterPasswordDialog - asks for a password. askRepeat adds a second field (new
// password for "Export with new password"); showSecondFactor adds the second factor field.
// The caller reports failures through errorText and closes the dialog on success.
ContentDialog {
    id: root
    property bool askRepeat: false
    property bool showSecondFactor: false
    property bool working: false
    property string errorText: ""
    readonly property string password: passwordBox.text
    readonly property string repeat: repeatBox.text
    readonly property string secondFactor: secondFactorBox.text
    signal submitted(string password, string repeat, string secondFactor)

    primaryButtonText: qsTr("OK")
    closeButtonText: qsTr("Cancel")
    closeOnPrimary: false
    primaryButtonEnabled: !working && passwordBox.text.length > 0

    function clear() {
        passwordBox.text = ""
        repeatBox.text = ""
        secondFactorBox.text = ""
    }
    onAboutToShow: {
        clear()
        errorText = ""
        showSecondFactor = false
        passwordBox.forceActiveFocus()
    }
    onClosed: clear()
    onPrimaryClicked: {
        errorText = ""
        submitted(passwordBox.text, repeatBox.text, secondFactorBox.text)
    }

    EditPasswordBox {
        id: passwordBox
        objectName: "promptPassword"
        placeholderText: root.askRepeat ? qsTr("New password") : qsTr("Password")
        doubleClickToCopy: false
        Layout.fillWidth: true
        Keys.onReturnPressed: root.clickPrimary()
        Keys.onEnterPressed: root.clickPrimary()
    }
    EditPasswordBox {
        id: repeatBox
        objectName: "promptRepeat"
        visible: root.askRepeat
        placeholderText: qsTr("Repeat new password")
        doubleClickToCopy: false
        Layout.fillWidth: true
        Keys.onReturnPressed: root.clickPrimary()
        Keys.onEnterPressed: root.clickPrimary()
    }
    EditPasswordBox {
        id: secondFactorBox
        objectName: "promptSecondFactor"
        visible: root.showSecondFactor
        placeholderText: qsTr("Second factor")
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
        objectName: "promptError"
        visible: open
        open: root.errorText.length > 0
        severity: Theme.Severity.Error
        message: root.errorText
        closable: false
        Layout.fillWidth: true
    }
}
