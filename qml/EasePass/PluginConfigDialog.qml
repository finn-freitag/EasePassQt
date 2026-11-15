import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Settings > Plugins > Remote database settings (steps.txt T28): the settings of a
// database-source plugin as a form. Plugins have no UI of their own; they describe their
// settings (JSON Schema) and Ease Pass draws the fields. `plugins` is a PluginsViewModel.
Item {
    id: root
    required property var plugins
    // key -> value of the fields the user changed
    property var edited: ({})

    function open() {
        plugins.loadProviders()
        providerDialog.open()
    }
    function setValue(key, value) {
        const copy = Object.assign({}, edited)
        copy[key] = value
        edited = copy
    }

    ContentDialog {
        id: providerDialog
        objectName: "pluginProviderDialog"
        parent: Overlay.overlay
        title: qsTr("Remote database settings")
        closeButtonText: qsTr("Close")

        Label {
            objectName: "noProviders"
            visible: root.plugins.providers.length === 0 && !root.plugins.busy
            text: qsTr("No plugin for remote databases is installed.")
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        Repeater {
            objectName: "providerRepeater"
            model: root.plugins.providers
            SettingsControl {
                id: provider
                required property var modelData
                objectName: "provider"
                iconName: "folder"
                header: modelData.name
                Layout.fillWidth: true
                Button {
                    objectName: "providerSettingsButton"
                    text: qsTr("Settings")
                    highlighted: true
                    enabled: !root.plugins.busy
                    onClicked: root.plugins.openProviderConfig(provider.modelData.pluginId)
                }
            }
        }
    }

    ContentDialog {
        id: configDialog
        objectName: "pluginConfigDialog"
        parent: Overlay.overlay
        title: qsTr("Settings")
        primaryButtonText: qsTr("Save")
        closeButtonText: qsTr("Cancel")
        primaryButtonEnabled: !root.plugins.busy
        closeOnPrimary: false
        onPrimaryClicked: root.plugins.saveProviderConfig(root.edited)
        onClosed: root.plugins.closeProviderConfig()

        Repeater {
            objectName: "configRepeater"
            model: root.plugins.configFields
            ColumnLayout {
                id: field
                required property var modelData
                objectName: "configField"
                spacing: Theme.spacingXS
                Layout.fillWidth: true

                Label {
                    visible: field.modelData.kind !== "boolean"
                    text: field.modelData.title + (field.modelData.required ? " *" : "")
                    color: Theme.textPrimary
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                Loader {
                    Layout.fillWidth: true
                    sourceComponent: field.modelData.kind === "boolean" ? switchField
                                   : field.modelData.kind === "choice" ? choiceField
                                   : field.modelData.kind === "password" ? passwordField : textField
                }
                Label {
                    visible: field.modelData.description.length > 0
                    text: field.modelData.description
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontCaption
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                Component {
                    id: textField
                    TextField {
                        objectName: "configText"
                        text: field.modelData.value
                        maximumLength: field.modelData.maxLength
                        inputMethodHints: field.modelData.kind === "integer" ? Qt.ImhDigitsOnly
                                        : field.modelData.kind === "number" ? Qt.ImhFormattedNumbersOnly : Qt.ImhNone
                        Accessible.name: field.modelData.title
                        onTextEdited: root.setValue(field.modelData.key, text)
                    }
                }
                Component {
                    id: passwordField
                    EditPasswordBox {
                        objectName: "configPassword"
                        text: field.modelData.value
                        Accessible.name: field.modelData.title
                        onTextChanged: if (text !== field.modelData.value) root.setValue(field.modelData.key, text)
                    }
                }
                Component {
                    id: switchField
                    Switch {
                        objectName: "configSwitch"
                        text: field.modelData.title
                        checked: field.modelData.value === true
                        onToggled: root.setValue(field.modelData.key, checked)
                    }
                }
                Component {
                    id: choiceField
                    ComboBox {
                        objectName: "configChoice"
                        model: field.modelData.options
                        currentIndex: Math.max(0, field.modelData.options.indexOf(field.modelData.value))
                        Accessible.name: field.modelData.title
                        onActivated: root.setValue(field.modelData.key, currentText)
                    }
                }
            }
        }
    }
    Connections {
        target: root.plugins
        function onConfigReady() {
            root.edited = ({})
            providerDialog.close()
            configDialog.open()
        }
        function onConfigSaved(ok) { if (ok) configDialog.close() }
    }
}
