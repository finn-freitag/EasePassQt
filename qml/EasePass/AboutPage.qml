import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// C#: Views/AboutPage - version, developers, links, changelog, contribute, dependencies,
// privacy policy and disclaimer. `about` is an AboutViewModel; every link goes through
// about.openUrl() (http, https and mailto only).
Item {
    id: root
    required property var about
    property bool showBack: true
    // C#: navigating with a parameter opens the changelog (e.g. after an update).
    property bool changelogExpanded: false

    signal backRequested()

    Rectangle {
        anchors.fill: parent
        color: Theme.background
    }

    component LinkButton: Button {
        property string url: ""
        flat: true
        palette.buttonText: Theme.accent
        font.underline: hovered
        Accessible.role: Accessible.Link
        ToolTip.visible: hovered && url.length > 0
        ToolTip.text: url
        ToolTip.delay: 500
        onClicked: root.about.openUrl(url)
    }
    component Heading: Label {
        font.pixelSize: Theme.fontSubtitle
        font.weight: Font.DemiBold
        color: Theme.textPrimary
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
        Layout.topMargin: Theme.spacing
    }
    component Paragraph: Label {
        color: Theme.textPrimary
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }

    ScrollView {
        id: scroll
        objectName: "aboutScroll"
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
                    text: qsTr("About")
                    font.pixelSize: Theme.fontTitle
                    color: Theme.textPrimary
                    horizontalAlignment: Text.AlignHCenter
                    Layout.fillWidth: true
                }
                Item { implicitWidth: Theme.controlHeight }
            }

            // --- App card: icon, name, version, developers ----------------------------------
            Pane {
                objectName: "appCard"
                padding: Theme.spacingL
                Layout.fillWidth: true
                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: Theme.card
                    border.color: Theme.cardBorder
                }
                GridLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    columns: parent.width > 640 ? 4 : 2
                    columnSpacing: Theme.spacingXL
                    rowSpacing: Theme.spacing

                    Image {
                        source: "qrc:/about/appicon.svg"
                        sourceSize: Qt.size(50, 50)
                        Layout.preferredWidth: 50
                        Layout.preferredHeight: 50
                        Accessible.ignored: true
                    }
                    Label {
                        text: "Ease Pass"
                        font.pixelSize: Theme.fontSubtitle
                        color: Theme.textPrimary
                        Layout.fillWidth: true
                    }
                    ColumnLayout {
                        spacing: 0
                        Label {
                            text: qsTr("Version")
                            font.pixelSize: Theme.fontBody
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                        }
                        Label {
                            objectName: "versionLabel"
                            text: root.about.version
                            color: Theme.textSecondary
                        }
                    }
                    ColumnLayout {
                        spacing: 0
                        Label {
                            text: qsTr("Developers")
                            font.pixelSize: Theme.fontBody
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                        }
                        Flow {
                            Layout.preferredWidth: 240
                            Repeater {
                                id: developerRepeater
                                objectName: "developerRepeater"
                                model: root.about.developers
                                delegate: LinkButton {
                                    required property var modelData
                                    text: modelData.name
                                    url: modelData.url
                                    padding: Theme.spacingXS
                                }
                            }
                        }
                    }
                }
            }

            // --- More ----------------------------------------------------------------------
            SettingsItemSeparator { text: qsTr("More"); Layout.fillWidth: true }
            SettingsControl {
                iconName: "add"
                header: qsTr("Contributor")
                Layout.fillWidth: true
                Row {
                    Repeater {
                        model: root.about.contributors
                        delegate: LinkButton {
                            required property var modelData
                            text: modelData.name
                            url: modelData.url
                        }
                    }
                }
            }
            SettingsControl {
                objectName: "websiteLink"
                iconName: "search"
                header: qsTr("Website")
                description: root.about.websiteUrl
                clickable: true
                Layout.fillWidth: true
                onClicked: root.about.openUrl(root.about.websiteUrl)
            }
            SettingsControl {
                objectName: "githubLink"
                iconName: "info"
                header: qsTr("GitHub")
                description: root.about.repositoryUrl
                clickable: true
                Layout.fillWidth: true
                onClicked: root.about.openUrl(root.about.repositoryUrl)
            }
            SettingsControl {
                objectName: "originalLink"
                iconName: "info"
                header: qsTr("Original Windows version")
                description: root.about.originalRepositoryUrl
                clickable: true
                Layout.fillWidth: true
                onClicked: root.about.openUrl(root.about.originalRepositoryUrl)
            }

            Expander {
                id: changelog
                objectName: "changelogExpander"
                iconName: "refresh"
                header: qsTr("Changelog")
                expanded: root.changelogExpanded
                Layout.fillWidth: true
                Layout.topMargin: Theme.spacingXS
                Repeater {
                    id: changelogRepeater
                    objectName: "changelogRepeater"
                    model: root.about.changelog
                    delegate: ColumnLayout {
                        required property var modelData
                        spacing: Theme.spacingXS
                        Layout.fillWidth: true
                        Heading { text: modelData.title }
                        Repeater {
                            model: modelData.lines
                            delegate: Paragraph {
                                required property string modelData
                                text: modelData
                            }
                        }
                    }
                }
            }

            SettingsControl {
                iconName: "edit"
                header: qsTr("Contribute")
                Layout.fillWidth: true
                Layout.topMargin: Theme.spacingXS
                Row {
                    spacing: Theme.spacingS
                    LinkButton {
                        objectName: "reportBugButton"
                        text: qsTr("Report bug")
                        url: root.about.issuesUrl
                    }
                    LinkButton {
                        objectName: "requestFeatureButton"
                        text: qsTr("Request feature")
                        url: root.about.issuesUrl
                    }
                }
            }

            Expander {
                objectName: "dependenciesExpander"
                iconName: "copy"
                header: qsTr("Dependencies used")
                Layout.fillWidth: true
                Layout.topMargin: Theme.spacingXS
                Repeater {
                    id: dependencyRepeater
                    objectName: "dependencyRepeater"
                    model: root.about.dependencies
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        LinkButton {
                            text: modelData.version.length > 0 ? modelData.name + " " + modelData.version
                                                                : modelData.name
                            url: modelData.url
                        }
                        Item { Layout.fillWidth: true }
                        Label {
                            text: qsTr("License: %1").arg(modelData.license)
                            color: Theme.textSecondary
                        }
                    }
                }
                Paragraph {
                    text: qsTr("Ease Pass Qt is released under the MIT license.")
                    color: Theme.textSecondary
                }
            }

            // --- Legal ---------------------------------------------------------------------
            SettingsItemSeparator { text: qsTr("Legal"); Layout.fillWidth: true }
            Expander {
                objectName: "privacyExpander"
                iconName: "lock"
                header: qsTr("Privacy policy")
                Layout.fillWidth: true
                Paragraph { text: qsTr("By using the app you are consenting to our policies regarding the collection, use and disclosure of personal information set out in this privacy policy.") }
                Heading { text: qsTr("Collection of Personal Information") }
                Paragraph { text: qsTr("We do not collect, store, use or share any information, personal or otherwise.") }
                Heading { text: qsTr("Email") }
                Paragraph { text: qsTr("If you email the developer for support or other feedback, the emails with email addresses will be retained for quality assurance purposes. The email addresses will be used only to reply to the concerns or suggestions raised and will never be used for any marketing purpose.") }
                Heading { text: qsTr("Disclosure of Personal Information") }
                Paragraph { text: qsTr("We will not disclose your information to any third party except if you expressly consent or where required by law.") }
                Heading { text: qsTr("Contacting Us") }
                Paragraph { text: qsTr("If you have any questions regarding this privacy policy, you can email") }
                LinkButton {
                    objectName: "contactLink"
                    text: root.about.contactEmail
                    url: "mailto:" + root.about.contactEmail
                }
            }
            Expander {
                objectName: "disclaimerExpander"
                iconName: "warning"
                header: qsTr("Disclaimer")
                Layout.fillWidth: true
                Layout.topMargin: Theme.spacingXS
                Paragraph { text: qsTr("We are doing our best to prepare the content of this app. However, Ease Pass cannot warranty the expressions and suggestions of the contents, as well as its accuracy. In addition, to the extent permitted by the law, Ease Pass shall not be responsible for any losses and/or damages due to the usage of the information on our app.") }
                Paragraph { text: qsTr("By using our app, you hereby consent to our disclaimer and agree to its terms.") }
                Paragraph { text: qsTr("Any links contained in our app may lead to external sites are provided for convenience only. Any information or statements that appeared in these sites or app are not sponsored, endorsed, or otherwise approved by Ease Pass. For these external sites, Ease Pass cannot be held liable for the availability of, or the content located on or through it. Plus, any losses or damages occurred from using these contents or the internet generally.") }
            }
            Item { implicitHeight: Theme.spacingXL }
        }
    }
}
