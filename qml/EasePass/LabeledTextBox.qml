import QtQuick
import QtQuick.Controls

// C#: Controls/LabeledTextBox.xaml(.cs) - text box with a small info label on the right
// inside the field (e.g. "3 found").
TextField {
    id: root
    property string infoLabel: ""
    property string accessibleName: placeholderText

    rightPadding: (infoLabel.length > 0 ? info.implicitWidth + Theme.spacing : 0) + Theme.spacingM
    selectByMouse: true
    Accessible.name: accessibleName

    Label {
        id: info
        text: root.infoLabel
        visible: text.length > 0
        color: Theme.textSecondary
        font.pixelSize: Theme.fontCaption
        anchors.right: parent.right
        anchors.rightMargin: Theme.spacingM
        anchors.verticalCenter: parent.verticalCenter
    }
}
