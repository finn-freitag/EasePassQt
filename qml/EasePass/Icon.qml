import QtQuick
import QtQuick.Controls.impl as Impl

// Tinted monochrome icon. `iconName` refers to an SVG in icons/ (see Theme.icon()).
Impl.IconImage {
    id: root
    property string iconName
    property int size: Theme.iconSize

    source: iconName.length > 0 ? Theme.icon(iconName) : ""
    sourceSize: Qt.size(size, size)
    width: size
    height: size
    color: enabled ? Theme.textPrimary : Theme.textDisabled
    Accessible.ignored: true
}
