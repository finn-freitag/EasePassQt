import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// C#: Dialogs/Add2FADialog + Views/DialogPages/Add2FAPage. QR code from the screen or the
// webcam (C#), plus from an image file or the clipboard, or the parameters typed by hand.
// Saves through the edit buffer, so an invalid secret keeps the dialog open with a message
// (C# saved it and cleared the secret afterwards: InfoMessages.Invalid2FA).
ContentDialog {
    id: root
    required property var editor          // App::ItemViewModel (edit buffer)
    required property var qrImport        // App::QrImportService
    property var screenScan: null         // App::ScreenScanViewModel
    property var webcamScan: null         // App::WebcamScanViewModel
    property var candidates: []           // several QR codes on the screen: user picks one
    readonly property bool scanning: screenScan !== null && screenScan.busy
    signal committed(int index)
    signal copyRequested(string text)

    function openFor(index) {
        if (!editor.load(index)) return false
        editor.prepare2FA()
        candidates = []
        messageBar.open = false
        twoFactor.sync()
        open()
        return true
    }
    function showMessage(severity, text) {
        messageBar.severity = severity
        messageBar.message = text
        messageBar.open = true
    }
    function applyUri(uri) {
        candidates = []
        if (editor.applyOtpauthUri(uri)) {
            twoFactor.sync()
            showMessage(Theme.Severity.Success, qsTr("The QR code was read."))
        } else {
            showMessage(Theme.Severity.Error, qsTr("The QR code is not a valid two-factor authentication code."))
        }
    }
    function save() {
        if (editor.commit()) {
            const index = editor.index
            close()
            committed(index)
        } else {
            showMessage(Theme.Severity.Error, editor.validationErrors.join("\n"))
        }
    }

    title: qsTr("Add 2FA to") + " " + (editor ? editor.displayName : "")
    primaryButtonText: qsTr("Add")
    primaryButtonEnabled: editor ? editor.secret.trim().length > 0 && !scanning : false
    closeButtonText: qsTr("Cancel")
    closeOnPrimary: false
    onPrimaryClicked: save()
    onClosed: {
        candidates = []
        editor.unload()
    }

    Connections {
        target: root.qrImport
        enabled: root.visible
        function onOtpauthFound(uri) { root.applyUri(uri) }
        function onScanFailed(message) { root.showMessage(Theme.Severity.Error, message) }
    }
    Connections {
        target: root.screenScan
        enabled: root.visible
        function onScanFinished(status) {
            const s = root.screenScan
            // App::ScreenScanViewModel::Status: Idle, Scanning, Found, NoneFound, Multiple, Cancelled, Failed
            switch (status) {
            case 2: root.qrImport.accept(s.resultText); break
            case 3: root.showMessage(Theme.Severity.Warning, qsTr("No QR code found on the screen.")); break
            case 4:
                root.candidates = s.candidates
                root.showMessage(Theme.Severity.Info, qsTr("Several QR codes found. Select one."))
                break
            case 6: root.showMessage(Theme.Severity.Error, s.errorString); break
            default: break
            }
        }
    }

    InfoBar {
        id: messageBar
        objectName: "messageBar"
        managed: true
        open: false
        Layout.fillWidth: true
        onClosed: open = false
    }

    GridLayout {
        columns: 2
        columnSpacing: Theme.spacing
        rowSpacing: Theme.spacing
        Layout.fillWidth: true
        Button {
            objectName: "screenButton"
            text: qsTr("QR Code from screen")
            ToolTip.text: qsTr("Scan a QR code on the screen")
            ToolTip.visible: hovered
            enabled: root.screenScan !== null && root.screenScan.available && !root.scanning
            Layout.fillWidth: true
            onClicked: { messageBar.open = false; root.screenScan.start() }
        }
        Button {
            objectName: "webcamButton"
            text: qsTr("QR Code from webcam")
            ToolTip.text: qsTr("Use your webcam to scan the QR code")
            ToolTip.visible: hovered
            enabled: root.webcamScan !== null && root.webcamScan.available
            Layout.fillWidth: true
            onClicked: webcamDialog.open()
        }
        Button {
            objectName: "fileButton"
            text: qsTr("QR Code from file")
            Layout.fillWidth: true
            onClicked: fileLoader.active = true
        }
        Button {
            objectName: "clipboardButton"
            text: qsTr("QR Code from clipboard")
            Layout.fillWidth: true
            onClicked: root.qrImport.scanClipboard()
        }
    }
    BusyIndicator {
        running: root.scanning
        visible: running
        Layout.alignment: Qt.AlignHCenter
    }
    Repeater {
        objectName: "candidateRepeater"
        model: root.candidates
        delegate: Button {
            required property string modelData
            required property int index
            objectName: "candidate" + index
            text: root.qrImport.describe(modelData)
            Layout.fillWidth: true
            onClicked: root.qrImport.accept(modelData)
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: Theme.spacing
        Rectangle { color: Theme.divider; implicitHeight: 2; Layout.fillWidth: true }
        Label { text: qsTr("or"); color: Theme.textSecondary }
        Rectangle { color: Theme.divider; implicitHeight: 2; Layout.fillWidth: true }
    }

    TwoFactorFields {
        id: twoFactor
        objectName: "twoFactorFields"
        editor: root.editor
        Layout.fillWidth: true
        onCopyRequested: (t) => root.copyRequested(t)
    }

    WebcamScanDialog {
        id: webcamDialog
        objectName: "webcamDialog"
        scanner: root.webcamScan
        parent: root.parent
        onCodeScanned: (text) => root.qrImport.accept(text)
    }
    Loader {
        id: fileLoader
        active: false
        visible: false // not part of the layout
        sourceComponent: FileDialog {
            title: qsTr("Select QR code image")
            nameFilters: [qsTr("Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp)"), qsTr("All files (*)")]
            onAccepted: { root.qrImport.scanFile(selectedFile.toString()); fileLoader.active = false }
            onRejected: fileLoader.active = false
            Component.onCompleted: open()
        }
    }
}
