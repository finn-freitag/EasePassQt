import QtQuick
import QtQuick.Controls

// C#: Controls/SearchPasswordsBox.xaml(.cs) - search field with search icon, info label
// ("x of y") and suggestions. A query starting with '#' is a tag search; keyPressed reports
// that flag so the view model can complete tags.
SuggestionBox {
    id: root
    property string infoLabel: ""
    readonly property bool isTagSearch: text.startsWith("#")
    signal searchKeyPressed(bool isTagSearch, var event)

    placeholderText: qsTr("Search passwords")
    leftPadding: searchIcon.width + Theme.spacingM + Theme.spacingS
    rightPadding: (infoLabel.length > 0 ? info.implicitWidth + Theme.spacing : 0) + Theme.spacingM
    onKeyPressed: (event) => root.searchKeyPressed(root.isTagSearch, event)

    Icon {
        id: searchIcon
        iconName: "search"
        color: Theme.textSecondary
        anchors.left: parent.left
        anchors.leftMargin: Theme.spacingM
        anchors.verticalCenter: parent.verticalCenter
    }
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
