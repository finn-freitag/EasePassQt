import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Settings > Plugins > Import passwords (steps.txt T28; C#: ImportPasswordsDialog with
// IPasswordImporter plugins): choose a source of an importer plugin, then select which of
// the passwords it returned go into the open database. `plugins` is a PluginsViewModel.
Item {
    id: root
    required property var plugins

    function open() {
        plugins.loadImportSources()
        sourceDialog.open()
    }

    ContentDialog {
        id: sourceDialog
        objectName: "pluginImportSourceDialog"
        parent: Overlay.overlay
        title: qsTr("Import passwords")
        closeButtonText: qsTr("Close")

        Label {
            objectName: "noImportSources"
            visible: root.plugins.importSources.length === 0 && !root.plugins.busy
            text: qsTr("No plugin offers passwords to import.")
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        BusyIndicator {
            running: root.plugins.busy
            visible: running
            Layout.alignment: Qt.AlignHCenter
        }
        Repeater {
            objectName: "importSourceRepeater"
            model: root.plugins.importSources
            SettingsControl {
                id: source
                required property var modelData
                objectName: "importSource"
                header: modelData.name
                description: modelData.description.length > 0 ? modelData.pluginName + " - " + modelData.description
                                                              : modelData.pluginName
                Layout.fillWidth: true
                Button {
                    objectName: "importSourceButton"
                    text: qsTr("Import")
                    highlighted: true
                    enabled: !root.plugins.busy
                    onClicked: root.plugins.importFrom(source.modelData.pluginId, source.modelData.sourceId)
                }
            }
        }
    }

    SelectItemsDialog {
        id: selectDialog
        objectName: "pluginImportSelectDialog"
        parent: Overlay.overlay
        title: qsTr("Select the passwords to import")
        primaryButtonText: qsTr("Import")
        allowOverride: true
        names: root.plugins.importCandidates
        onSelectionAccepted: (indexes, overrideExisting) => root.plugins.importSelected(indexes, overrideExisting)
        onCloseClicked: root.plugins.cancelImport()
    }
    Connections {
        target: root.plugins
        function onImportReady() {
            sourceDialog.close()
            selectDialog.open()
        }
        function onImportCandidatesChanged() {
            // The database was locked meanwhile: nothing left to select.
            if (root.plugins.importCandidates.length === 0 && selectDialog.visible)
                selectDialog.close()
        }
    }
}
