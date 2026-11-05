import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// C#: AppWindows/WebcamScannerWindow - live camera preview, closes on the first QR code.
ContentDialog {
    id: root
    property var scanner: null            // App::WebcamScanViewModel
    signal codeScanned(string text)

    title: qsTr("Scan QR code with webcam")
    closeButtonText: qsTr("Cancel")
    onOpened: if (scanner) scanner.start()
    onClosed: if (scanner) scanner.stop()

    Connections {
        target: root.scanner
        enabled: root.visible
        function onCodeDetected(text) {
            root.close()
            root.codeScanned(text)
        }
    }

    ComboBox {
        objectName: "cameraBox"
        model: root.scanner ? root.scanner.deviceNames : []
        currentIndex: root.scanner ? root.scanner.deviceIndex : -1
        visible: count > 1
        Accessible.name: qsTr("Camera")
        Layout.fillWidth: true
        onActivated: (index) => root.scanner.deviceIndex = index
    }
    Rectangle {
        color: Theme.videoBackground
        radius: Theme.radiusSmall
        Layout.fillWidth: true
        Layout.preferredHeight: width * 9 / 16
        Image {
            objectName: "webcamPreview"
            anchors.fill: parent
            fillMode: Image.PreserveAspectFit
            cache: false
            asynchronous: false
            source: root.visible && root.scanner && root.scanner.active
                    ? "image://webcam/" + root.scanner.frameRevision : ""
        }
    }
    Label {
        objectName: "webcamStatus"
        text: root.scanner && root.scanner.errorString.length > 0
              ? root.scanner.errorString : qsTr("Hold the QR code in front of the camera.")
        color: root.scanner && root.scanner.errorString.length > 0 ? Theme.errorForeground : Theme.textSecondary
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
}
