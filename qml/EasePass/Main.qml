import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// QML application window (D14: second target next to the legacy Widgets app).
// The C++ objects come in as initial properties from src/qmlapp/main.cpp (AppContext).
ApplicationWindow {
    id: window
    required property var appController
    required property var databaseController
    required property var dailyTip
    required property var dialogService
    required property var clipboard

    // Values of App::AppController::Page
    readonly property int pageLogin: 0
    readonly property int pageRegister: 1
    readonly property int pagePasswords: 2
    readonly property int pageDatabase: 3
    readonly property int pageSettings: 4
    readonly property int pageAbout: 5
    readonly property int pagePlugins: 6

    width: 1100
    height: 700
    minimumWidth: 480
    minimumHeight: 400
    visible: true
    title: databaseController.isOpen ? databaseController.databaseName + " - " + qsTr("Ease Pass") : qsTr("Ease Pass")
    color: Theme.background

    Loader {
        id: pageLoader
        objectName: "pageLoader"
        anchors.fill: parent
        focus: true
        sourceComponent: {
            switch (window.appController.page) {
            case window.pageLogin: return loginPage
            case window.pageRegister: return registerPage
            default: return placeholderPage
            }
        }
        onLoaded: item.forceActiveFocus()
    }

    Component {
        id: loginPage
        LoginPage {
            controller: window.databaseController
            dailyTip: window.dailyTip.tip
            onCopyRequested: (t) => window.clipboard.copy(t, true)
        }
    }
    Component {
        id: registerPage
        RegisterPage {
            controller: window.databaseController
            onImportFinished: window.appController.start(false) // C#: back to the login page
        }
    }
    Component {
        id: placeholderPage
        PlaceholderPage {
            title: window.databaseController.databaseName
            onLockRequested: window.databaseController.lock()
        }
    }

    // One-time second factor rotated after login (C#: ShowSecondFactorDialog).
    OneTimeTokenDialog {
        id: tokenDialog
        objectName: "tokenDialog"
        parent: Overlay.overlay
        onCopyRequested: (t) => window.clipboard.copy(t, true)
    }
    Connections {
        target: window.databaseController
        function onOneTimeTokenIssued(token) { tokenDialog.showToken(token) }
    }

    // Generic C++ -> QML questions (DialogService.ask).
    ContentDialog {
        id: serviceDialog
        property int requestId: -1
        property string text: ""
        parent: Overlay.overlay
        onPrimaryClicked: window.dialogService.respond(requestId, 1)
        onSecondaryClicked: window.dialogService.respond(requestId, 2)
        onCloseClicked: window.dialogService.respond(requestId, 0)
        Label {
            text: serviceDialog.text
            wrapMode: Text.WordWrap
            color: Theme.textPrimary
            Layout.fillWidth: true
        }
    }
    Connections {
        target: window.dialogService
        function onDialogRequested(id, title, message, primaryText, secondaryText, closeText) {
            serviceDialog.requestId = id
            serviceDialog.title = title
            serviceDialog.text = message
            serviceDialog.primaryButtonText = primaryText
            serviceDialog.secondaryButtonText = secondaryText
            serviceDialog.closeButtonText = closeText
            serviceDialog.open()
        }
        function onDialogClosed(id, result) {
            if (id === serviceDialog.requestId && serviceDialog.visible)
                serviceDialog.close() // answered elsewhere (e.g. cancelled on lock)
        }
        function onInfoRequested(severity, title, message) {
            globalInfo.severity = severity
            globalInfo.title = title
            globalInfo.message = message
            globalInfo.show()
        }
    }

    // Non-modal feedback (C#: InfoMessages) at the bottom of the window.
    InfoBar {
        id: globalInfo
        objectName: "globalInfo"
        open: false
        autoCloseInterval: 6000
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Theme.spacingL
        z: 10
    }
}
