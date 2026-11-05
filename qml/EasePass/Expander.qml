import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// WinUI Expander replacement: a header card that shows/hides its content (default property).
ColumnLayout {
    id: root
    property string iconName: ""
    property string header: ""
    property bool expanded: false
    default property alias content: contentColumn.data
    spacing: 0

    SettingsControl {
        id: headerCard
        objectName: "expanderHeader"
        iconName: root.iconName
        header: root.header
        clickable: true
        Layout.fillWidth: true
        Accessible.role: Accessible.Button
        Accessible.checkable: true
        Accessible.checked: root.expanded
        onClicked: root.expanded = !root.expanded
    }
    Pane {
        visible: root.expanded
        padding: Theme.spacingL
        Layout.fillWidth: true
        background: Rectangle {
            color: Theme.card
            border.color: Theme.cardBorder
            radius: Theme.radiusSmall
        }
        ColumnLayout {
            id: contentColumn
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: Theme.spacing
        }
    }
}
