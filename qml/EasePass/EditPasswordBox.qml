import QtQuick
import QtQuick.Controls

// C#: Controls/EditPasswordBox.xaml(.cs) - editable password field with reveal toggle.
// Ctrl+C emits copyRequested(password) (a masked TextField would otherwise refuse to copy).
TextField {
    id: root
    property alias password: root.text
    property bool revealed: false
    property bool doubleClickToCopy: true
    property string accessibleName: placeholderText.length > 0 ? placeholderText : qsTr("Password")
    signal copyRequested(string text)

    echoMode: revealed ? TextInput.Normal : TextInput.Password
    selectByMouse: true
    inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
    rightPadding: revealButton.width + Theme.spacingS
    Accessible.name: accessibleName

    Keys.onPressed: (event) => {
        if (event.matches(StandardKey.Copy)) {
            root.copyRequested(root.text)
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
            if (root.doubleClickToCopy) root.copyRequested(root.text)
        } else {
            _lastRelease = now
        }
    }

    IconButton {
        id: revealButton
        objectName: "revealButton"
        iconName: root.revealed ? "eye-off" : "eye"
        toolTip: root.revealed ? qsTr("Hide password") : qsTr("Show password")
        checkable: true
        checked: root.revealed
        focusPolicy: Qt.TabFocus
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        onClicked: root.revealed = !root.revealed
    }
}
