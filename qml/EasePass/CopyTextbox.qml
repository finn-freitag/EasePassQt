import QtQuick
import QtQuick.Controls

// C#: Controls/CopyTextbox.xaml(.cs) - read-only text with a copy button.
// The component never touches the clipboard itself: it emits copyRequested(text) and the
// view model routes that to ClipboardManager (auto-clear, history exclusion).
TextField {
    id: root
    property bool doubleClickToCopy: true   // C#: AppSettings.DoubleTapToCopy
    property string accessibleName: placeholderText
    signal copyRequested(string text)

    function copy() { root.copyRequested(root.text) }

    readOnly: true
    selectByMouse: true
    rightPadding: copyButton.width + Theme.spacingS
    Accessible.name: accessibleName

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

    IconButton {
        id: copyButton
        objectName: "copyButton"
        iconName: "copy"
        toolTip: qsTr("Copy")
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        focusPolicy: Qt.TabFocus
        onClicked: root.copy()
    }
}
