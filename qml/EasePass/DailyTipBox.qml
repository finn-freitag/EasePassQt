import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// C#: LoginPage dailyTipGrid - shown only when a tip is available.
Control {
    id: root
    property string tip: ""

    visible: tip.length > 0
    padding: Theme.spacingM
    Accessible.role: Accessible.StaticText
    Accessible.name: qsTr("Daily tip") + ": " + tip

    background: Rectangle {
        radius: Theme.radius
        color: Theme.card
        border.color: Theme.cardBorder
    }
    contentItem: RowLayout {
        spacing: Theme.spacingM
        Icon {
            iconName: "lightbulb"
            color: Theme.accent
            size: Theme.iconSizeLarge
            Layout.alignment: Qt.AlignTop
        }
        ColumnLayout {
            spacing: Theme.spacingXS
            Layout.fillWidth: true
            Label {
                text: qsTr("Daily tip")
                font.weight: Font.DemiBold
                color: Theme.textPrimary
            }
            Label {
                objectName: "tipText"
                text: root.tip
                wrapMode: Text.WordWrap
                color: Theme.textSecondary
                Layout.fillWidth: true
            }
        }
    }
}
