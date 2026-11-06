import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// QML application window. The C++ objects come in as initial properties from
// App::loadMainWindow (src/app/MainWindowLoader.cpp, AppContext).
ApplicationWindow {
    id: window
    required property var appController
    required property var databaseController
    required property var dailyTip
    required property var dialogService
    required property var clipboard
    required property var passwordList
    required property var itemViewModel
    required property var passwordActions
    required property var websiteIcons
    required property var settings
    required property var editor
    required property var generator
    required property var qrImport
    property var screenScan: null
    property var webcamScan: null
    property var databaseManagement: null
    property var printer: null
    property var settingsViewModel: null
    property var about: null
    property var autoLogout: null

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
    // Shown by main.cpp after the stored geometry/state was applied (WindowStateController, T19).
    visible: false
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
            case window.pagePasswords: return passwordsPage
            case window.pageDatabase: return window.databaseManagement ? databasePage : placeholderPage
            case window.pageSettings: return window.settingsViewModel ? settingsPage : placeholderPage
            case window.pageAbout: return window.about ? aboutPage : placeholderPage
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
        id: passwordsPage
        PasswordsPage {
            listModel: window.passwordList
            item: window.itemViewModel
            actions: window.passwordActions
            settings: window.settings
            controller: window.databaseController
            clipboard: window.clipboard
            icons: window.websiteIcons
            onSettingsRequested: window.appController.navigate(window.pageSettings)
            onAboutRequested: window.appController.navigate(window.pageAbout)
            onManageDatabaseRequested: window.appController.navigate(window.pageDatabase)
            onAddRequested: itemDialog.openNew()
            onEditRequested: (index) => itemDialog.openEdit(index)
            onAdd2FARequested: (index) => add2FADialog.openFor(index)
            onGenerateRequested: generatorDialog.open()
        }
    }
    // Shows the saved item in the list after an add/edit dialog.
    function selectItem(index) {
        if (pageLoader.item && pageLoader.item.select !== undefined)
            pageLoader.item.select(index)
    }

    // --- Item dialogs (T15) -----------------------------------------------------------
    ItemEditDialog {
        id: itemDialog
        objectName: "itemDialog"
        parent: Overlay.overlay
        editor: window.editor
        generator: window.generator
        onCommitted: (index) => window.selectItem(index)
        onCopyRequested: (t) => window.clipboard.copy(t, true)
    }
    Add2FADialog {
        id: add2FADialog
        objectName: "add2FADialog"
        parent: Overlay.overlay
        editor: window.editor
        qrImport: window.qrImport
        screenScan: window.screenScan
        webcamScan: window.webcamScan
        onCommitted: (index) => window.selectItem(index)
        onCopyRequested: (t) => window.clipboard.copy(t, true)
    }
    PasswordGeneratorDialog {
        id: generatorDialog
        objectName: "generatorDialog"
        parent: Overlay.overlay
        generator: window.generator
        onCopyRequested: (t) => window.clipboard.copy(t, true)
    }
    Component {
        id: databasePage
        DatabasePage {
            manager: window.databaseManagement
            controller: window.databaseController
            printer: window.printer
            autoLogout: window.autoLogout
            showBack: window.appController.canGoBack
            onBackRequested: window.appController.goBack()
            onCopyRequested: (t) => window.clipboard.copy(t, true)
        }
    }
    Component {
        id: settingsPage
        SettingsPage {
            viewModel: window.settingsViewModel
            databaseOpen: window.databaseController.isOpen
            showBack: window.appController.canGoBack
            onBackRequested: window.appController.goBack()
            onManageDatabaseRequested: window.appController.navigate(window.pageDatabase)
        }
    }
    Component {
        id: aboutPage
        AboutPage {
            about: window.about
            showBack: window.appController.canGoBack
            onBackRequested: window.appController.goBack()
        }
    }
    Component {
        id: placeholderPage
        PlaceholderPage {
            title: window.databaseController.databaseName
            showBack: window.appController.canGoBack
            onBackRequested: window.appController.goBack()
            onLockRequested: window.databaseController.lock()
        }
    }

    // --- Inactivity logout (T19) --------------------------------------------------------
    Loader {
        active: window.autoLogout !== null
        sourceComponent: AutoLogoutDialog {
            objectName: "autoLogoutDialog"
            parent: Overlay.overlay
            autoLogout: window.autoLogout
            controller: window.databaseController
        }
    }
    Connections {
        target: window.autoLogout
        function onLoggedOut() {
            // C#: InfoMessages.AutomaticallyLoggedOut
            globalInfo.severity = Theme.Severity.Info
            globalInfo.title = ""
            globalInfo.message = qsTr("You were logged out automatically because of inactivity.")
            globalInfo.show()
        }
    }
    // Locking (manually or automatically) closes the item dialogs; their buffers are gone
    // (C#: AutoLogoutContentDialog.InactivityStarted hides all dialogs).
    Connections {
        target: window.databaseController
        function onStateChanged() {
            if (window.databaseController.isOpen)
                return
            itemDialog.close()
            add2FADialog.close()
            generatorDialog.close()
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
