import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Warning before the inactivity logout (steps.txt T19, new compared to C# which locked without
// warning). Opens while autoLogout.warningActive; any input (ActivityEventFilter), "Stay logged
// in" or Escape restarts the inactivity period, "Log out now" locks immediately.
ContentDialog {
    id: root
    required property var autoLogout
    required property var controller
    readonly property int secondsLeft: autoLogout ? autoLogout.secondsLeft : 0
    readonly property bool active: autoLogout ? autoLogout.warningActive : false
    property int initialSeconds: 1

    title: qsTr("Still there?")
    primaryButtonText: qsTr("Stay logged in")
    secondaryButtonText: qsTr("Log out now")
    closeButtonText: ""

    onActiveChanged: {
        if (active) {
            initialSeconds = Math.max(1, secondsLeft)
            open()
        } else if (visible) {
            close()
        }
    }
    onPrimaryClicked: autoLogout.reportActivity()
    onCloseClicked: autoLogout.reportActivity()      // Escape
    onSecondaryClicked: controller.lock()

    Label {
        objectName: "countdownLabel"
        text: qsTr("You will be logged out in %n second(s) because of inactivity.", "", root.secondsLeft)
        wrapMode: Text.WordWrap
        color: Theme.textPrimary
        Layout.fillWidth: true
    }
    ProgressBar {
        objectName: "countdownBar"
        from: 0
        to: root.initialSeconds
        value: root.secondsLeft
        Layout.fillWidth: true
    }
}
