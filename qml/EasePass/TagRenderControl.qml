import QtQuick
import QtQuick.Controls

// C#: Controls/TagRenderControl.xaml.cs - row of tag chips. Border colour is derived from
// MD5(tag) bytes [1],[0],[2] exactly like the C# app so tags keep their colour.
Flow {
    id: root
    property var tags: []
    spacing: Theme.spacingS + Theme.spacingXS

    Accessible.role: Accessible.List
    Accessible.name: qsTr("Tags")

    function tagColor(tag) {
        const h = Qt.md5(tag)
        const b = (i) => parseInt(h.substr(i * 2, 2), 16) / 255
        return Qt.rgba(b(1), b(0), b(2), 1)
    }

    Repeater {
        model: root.tags ? root.tags : []
        delegate: Rectangle {
            required property string modelData
            radius: 5
            border.width: 1.5
            border.color: root.tagColor(modelData)
            color: "transparent"
            implicitWidth: tagText.implicitWidth + 2 * Theme.spacingS
            implicitHeight: tagText.implicitHeight + Theme.spacingXS
            width: implicitWidth
            height: implicitHeight
            Accessible.role: Accessible.ListItem
            Accessible.name: modelData
            Label {
                id: tagText
                anchors.centerIn: parent
                text: parent.modelData
                color: Theme.textPrimary
                font.pixelSize: Theme.fontCaption
            }
        }
    }
}
