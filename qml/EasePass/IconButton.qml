import QtQuick
import QtQuick.Controls

// C#: Controls/IconButton.xaml(.cs) - button with a glyph and optional text.
ToolButton {
    id: root
    property string iconName
    property string toolTip: ""

    icon.source: iconName.length > 0 ? Theme.icon(iconName) : ""
    icon.width: Theme.iconSize
    icon.height: Theme.iconSize
    icon.color: enabled ? Theme.textPrimary : Theme.textDisabled
    display: text.length > 0 ? AbstractButton.TextBesideIcon : AbstractButton.IconOnly
    focusPolicy: Qt.StrongFocus

    ToolTip.visible: toolTip.length > 0 && hovered
    ToolTip.text: toolTip
    ToolTip.delay: 500

    Accessible.role: Accessible.Button
    Accessible.name: text.length > 0 ? text : toolTip
    Accessible.onPressAction: clicked()
}
