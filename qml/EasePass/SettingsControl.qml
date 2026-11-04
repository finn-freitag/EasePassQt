import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// C#: Controls/SettingsControl.xaml(.cs) - settings card: icon, header, description and an
// arbitrary control on the right (default property). Clickable cards emit clicked().
Control {
    id: root
    property string iconName: ""
    property string header: ""
    property string description: ""
    property bool clickable: false
    default property alias content: contentHost.data
    signal clicked()

    implicitWidth: 400
    implicitHeight: Math.max(Theme.settingsCardHeight, layout.implicitHeight + 2 * Theme.spacingM)
    padding: Theme.spacingL
    topPadding: Theme.spacingM
    bottomPadding: Theme.spacingM
    focusPolicy: clickable ? Qt.StrongFocus : Qt.NoFocus
    hoverEnabled: clickable

    Accessible.role: clickable ? Accessible.Button : Accessible.Grouping
    Accessible.name: header
    Accessible.description: description
    Accessible.onPressAction: if (clickable) clicked()

    Keys.onPressed: (event) => {
        if (clickable && (event.key === Qt.Key_Return || event.key === Qt.Key_Enter
                          || event.key === Qt.Key_Space)) {
            root.clicked()
            event.accepted = true
        }
    }

    background: Rectangle {
        radius: Theme.radiusSmall
        color: tap.pressed && root.clickable ? Theme.cardPressed
             : root.hovered ? Theme.cardHover : Theme.card
        border.color: root.visualFocus ? Theme.accent : Theme.cardBorder
        border.width: root.visualFocus ? 2 : 1
        Behavior on color { ColorAnimation { duration: Theme.animationFast } }
    }

    TapHandler {
        id: tap
        enabled: root.clickable
        onTapped: root.clicked()
    }

    contentItem: RowLayout {
        id: layout
        spacing: Theme.spacingL

        Icon {
            iconName: root.iconName
            size: Theme.iconSizeLarge
            visible: root.iconName.length > 0
            Layout.alignment: Qt.AlignVCenter
        }
        ColumnLayout {
            spacing: 0
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            Label {
                text: root.header
                color: Theme.textPrimary
                font.pixelSize: Theme.fontBody
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Label {
                text: root.description
                visible: text.length > 0
                color: Theme.textSecondary
                font.pixelSize: Theme.fontCaption
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }
        Item {
            id: contentHost
            Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            implicitWidth: childrenRect.width
            implicitHeight: childrenRect.height
        }
        Icon {
            iconName: "chevron-right"
            visible: root.clickable
            color: Theme.textSecondary
            Layout.alignment: Qt.AlignVCenter
        }
    }
}
