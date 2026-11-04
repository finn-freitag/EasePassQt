import QtQuick
import QtQuick.Controls

// Resizable pane splitter (C#: password list | item details). Children use
// SplitView.preferredWidth / minimumWidth; the handle can be dragged with the mouse.
SplitView {
    id: root
    handle: Rectangle {
        implicitWidth: root.orientation === Qt.Horizontal ? 5 : root.width
        implicitHeight: root.orientation === Qt.Horizontal ? root.height : 5
        color: SplitHandle.pressed ? Theme.accent
             : SplitHandle.hovered ? Theme.divider : "transparent"
        Rectangle {
            anchors.centerIn: parent
            width: root.orientation === Qt.Horizontal ? 1 : parent.width
            height: root.orientation === Qt.Horizontal ? parent.height : 1
            color: Theme.divider
        }
    }
}
