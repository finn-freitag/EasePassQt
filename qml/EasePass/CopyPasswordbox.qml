import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// C#: Controls/CopyPasswordbox.xaml(.cs) - read-only password display (masked with '•'
// unless showPassword), show/hide toggle, copy button and a small PasswordSafetyChart.
// The chart's checks come from the view model (password evaluation lives in C++).
TextField {
    id: root
    property string password: ""
    property bool showPassword: false
    property bool doubleClickToCopy: true
    property bool showSafetyChart: true
    property alias safetyChecks: chart.checks
    property string accessibleName: qsTr("Password")
    signal copyRequested(string text)

    function copy() { root.copyRequested(root.password) }

    text: showPassword ? password : "•".repeat(password.length)
    readOnly: true
    selectByMouse: showPassword
    rightPadding: buttons.implicitWidth + Theme.spacingS
    Accessible.name: accessibleName
    Accessible.passwordEdit: !showPassword

    Keys.onPressed: (event) => {
        if (event.matches(StandardKey.Copy)) {
            root.copy()
            event.accepted = true
        }
    }
    // Double click copies (C#: AppSettings.DoubleTapToCopy). TextInput grabs the mouse, so a
    // TapHandler never sees the second click; detect it from the released() timing instead.
    property double _lastRelease: 0
    onReleased: {
        const now = Date.now()
        if (now - _lastRelease <= Qt.styleHints.mouseDoubleClickInterval) {
            _lastRelease = 0
            if (root.doubleClickToCopy) root.copy()
        } else {
            _lastRelease = now
        }
    }

    RowLayout {
        id: buttons
        spacing: 0
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter

        PasswordSafetyChart {
            id: chart
            objectName: "safetyChart"
            visible: root.showSafetyChart
            showInfo: false
            Layout.preferredWidth: 22
            Layout.preferredHeight: 22
            Layout.rightMargin: Theme.spacingS
        }
        IconButton {
            objectName: "showButton"
            iconName: root.showPassword ? "eye-off" : "eye"
            toolTip: root.showPassword ? qsTr("Hide password") : qsTr("Show password")
            checkable: true
            checked: root.showPassword
            focusPolicy: Qt.TabFocus
            onClicked: root.showPassword = !root.showPassword
        }
        IconButton {
            objectName: "copyButton"
            iconName: "copy"
            toolTip: qsTr("Copy password")
            focusPolicy: Qt.TabFocus
            onClicked: root.copy()
        }
    }
}
