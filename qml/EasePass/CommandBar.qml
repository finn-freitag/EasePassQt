import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// WinUI CommandBar replacement: primary Actions as buttons, secondary Actions in a "more"
// overflow menu. Action.icon.name is a Theme icon name; Action.text is the label/tooltip.
ToolBar {
    id: root
    property list<Action> primaryActions
    property list<Action> secondaryActions
    property bool showLabels: false

    Accessible.role: Accessible.ToolBar
    background: Item {}

    RowLayout {
        anchors.fill: parent
        spacing: Theme.spacingXS
        Item { Layout.fillWidth: true }
        Repeater {
            model: root.primaryActions
            delegate: IconButton {
                required property Action modelData
                action: modelData
                iconName: modelData.icon.name
                toolTip: modelData.text
                display: root.showLabels ? AbstractButton.TextBesideIcon : AbstractButton.IconOnly
            }
        }
        IconButton {
            id: moreButton
            objectName: "moreButton"
            visible: root.secondaryActions.length > 0
            iconName: "more"
            toolTip: qsTr("More options")
            onClicked: overflow.openAt(moreButton)
        }
    }

    FlyoutMenu {
        id: overflow
        objectName: "overflowMenu"
        Instantiator {
            model: root.secondaryActions
            delegate: MenuItem {
                required property Action modelData
                action: modelData
                icon.source: modelData.icon.name.length > 0 ? Theme.icon(modelData.icon.name) : ""
                icon.color: Theme.textPrimary
            }
            onObjectAdded: (index, object) => overflow.insertItem(index, object)
            onObjectRemoved: (index, object) => overflow.removeItem(object)
        }
    }
}
