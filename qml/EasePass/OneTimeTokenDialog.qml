import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// C#: Dialogs/ShowSecondFactorDialog - after logging in to a database with a one-time second
// factor, the new token is shown exactly once.
ContentDialog {
    id: root
    property string token: ""
    signal copyRequested(string text)

    function showToken(t) {
        token = t
        open()
    }

    title: qsTr("Your new second factor")
    closeButtonText: qsTr("I have saved it")
    onClosed: token = ""

    Label {
        text: qsTr("This database uses a one-time second factor. Save this new token; you need it for your next login. It will not be shown again.")
        wrapMode: Text.WordWrap
        color: Theme.textPrimary
        Layout.fillWidth: true
    }
    CopyTokenBox {
        objectName: "tokenBox"
        token: root.token
        showToken: true
        Layout.fillWidth: true
        onCopyRequested: (t) => root.copyRequested(t)
    }
}
