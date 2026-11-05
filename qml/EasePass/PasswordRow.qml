import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One row of the password list (C#: PasswordsPage ListView item template): website icon or
// letter avatar, name, username, tag chips and a 2FA badge. Roles come from PasswordListModel.
ItemDelegate {
    id: root
    required property int index
    required property int sourceIndex
    required property string displayName
    required property string username
    required property string email
    required property bool hasPassword
    required property string website
    required property var tags
    required property bool has2FA
    required property string avatarLetter
    required property color avatarColor
    required property color avatarTextColor
    property var icons: null
    signal contextMenuRequested(int sourceIndex, real x, real y)

    readonly property string iconSource: icons && icons.enabled
                                         ? (icons.revision, icons.iconUrl(website)) : ""

    width: ListView.view ? ListView.view.width : implicitWidth
    highlighted: ListView.isCurrentItem
    padding: Theme.spacing
    Accessible.name: displayName + (username.length > 0 ? ", " + username : "")

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: (eventPoint) => root.contextMenuRequested(root.sourceIndex, eventPoint.position.x, eventPoint.position.y)
    }
    onPressAndHold: contextMenuRequested(sourceIndex, width / 2, height / 2)

    contentItem: RowLayout {
        spacing: Theme.spacingM
        Item {
            Layout.preferredWidth: 32
            Layout.preferredHeight: 32
            Rectangle {
                anchors.fill: parent
                radius: width / 2
                visible: root.iconSource.length === 0
                color: root.avatarColor
                Label {
                    anchors.centerIn: parent
                    text: root.avatarLetter
                    color: root.avatarTextColor
                    font.pixelSize: Theme.fontBody
                }
            }
            Image {
                anchors.fill: parent
                visible: root.iconSource.length > 0
                source: root.iconSource
                sourceSize: Qt.size(32, 32)
                fillMode: Image.PreserveAspectFit
                asynchronous: true
            }
        }
        ColumnLayout {
            spacing: 0
            Layout.fillWidth: true
            Label {
                text: root.displayName
                font.pixelSize: Theme.fontBody + 2
                color: Theme.textPrimary
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Label {
                text: root.username
                visible: text.length > 0
                font.pixelSize: Theme.fontCaption
                color: Theme.textSecondary
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            TagRenderControl {
                tags: root.tags
                visible: root.tags && root.tags.length > 0
                Layout.fillWidth: true
            }
        }
        Label {
            objectName: "twoFactorBadge"
            visible: root.has2FA
            text: "2FA"
            font.pixelSize: Theme.fontCaption
            color: Theme.accent
            Accessible.name: qsTr("Two-factor authentication")
        }
    }
}
