import QtQuick
import QtQuick.Controls

// C#: Controls/SettingsItemSeparator.xaml - section header between settings cards.
Item {
    id: root
    property string text: ""

    implicitWidth: 200
    implicitHeight: text.length > 0 ? label.implicitHeight + Theme.spacingL + Theme.spacing
                                    : Theme.spacingL
    Accessible.role: text.length > 0 ? Accessible.Heading : Accessible.Separator
    Accessible.name: text

    Label {
        id: label
        visible: root.text.length > 0
        text: root.text
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.spacing
        font.pixelSize: Theme.fontBody
        font.weight: Font.DemiBold
        color: Theme.textPrimary
    }
}
