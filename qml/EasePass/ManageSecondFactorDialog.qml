import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// C#: Dialogs/ManageSecondFactorDialog + Views/DialogPages/ManageSecondFactorPage and
// Dialogs/AddSecondFactorConfirmationDialog: switch the second factor on/off, pick Token or
// OTP and show the token to write down. Turning it on asks for confirmation first.
ContentDialog {
    id: root
    required property var manager
    property string token: ""
    property string errorText: ""
    readonly property bool working: manager ? manager.busy : false
    signal copyRequested(string text)

    title: qsTr("Manage second factor for %1").arg(manager ? manager.databaseName : "")
    primaryButtonText: qsTr("Change")
    closeButtonText: qsTr("Cancel")
    closeOnPrimary: false
    primaryButtonEnabled: !working

    function apply() {
        errorText = ""
        manager.applySecondFactor(enableSwitch.checked, typeBox.currentIndex + 1, token)
    }

    onAboutToShow: {
        errorText = ""
        token = manager.currentSecondFactorToken()
        if (token.length === 0)
            token = manager.generateSecondFactorToken() // C#: TokenHelper.Generate(12)
        enableSwitch.checked = manager.secondFactorEnabled
        typeBox.currentIndex = manager.secondFactorType === 2 ? 1 : 0
    }
    onPrimaryClicked: {
        if (enableSwitch.checked && !manager.secondFactorEnabled)
            confirmDialog.open()
        else
            apply()
    }
    onClosed: token = ""

    Label {
        text: qsTr("Enable second factor")
        color: Theme.textPrimary
        Layout.fillWidth: true
    }
    Switch {
        id: enableSwitch
        objectName: "secondFactorSwitch"
        Accessible.name: qsTr("Enable second factor")
        ToolTip.visible: hovered
        ToolTip.text: qsTr("Enables the second factor for the database")
    }
    Label {
        text: qsTr("Second factor type")
        color: Theme.textPrimary
        Layout.fillWidth: true
    }
    ComboBox {
        id: typeBox
        objectName: "secondFactorType"
        enabled: enableSwitch.checked
        model: [qsTr("Token"), qsTr("One-time token (OTP)")]
        Accessible.name: qsTr("Second factor type")
        Layout.fillWidth: true
    }
    Label {
        text: typeBox.currentIndex === 1
              ? qsTr("A new token is shown after every login; the old one stops working.")
              : qsTr("The token stays the same until you change the second factor.")
        visible: enableSwitch.checked
        wrapMode: Text.WordWrap
        color: Theme.textSecondary
        Layout.fillWidth: true
    }
    Label {
        text: qsTr("Second factor token")
        color: Theme.textPrimary
        visible: enableSwitch.checked
        Layout.fillWidth: true
    }
    CopyTokenBox {
        objectName: "secondFactorToken"
        visible: enableSwitch.checked
        token: root.token
        showToken: true
        showProgress: false
        Layout.fillWidth: true
        onCopyRequested: (t) => root.copyRequested(t)
    }
    ProgressBar {
        indeterminate: true
        visible: root.working
        Layout.fillWidth: true
    }
    InfoBar {
        objectName: "secondFactorError"
        visible: open
        open: root.errorText.length > 0
        severity: Theme.Severity.Error
        message: root.errorText
        closable: false
        Layout.fillWidth: true
    }

    ContentDialog {
        id: confirmDialog
        objectName: "confirmSecondFactor"
        parent: Overlay.overlay
        title: qsTr("Confirm enabling second factor")
        primaryButtonText: qsTr("Confirm")
        closeButtonText: qsTr("Cancel")
        onPrimaryClicked: root.apply()
        Label {
            text: qsTr("Are you sure you want to enable a second factor for the database \"%1\"?\nIf you forget your password or lose the token you will have no access to your database!")
                  .arg(root.manager ? root.manager.databaseName : "")
            wrapMode: Text.WordWrap
            color: Theme.textPrimary
            Layout.fillWidth: true
        }
    }

    Connections {
        target: root.manager
        enabled: root.visible
        function onSecondFactorFinished(ok, message) {
            if (ok)
                root.close()
            else
                root.errorText = message
        }
    }
}
