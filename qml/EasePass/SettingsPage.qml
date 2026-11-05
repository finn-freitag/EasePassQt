import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// C#: Views/SettingsPage - language, database, security, password generator and
// miscellaneous settings. Every control reads and writes AppSettings directly (through
// SettingsViewModel `viewModel`); AppSettings clamps the values. Plugin sections follow in T26.
Item {
    id: root
    required property var viewModel
    readonly property var settings: viewModel ? viewModel.settings : null
    property bool databaseOpen: false
    property bool showBack: true

    signal backRequested()
    signal manageDatabaseRequested()

    // Re-establishes a binding after the user changed a spin box (SpinBox replaces it).
    function rebind(box, getter) { box.value = Qt.binding(getter) }

    Rectangle {
        anchors.fill: parent
        color: Theme.background
    }

    ScrollView {
        id: scroll
        objectName: "settingsScroll"
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            id: column
            width: Math.min(scroll.availableWidth - 2 * Theme.spacingL, 1000)
            x: (scroll.availableWidth - width) / 2
            spacing: Theme.spacingXS

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Theme.spacingL
                Layout.bottomMargin: Theme.spacingXL
                IconButton {
                    objectName: "backButton"
                    iconName: "chevron-right"
                    rotation: 180
                    toolTip: qsTr("Back")
                    visible: root.showBack
                    onClicked: root.backRequested()
                }
                Label {
                    text: qsTr("Settings")
                    font.pixelSize: Theme.fontTitle
                    color: Theme.textPrimary
                    horizontalAlignment: Text.AlignHCenter
                    Layout.fillWidth: true
                }
                Item { implicitWidth: Theme.controlHeight }
            }

            // --- Language --------------------------------------------------------------
            SettingsControl {
                iconName: "info"
                header: qsTr("Language")
                description: root.viewModel.restartRequired ? qsTr("To apply please restart the app") : ""
                Layout.fillWidth: true
                ComboBox {
                    id: languageBox
                    objectName: "languageBox"
                    width: 200
                    model: root.viewModel.languages
                    textRole: "name"
                    valueRole: "tag"
                    currentIndex: root.viewModel.languageIndex
                    Accessible.name: qsTr("Language")
                    onActivated: (index) => root.viewModel.languageIndex = index
                }
            }

            // --- Database --------------------------------------------------------------
            SettingsControl {
                iconName: "folder"
                header: qsTr("Database")
                description: root.databaseOpen ? "" : qsTr("Log in to manage your databases")
                Layout.fillWidth: true
                Button {
                    objectName: "manageDatabaseButton"
                    text: qsTr("Manage")
                    highlighted: true
                    width: 100
                    enabled: root.databaseOpen
                    onClicked: root.manageDatabaseRequested()
                }
            }
            SettingsControl {
                iconName: "refresh"
                header: qsTr("Auto-save delay")
                description: qsTr("Seconds after a change until the database is saved")
                Layout.fillWidth: true
                SpinBox {
                    id: saveDelayBox
                    objectName: "saveDelayBox"
                    from: root.viewModel.minSaveDelaySeconds
                    to: root.viewModel.maxSaveDelaySeconds
                    editable: true
                    value: root.settings.databaseDeferredSaveSeconds
                    Accessible.name: qsTr("Auto-save delay in seconds")
                    onValueModified: {
                        root.settings.databaseDeferredSaveSeconds = value
                        root.rebind(saveDelayBox, () => root.settings.databaseDeferredSaveSeconds)
                    }
                }
            }

            // --- Security --------------------------------------------------------------
            SettingsItemSeparator { text: qsTr("Security"); Layout.fillWidth: true }
            SettingsControl {
                iconName: "lock"
                header: qsTr("Inactivity logout")
                description: qsTr("Minutes")
                Layout.fillWidth: true
                SpinBox {
                    id: inactivityBox
                    objectName: "inactivityBox"
                    from: root.viewModel.minInactivityMinutes
                    to: root.viewModel.maxInactivityMinutes
                    editable: true
                    value: root.settings.inactivityLogoutMinutes
                    Accessible.name: qsTr("Inactivity logout in minutes")
                    onValueModified: {
                        root.settings.inactivityLogoutMinutes = value
                        root.rebind(inactivityBox, () => root.settings.inactivityLogoutMinutes)
                    }
                }
            }
            SettingsControl {
                iconName: "copy"
                header: qsTr("Clipboard clear timeout")
                description: qsTr("Seconds")
                Layout.fillWidth: true
                SpinBox {
                    id: clipboardBox
                    objectName: "clipboardBox"
                    from: root.viewModel.minClipboardSeconds
                    to: root.viewModel.maxClipboardSeconds
                    editable: true
                    value: root.settings.clipboardClearSeconds
                    Accessible.name: qsTr("Clipboard clear timeout in seconds")
                    onValueModified: {
                        root.settings.clipboardClearSeconds = value
                        root.rebind(clipboardBox, () => root.settings.clipboardClearSeconds)
                    }
                }
            }

            // --- Password generator (C#: Expander) -----------------------------------------
            SettingsItemSeparator { text: qsTr("Password generator"); Layout.fillWidth: true }
            Pane {
                objectName: "generatorSection"
                padding: Theme.spacingL
                Layout.fillWidth: true
                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: Theme.card
                    border.color: Theme.cardBorder
                }
                ColumnLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    spacing: Theme.spacing

                    Label {
                        text: qsTr("Possible characters in password")
                        color: Theme.textPrimary
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        TextField {
                            id: charsField
                            objectName: "passwordCharsField"
                            // Not bound: clearing the field stores the defaults (C#) but must
                            // leave the field empty while the user types.
                            Component.onCompleted: text = root.settings.passwordChars
                            placeholderText: qsTr("Some letters, digits and punctuation...")
                            Accessible.name: qsTr("Possible characters in password")
                            Layout.fillWidth: true
                            onTextEdited: root.viewModel.applyPasswordCharsText(text)
                        }
                        Button {
                            objectName: "resetCharsButton"
                            text: qsTr("Reset")
                            highlighted: true
                            onClicked: {
                                root.viewModel.resetPasswordChars()
                                charsField.text = root.settings.passwordChars
                            }
                        }
                    }
                    Label {
                        objectName: "charsWarning"
                        visible: !root.viewModel.passwordCharsValid
                        text: qsTr("Use at least two different characters.")
                        color: Theme.warningForeground
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    Label {
                        text: qsTr("Password length (min: %1)").arg(root.viewModel.minPasswordLength)
                        color: Theme.textPrimary
                        Layout.topMargin: Theme.spacing
                    }
                    TextField {
                        id: lengthField
                        objectName: "passwordLengthField"
                        Component.onCompleted: text = root.settings.passwordLength
                        placeholderText: qsTr("Length of the password...")
                        inputMethodHints: Qt.ImhDigitsOnly
                        Accessible.name: qsTr("Password length")
                        Layout.fillWidth: true
                        onTextEdited: {
                            const digits = root.viewModel.applyPasswordLengthText(text)
                            if (digits !== text) text = digits
                        }
                        // Shows the stored (clamped) value once the field loses focus.
                        onActiveFocusChanged: if (!activeFocus) text = root.settings.passwordLength
                    }
                    Switch {
                        objectName: "verifyLeakedSwitch"
                        text: qsTr("Verify that the password was not leaked")
                        checked: root.settings.verifyNotLeaked
                        Layout.topMargin: Theme.spacing
                        onToggled: root.settings.verifyNotLeaked = checked
                    }
                }
            }

            // --- Miscellaneous ----------------------------------------------------------
            SettingsItemSeparator { text: qsTr("Miscellaneous"); Layout.fillWidth: true }
            SettingsControl {
                iconName: "copy"
                header: qsTr("Quick copy on double click")
                description: qsTr("Copy text by double clicking on the text box")
                Layout.fillWidth: true
                Switch {
                    objectName: "doubleClickSwitch"
                    checked: root.settings.doubleClickCopy
                    Accessible.name: qsTr("Quick copy on double click")
                    onToggled: root.settings.doubleClickCopy = checked
                }
            }
            SettingsControl {
                iconName: "eye"
                header: qsTr("Show icons")
                description: qsTr("Show or hide the icons in the password list")
                Layout.fillWidth: true
                Switch {
                    objectName: "showIconsSwitch"
                    checked: root.settings.showIcons
                    Accessible.name: qsTr("Show icons")
                    onToggled: root.settings.showIcons = checked
                }
            }
            SettingsControl {
                iconName: "search"
                header: qsTr("Download website icons")
                description: qsTr("Fetches the favicon of each website. The website learns that you use it.")
                enabled: root.settings.showIcons
                Layout.fillWidth: true
                Switch {
                    objectName: "fetchFaviconsSwitch"
                    checked: root.settings.fetchFavicons
                    Accessible.name: qsTr("Download website icons")
                    onToggled: root.settings.fetchFavicons = checked
                }
            }
            SettingsControl {
                iconName: "sort"
                header: qsTr("Password popularity counter")
                description: qsTr("Resets the counter that counts the clicks to sort by popularity.")
                Layout.fillWidth: true
                Button {
                    objectName: "resetPopularityButton"
                    text: qsTr("Reset")
                    highlighted: true
                    width: 100
                    enabled: root.viewModel.canResetPopularity
                    onClicked: root.viewModel.resetPopularity()
                }
            }
            Item { implicitHeight: Theme.spacingXL }
        }
    }

    InfoBar {
        id: pageInfo
        objectName: "pageInfo"
        open: false
        autoCloseInterval: 6000
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Theme.spacingL
        z: 5
    }
    // External changes (resets) reach the text fields unless the user is editing them.
    Connections {
        target: root.settings
        function onPasswordCharsChanged() {
            if (!charsField.activeFocus) charsField.text = root.settings.passwordChars
        }
        function onPasswordLengthChanged() {
            if (!lengthField.activeFocus) lengthField.text = root.settings.passwordLength
        }
    }
    Connections {
        target: root.viewModel
        function onMessage(severity, text) {
            pageInfo.severity = severity
            pageInfo.message = text
            pageInfo.show()
        }
    }
}
