import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Component gallery: instantiates every control of the library. Used as the start page of
// easepass_qml until the real pages exist (T12+) and by the "gallery" QML test.
ScrollView {
    id: root
    property string lastCopied: ""
    contentWidth: availableWidth
    Accessible.name: qsTr("Component gallery")

    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.spacing

        Label {
            text: qsTr("Ease Pass components")
            font.pixelSize: Theme.fontTitle
            color: Theme.textPrimary
            Layout.margins: Theme.spacingL
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.spacingL
            Layout.rightMargin: Theme.spacingL
            spacing: Theme.spacing

            CommandBar {
                objectName: "commandBar"
                Layout.fillWidth: true
                primaryActions: [
                    Action { text: qsTr("Add"); icon.name: "add" },
                    Action { text: qsTr("Edit"); icon.name: "edit" },
                    Action { text: qsTr("Delete"); icon.name: "delete" }
                ]
                secondaryActions: [
                    Action { text: qsTr("Settings"); icon.name: "settings" }
                ]
            }

            InfoBar {
                objectName: "infoBar"
                severity: Theme.Severity.Success
                title: qsTr("Saved")
                message: qsTr("Database saved successfully.")
                Layout.fillWidth: true
            }

            SettingsItemSeparator { text: qsTr("Settings"); Layout.fillWidth: true }
            SettingsControl {
                objectName: "settingsControl"
                iconName: "key"
                header: qsTr("Password length")
                description: qsTr("Default length of generated passwords")
                Layout.fillWidth: true
                SpinBox { from: 8; to: 128; value: 15 }
            }
            SettingsControl {
                iconName: "settings"
                header: qsTr("Open settings")
                clickable: true
                Layout.fillWidth: true
            }

            SettingsItemSeparator { text: qsTr("Fields"); Layout.fillWidth: true }
            LabeledTextBox {
                objectName: "labeledTextBox"
                placeholderText: qsTr("Name")
                infoLabel: qsTr("required")
                Layout.fillWidth: true
            }
            CopyTextbox {
                objectName: "copyTextbox"
                text: "user@example.com"
                accessibleName: qsTr("Email")
                Layout.fillWidth: true
                onCopyRequested: (t) => root.lastCopied = t
            }
            CopyPasswordbox {
                objectName: "copyPasswordbox"
                password: "correct horse battery staple"
                safetyChecks: [true, false, true, null, false, false, true, true]
                Layout.fillWidth: true
                onCopyRequested: (t) => root.lastCopied = t
            }
            CopyTokenBox {
                objectName: "copyTokenBox"
                token: "123456"
                progress: 0.6
                Layout.fillWidth: true
                onCopyRequested: (t) => root.lastCopied = t
            }
            EditPasswordBox {
                objectName: "editPasswordBox"
                placeholderText: qsTr("Password")
                Layout.fillWidth: true
            }
            SearchPasswordsBox {
                objectName: "searchPasswordsBox"
                infoLabel: "3 / 12"
                Layout.fillWidth: true
            }
            TextWithSuggestionBox {
                objectName: "textWithSuggestionBox"
                placeholderText: qsTr("Username")
                suggestionItems: ["alice", "bob", "alice@example.com"]
                Layout.fillWidth: true
            }
            TagRenderControl {
                objectName: "tagRenderControl"
                tags: ["work", "email", "banking"]
                Layout.fillWidth: true
            }

            RowLayout {
                spacing: Theme.spacingL
                PasswordSafetyChart {
                    objectName: "passwordSafetyChart"
                    checks: [true, true, false, true, null, true, false, true]
                    Layout.preferredWidth: 100
                    Layout.preferredHeight: 100
                }
                IconButton {
                    objectName: "iconButton"
                    iconName: "refresh"
                    text: qsTr("Regenerate")
                }
                Button {
                    text: qsTr("Open dialog")
                    onClicked: dialog.open()
                }
                IconButton {
                    id: menuButton
                    iconName: "more"
                    toolTip: qsTr("Menu")
                    onClicked: flyout.openAt(menuButton)
                }
            }

            Splitter {
                objectName: "splitter"
                Layout.fillWidth: true
                Layout.preferredHeight: 120
                Rectangle {
                    color: Theme.card
                    SplitView.preferredWidth: 200
                    SplitView.minimumWidth: 100
                    Label { anchors.centerIn: parent; text: qsTr("List"); color: Theme.textPrimary }
                }
                Rectangle {
                    color: Theme.card
                    SplitView.fillWidth: true
                    Label { anchors.centerIn: parent; text: qsTr("Details"); color: Theme.textPrimary }
                }
            }
        }
    }

    FlyoutMenu {
        id: flyout
        objectName: "flyoutMenu"
        MenuItem { text: qsTr("Copy username") }
        MenuItem { text: qsTr("Copy password") }
    }

    ContentDialog {
        id: dialog
        objectName: "contentDialog"
        parent: Overlay.overlay
        title: qsTr("Delete item?")
        primaryButtonText: qsTr("Delete")
        closeButtonText: qsTr("Cancel")
        Label {
            text: qsTr("This cannot be undone.")
            color: Theme.textPrimary
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }
}
