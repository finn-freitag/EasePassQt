import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Plugin manager (steps.txt T28; C#: Views/ExtensionPage): installed plugins with icon,
// name, version, author, computed permissions, enable switch, website and uninstall; below
// the plugin store (catalog) with install/update. `plugins` is a PluginsViewModel.
Item {
    id: root
    required property var plugins
    property bool showBack: true

    signal backRequested()
    signal installFromFileRequested()

    // Values of PluginsViewModel::CatalogStatus
    readonly property int catalogLoading: 1
    readonly property int catalogLive: 2
    readonly property int catalogCached: 3
    readonly property int catalogUnavailable: 4

    Component.onCompleted: plugins.refreshStore()

    Rectangle {
        anchors.fill: parent
        color: Theme.background
    }

    component Card: Pane {
        padding: Theme.spacingL
        Layout.fillWidth: true
        background: Rectangle {
            radius: Theme.radiusSmall
            color: Theme.card
            border.color: Theme.cardBorder
        }
    }

    ScrollView {
        id: scroll
        objectName: "pluginsScroll"
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
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
                    text: qsTr("Plugins")
                    font.pixelSize: Theme.fontTitle
                    color: Theme.textPrimary
                    horizontalAlignment: Text.AlignHCenter
                    Layout.fillWidth: true
                }
                Item { implicitWidth: Theme.controlHeight }
            }

            // --- Installed ------------------------------------------------------------
            RowLayout {
                Layout.fillWidth: true
                SettingsItemSeparator { text: qsTr("Installed"); Layout.fillWidth: true }
                Button {
                    objectName: "installFromFileButton"
                    text: qsTr("Install from file...")
                    enabled: !root.plugins.busy
                    onClicked: root.installFromFileRequested()
                }
            }
            Label {
                objectName: "noPluginsLabel"
                visible: root.plugins.installed.length === 0
                text: qsTr("No plugins installed.")
                color: Theme.textSecondary
                Layout.fillWidth: true
            }
            Repeater {
                objectName: "installedRepeater"
                model: root.plugins.installed
                Card {
                    id: card
                    required property var modelData
                    objectName: "installedCard"
                    ColumnLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        spacing: Theme.spacingS
                        RowLayout {
                            spacing: Theme.spacingM
                            Layout.fillWidth: true
                            Image {
                                objectName: "pluginIcon"
                                source: card.modelData.icon
                                visible: card.modelData.icon.length > 0
                                sourceSize: Qt.size(40, 40)
                                Layout.preferredWidth: 40
                                Layout.preferredHeight: 40
                                fillMode: Image.PreserveAspectFit
                                Accessible.ignored: true
                            }
                            ColumnLayout {
                                spacing: 0
                                Layout.fillWidth: true
                                Label {
                                    objectName: "pluginName"
                                    text: card.modelData.name + "  " + card.modelData.version
                                    font.weight: Font.DemiBold
                                    color: Theme.textPrimary
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Label {
                                    objectName: "pluginAuthor"
                                    visible: card.modelData.author.length > 0
                                    text: qsTr("By %1").arg(card.modelData.author)
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fontCaption
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Label {
                                    objectName: "pluginVerification"
                                    text: card.modelData.verificationText
                                    color: card.modelData.verification === 0 ? Theme.successForeground
                                                                            : Theme.warningForeground
                                    font.pixelSize: Theme.fontCaption
                                    Layout.fillWidth: true
                                }
                            }
                            Switch {
                                objectName: "enabledSwitch"
                                checked: card.modelData.enabled
                                Accessible.name: qsTr("Enable %1").arg(card.modelData.name)
                                onToggled: root.plugins.setEnabled(card.modelData.id, checked)
                            }
                            IconButton {
                                objectName: "websiteButton"
                                iconName: "info"
                                toolTip: qsTr("Open website")
                                visible: card.modelData.homepage.length > 0
                                onClicked: root.plugins.openWebsite(card.modelData.id)
                            }
                            IconButton {
                                objectName: "uninstallButton"
                                iconName: "delete"
                                toolTip: qsTr("Uninstall")
                                onClicked: root.plugins.requestUninstall(card.modelData.id)
                            }
                        }
                        Label {
                            visible: card.modelData.description.length > 0
                            text: card.modelData.description
                            color: Theme.textSecondary
                            wrapMode: Text.WordWrap
                            maximumLineCount: 4
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Repeater {
                            model: card.modelData.types.concat(card.modelData.permissions)
                            Label {
                                required property string modelData
                                objectName: "abilityRow"
                                text: "• " + modelData
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontCaption
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                        }
                        Repeater {
                            model: card.modelData.warnings
                            Label {
                                required property string modelData
                                text: modelData
                                color: Theme.warningForeground
                                font.pixelSize: Theme.fontCaption
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                        }
                        Switch {
                            objectName: "privateNetworkSwitch"
                            visible: card.modelData.usesNetwork
                            text: qsTr("Allow connections to this computer and the local network")
                            checked: card.modelData.allowPrivateNetwork
                            onToggled: root.plugins.setAllowPrivateNetwork(card.modelData.id, checked)
                        }
                    }
                }
            }

            // --- Store ----------------------------------------------------------------
            RowLayout {
                Layout.fillWidth: true
                SettingsItemSeparator { text: qsTr("Plugin store"); Layout.fillWidth: true }
                Switch {
                    objectName: "showIncompatibleSwitch"
                    text: qsTr("Show plugins for other systems")
                    checked: root.plugins.showIncompatible
                    onToggled: root.plugins.showIncompatible = checked
                }
                IconButton {
                    objectName: "refreshStoreButton"
                    iconName: "refresh"
                    toolTip: qsTr("Reload the plugin list")
                    enabled: root.plugins.catalogStatus !== root.catalogLoading
                    onClicked: root.plugins.refreshStore()
                }
            }
            Label {
                objectName: "catalogStatusLabel"
                visible: text.length > 0
                text: {
                    switch (root.plugins.catalogStatus) {
                    case root.catalogLoading: return qsTr("Loading the plugin list...")
                    case root.catalogCached: return qsTr("The plugin list could not be loaded. Showing the last known list; installs from it cannot be verified.")
                    case root.catalogUnavailable: return qsTr("The plugin list could not be loaded.")
                    case root.catalogLive: return root.plugins.store.length === 0 ? qsTr("No plugins available.") : ""
                    default: return ""
                    }
                }
                color: root.plugins.catalogStatus === root.catalogCached
                       || root.plugins.catalogStatus === root.catalogUnavailable ? Theme.warningForeground
                                                                                 : Theme.textSecondary
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Repeater {
                objectName: "storeRepeater"
                model: root.plugins.store
                Card {
                    id: entry
                    required property var modelData
                    objectName: "storeCard"
                    RowLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        spacing: Theme.spacingM
                        ColumnLayout {
                            spacing: 0
                            Layout.fillWidth: true
                            Label {
                                objectName: "storeName"
                                text: entry.modelData.name + "  " + entry.modelData.version
                                font.weight: Font.DemiBold
                                color: entry.modelData.compatible ? Theme.textPrimary : Theme.textDisabled
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Label {
                                visible: entry.modelData.author.length > 0 || entry.modelData.feed.length > 0
                                text: (entry.modelData.author.length > 0 ? qsTr("By %1").arg(entry.modelData.author) : "")
                                      + (entry.modelData.feed.length > 0
                                         ? "  ·  " + qsTr("From the additional list \"%1\" (not official)").arg(entry.modelData.feed)
                                         : "")
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontCaption
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                            Label {
                                visible: entry.modelData.description.length > 0
                                text: entry.modelData.description
                                color: Theme.textSecondary
                                wrapMode: Text.WordWrap
                                maximumLineCount: 4
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Label {
                                objectName: "storeNote"
                                visible: text.length > 0
                                text: entry.modelData.note
                                color: Theme.warningForeground
                                font.pixelSize: Theme.fontCaption
                                Layout.fillWidth: true
                            }
                        }
                        Button {
                            objectName: "storeInstallButton"
                            text: entry.modelData.updateAvailable ? qsTr("Update")
                                : entry.modelData.installed ? qsTr("Installed") : qsTr("Install")
                            highlighted: entry.modelData.updateAvailable || !entry.modelData.installed
                            enabled: entry.modelData.compatible && !root.plugins.busy
                                     && (entry.modelData.updateAvailable || !entry.modelData.installed)
                            onClicked: root.plugins.installFromStore(entry.modelData.id, entry.modelData.feed)
                        }
                    }
                }
            }
            Item { implicitHeight: Theme.spacingXL }
        }
    }

    BusyIndicator {
        objectName: "pluginsBusy"
        anchors.centerIn: parent
        running: root.plugins.busy
        visible: running
    }
}
