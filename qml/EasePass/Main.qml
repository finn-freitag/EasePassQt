import QtQuick
import QtQuick.Controls

// QML application window (D14: second target next to the legacy Widgets app).
// Until the pages exist (T12+) it shows the component gallery.
ApplicationWindow {
    id: window
    width: 1000
    height: 720
    minimumWidth: 480
    minimumHeight: 400
    visible: true
    title: qsTr("Ease Pass")
    color: Theme.background

    Gallery {
        anchors.fill: parent
    }
}
