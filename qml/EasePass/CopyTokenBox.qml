import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// C#: Controls/CopyTokenBox.xaml(.cs) - read-only TOTP token, hidden by default, with a
// show toggle, copy button and a ring showing the remaining validity (progress 1 -> 0).
TextField {
    id: root
    property string token: ""
    property bool showToken: false
    property real progress: 1.0   // remaining fraction of the TOTP interval
    property string accessibleName: qsTr("Token")
    signal copyRequested(string text)

    function copy() { root.copyRequested(root.token) }

    text: showToken ? token : "*".repeat(token.length)
    readOnly: true
    rightPadding: buttons.implicitWidth + Theme.spacingS
    Accessible.name: accessibleName
    Accessible.passwordEdit: !showToken

    Keys.onPressed: (event) => {
        if (event.matches(StandardKey.Copy)) {
            root.copy()
            event.accepted = true
        }
    }

    RowLayout {
        id: buttons
        spacing: 0
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter

        Canvas {
            id: ring
            objectName: "progressRing"
            Layout.preferredWidth: 18
            Layout.preferredHeight: 18
            Layout.rightMargin: Theme.spacingS
            property real value: Math.max(0, Math.min(1, root.progress))
            property color ringColor: Theme.accent
            property color trackColor: Theme.divider
            onValueChanged: requestPaint()
            onRingColorChanged: requestPaint()
            Accessible.role: Accessible.ProgressBar
            Accessible.name: qsTr("Time remaining")
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                const r = width / 2 - 2
                ctx.lineWidth = 2.5
                ctx.strokeStyle = trackColor
                ctx.beginPath(); ctx.arc(width / 2, height / 2, r, 0, 2 * Math.PI); ctx.stroke()
                ctx.strokeStyle = ringColor
                ctx.beginPath()
                ctx.arc(width / 2, height / 2, r, -Math.PI / 2, -Math.PI / 2 + 2 * Math.PI * value)
                ctx.stroke()
            }
        }
        IconButton {
            objectName: "showButton"
            iconName: root.showToken ? "eye-off" : "eye"
            toolTip: root.showToken ? qsTr("Hide token") : qsTr("Show token")
            checkable: true
            checked: root.showToken
            focusPolicy: Qt.TabFocus
            onClicked: root.showToken = !root.showToken
        }
        IconButton {
            objectName: "copyButton"
            iconName: "copy"
            toolTip: qsTr("Copy token")
            focusPolicy: Qt.TabFocus
            onClicked: root.copy()
        }
    }
}
