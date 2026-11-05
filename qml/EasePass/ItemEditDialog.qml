import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// C#: Dialogs/AddItemDialog + EditItemDialog with Views/DialogPages/AddItemPage. Works on its
// own ItemViewModel buffer (AppContext::editorViewModel), so cancelling discards everything
// and the detail pane keeps showing the stored item.
ContentDialog {
    id: root
    required property var editor          // App::ItemViewModel (edit buffer)
    property var generator: null          // App::PasswordGeneratorViewModel
    property bool _awaitingPassword: false
    property bool _show2FA: false
    property bool _confirmRemove2FA: false
    property string qrCode: ""
    readonly property bool editMode: editor ? editor.hasItem && !editor.isNew : false
    readonly property bool generating: _awaitingPassword && generator !== null && generator.busy
    signal committed(int index)
    signal copyRequested(string text)

    function openNew() {
        editor.newItem()
        _prepare()
        open()
    }
    function openEdit(index) {
        if (!editor.load(index)) return false
        _prepare()
        open()
        return true
    }
    function _prepare() {
        _awaitingPassword = false
        _confirmRemove2FA = false
        qrCode = ""
        errorBar.open = false
        _show2FA = editor.has2FA   // C#: the 2FA section is hidden for items without 2FA
        nameBox.text = editor.displayName
        usernameBox.text = editor.username
        emailBox.setTextProgrammatically(editor.email)
        passwordBox.text = editor.password
        websiteBox.text = editor.website
        notesBox.text = editor.notes
        tagsBox.text = editor.tagsText
        twoFactor.sync()
    }
    function generatePassword() {
        if (!generator || generating) return
        _awaitingPassword = true
        generator.generate()
    }
    function save() {
        if (editor.commit()) {
            const index = editor.index
            close()
            committed(index)
        } else {
            errorBar.message = editor.validationErrors.join("\n")
            errorBar.open = true
        }
    }

    title: editMode ? qsTr("Edit item") : qsTr("Add Password")
    primaryButtonText: editMode ? qsTr("Done") : qsTr("Add")
    closeButtonText: qsTr("Cancel")
    closeOnPrimary: false
    onPrimaryClicked: save()
    onOpened: nameBox.forceActiveFocus()
    onClosed: {
        _awaitingPassword = false
        qrCode = ""
        editor.unload()
    }

    Connections {
        target: root.generator
        enabled: root._awaitingPassword
        function onGenerated(password) {
            root._awaitingPassword = false
            passwordBox.text = password
            root.editor.password = password
        }
    }

    InfoBar {
        id: errorBar
        objectName: "errorBar"
        managed: true
        open: false
        severity: Theme.Severity.Error
        title: qsTr("The item could not be saved")
        Layout.fillWidth: true
        onClosed: open = false
    }

    ScrollView {
        id: scroll
        objectName: "itemScroll"
        clip: true
        contentWidth: availableWidth
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(fields.implicitHeight,
                                         root.parent ? root.parent.height * 0.6 : 480)

        ColumnLayout {
            id: fields
            width: scroll.availableWidth
            spacing: Theme.spacingS

            Label { text: qsTr("Display Name"); color: Theme.textPrimary }
            TextField {
                id: nameBox
                objectName: "nameBox"
                Accessible.name: qsTr("Display Name")
                Layout.fillWidth: true
                onTextEdited: root.editor.displayName = text
            }
            Label { text: qsTr("Username"); color: Theme.textPrimary }
            TextField {
                id: usernameBox
                objectName: "usernameBox"
                Accessible.name: qsTr("Username")
                Layout.fillWidth: true
                onTextEdited: root.editor.username = text
            }
            Label { text: qsTr("Email"); color: Theme.textPrimary }
            TextWithSuggestionBox {
                id: emailBox
                objectName: "emailBox"
                accessibleName: qsTr("Email")
                suggestionItems: root.editor ? root.editor.emailSuggestions : []
                Layout.fillWidth: true
                onTextChanged: if (root.editor && root.editor.hasItem) root.editor.email = text
            }
            Label { text: qsTr("Password"); color: Theme.textPrimary }
            RowLayout {
                spacing: Theme.spacing
                Layout.fillWidth: true
                EditPasswordBox {
                    id: passwordBox
                    objectName: "passwordBox"
                    placeholderText: qsTr("Password")
                    enabled: !root.generating
                    Layout.fillWidth: true
                    onTextEdited: root.editor.password = text
                    onCopyRequested: (t) => root.copyRequested(t)
                    // C#: red border while the password is used by another item.
                    Rectangle {
                        objectName: "reuseBorder"
                        anchors.fill: parent
                        color: "transparent"
                        radius: Theme.radiusSmall
                        border.width: 2
                        border.color: Theme.errorForeground
                        visible: root.editor ? root.editor.passwordReused : false
                    }
                }
                IconButton {
                    objectName: "generatePasswordButton"
                    iconName: "generate"
                    toolTip: qsTr("Generate a password")
                    enabled: root.generator !== null && !root.generating
                    onClicked: root.generatePassword()
                }
            }
            ProgressBar {
                objectName: "generateProgress"
                indeterminate: true
                visible: root.generating
                Layout.fillWidth: true
            }
            Label {
                objectName: "reuseWarning"
                text: qsTr("This password is already used by another item.")
                visible: root.editor ? root.editor.passwordReused : false
                color: Theme.errorForeground
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            PasswordSafetyChart {
                objectName: "itemSafetyChart"
                checks: root.editor ? root.editor.safetyChecks : []
                visible: passwordBox.text.length > 0
                Layout.alignment: Qt.AlignHCenter
            }

            Label { text: qsTr("Website"); color: Theme.textPrimary }
            TextField {
                id: websiteBox
                objectName: "websiteBox"
                Accessible.name: qsTr("Website")
                Layout.fillWidth: true
                onTextEdited: root.editor.website = text
            }

            // --- Two-factor authentication (only for items that have it, like C#) -------
            Label {
                text: qsTr("Two Factor Authentication")
                visible: root._show2FA
                color: Theme.textPrimary
                font.weight: Font.DemiBold
                Layout.topMargin: Theme.spacing
            }
            TwoFactorFields {
                id: twoFactor
                objectName: "twoFactorFields"
                editor: root.editor
                visible: root._show2FA
                Layout.fillWidth: true
                onCopyRequested: (t) => root.copyRequested(t)
            }
            Button {
                objectName: "remove2FAButton"
                text: qsTr("Remove 2FA")
                visible: root._show2FA && !root._confirmRemove2FA
                Layout.fillWidth: true
                onClicked: root._confirmRemove2FA = true
            }
            ColumnLayout {
                visible: root._show2FA && root._confirmRemove2FA
                Layout.fillWidth: true
                Label {
                    text: qsTr("Are you sure you want to remove two-factor authentication?")
                    wrapMode: Text.WordWrap
                    color: Theme.textPrimary
                    Layout.fillWidth: true
                }
                RowLayout {
                    Button {
                        objectName: "confirmRemove2FAButton"
                        text: qsTr("Remove")
                        highlighted: true
                        onClicked: {
                            root.editor.remove2FA()
                            root._confirmRemove2FA = false
                            root._show2FA = false
                            root.qrCode = ""
                        }
                    }
                    Button {
                        text: qsTr("Cancel")
                        onClicked: root._confirmRemove2FA = false
                    }
                }
            }
            Button {
                objectName: "export2FAButton"
                text: qsTr("Export 2FA")
                visible: root._show2FA
                enabled: root.editor ? root.editor.has2FA : false
                Layout.fillWidth: true
                onClicked: root.qrCode = root.editor.otpauthQrCode()
            }
            Image {
                objectName: "qrCodeImage"
                source: root.qrCode
                visible: root._show2FA && root.qrCode.length > 0
                cache: false
                smooth: false
                fillMode: Image.PreserveAspectFit
                Layout.preferredWidth: 220
                Layout.preferredHeight: 220
                Layout.alignment: Qt.AlignHCenter
            }

            Label { text: qsTr("Notes"); color: Theme.textPrimary; Layout.topMargin: Theme.spacing }
            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: 150
                TextArea {
                    id: notesBox
                    objectName: "notesBox"
                    Accessible.name: qsTr("Notes")
                    wrapMode: TextEdit.Wrap
                    onTextChanged: if (root.editor && root.editor.hasItem && activeFocus) root.editor.notes = text
                }
            }
            Label { text: qsTr("Tags (Separated by Space)"); color: Theme.textPrimary }
            TextField {
                id: tagsBox
                objectName: "tagsBox"
                Accessible.name: qsTr("Tags")
                Layout.fillWidth: true
                onTextEdited: root.editor.tagsText = text
            }
        }
    }
}
