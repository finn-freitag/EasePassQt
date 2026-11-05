import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// WinUI InfoBar replacement (C#: InfoMessages.cs uses InfoBar for all non-modal feedback).
// Shown while `open`; the close button (closable) sets open=false and emits closed().
// autoCloseInterval > 0 closes it automatically after that many milliseconds.
Control {
    id: root
    property int severity: Theme.Severity.Info
    property string title: ""
    property string message: ""
    property bool open: true
    property bool closable: true
    property int autoCloseInterval: 0
    // managed: `open` is bound by the owner (e.g. to an error message). close() then only
    // emits closed() and the owner clears its state, so the binding is never overwritten.
    property bool managed: false
    default property alias actionContent: actionHost.data
    signal closed()

    function show() { open = true }
    function close() {
        if (!open) return
        if (!managed)
            open = false
        closed()
    }

    visible: open
    implicitWidth: 400
    implicitHeight: open ? layout.implicitHeight + topPadding + bottomPadding : 0
    padding: Theme.spacingM
    leftPadding: Theme.spacingL

    Accessible.role: Accessible.AlertMessage
    Accessible.name: title.length > 0 ? title + ": " + message : message

    onOpenChanged: if (open && autoCloseInterval > 0) autoTimer.restart()
    Component.onCompleted: if (open && autoCloseInterval > 0) autoTimer.restart()
    Timer {
        id: autoTimer
        interval: Math.max(1, root.autoCloseInterval)
        onTriggered: root.close()
    }

    background: Rectangle {
        radius: Theme.radiusSmall
        color: Theme.severityBackground(root.severity)
        border.color: Theme.cardBorder
    }

    contentItem: RowLayout {
        id: layout
        spacing: Theme.spacingM
        Icon {
            objectName: "severityIcon"
            source: Theme.severityIcon(root.severity)
            color: Theme.severityForeground(root.severity)
            size: Theme.iconSizeLarge
            Layout.alignment: Qt.AlignTop
            Layout.topMargin: Theme.spacingS
        }
        Label {
            text: root.title
            visible: text.length > 0
            font.weight: Font.DemiBold
            color: Theme.textPrimary
            Layout.alignment: Qt.AlignTop
            Layout.topMargin: Theme.spacingS
        }
        Label {
            text: root.message
            color: Theme.textPrimary
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            Layout.topMargin: Theme.spacingS
        }
        Item {
            id: actionHost
            implicitWidth: childrenRect.width
            implicitHeight: childrenRect.height
            Layout.alignment: Qt.AlignVCenter
        }
        IconButton {
            objectName: "closeButton"
            visible: root.closable
            iconName: "close"
            toolTip: qsTr("Close")
            Layout.alignment: Qt.AlignTop
            onClicked: root.close()
        }
    }
}
