import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Stand-in for pages that are not ported yet (Passwords: T14, Database: T15, ...).
Item {
    id: root
    property string title: ""
    property bool showBack: false
    property bool showManageDatabase: false
    signal lockRequested()
    signal manageDatabaseRequested()
    signal backRequested()

    ColumnLayout {
        anchors.centerIn: parent
        spacing: Theme.spacingM
        Label {
            text: root.title
            font.pixelSize: Theme.fontTitle
            color: Theme.textPrimary
            Layout.alignment: Qt.AlignHCenter
        }
        Label {
            text: qsTr("This page is not available yet.")
            color: Theme.textSecondary
            Layout.alignment: Qt.AlignHCenter
        }
        Button {
            text: qsTr("Back")
            visible: root.showBack
            Layout.alignment: Qt.AlignHCenter
            onClicked: root.backRequested()
        }
        Button {
            objectName: "manageDatabaseButton"
            text: qsTr("Manage database")
            visible: root.showManageDatabase
            Layout.alignment: Qt.AlignHCenter
            onClicked: root.manageDatabaseRequested()
        }
        Button {
            text: qsTr("Lock")
            icon.source: Theme.icon("lock")
            icon.color: Theme.textPrimary
            Layout.alignment: Qt.AlignHCenter
            onClicked: root.lockRequested()
        }
    }
}
