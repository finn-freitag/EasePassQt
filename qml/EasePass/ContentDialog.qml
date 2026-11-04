import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// WinUI ContentDialog replacement: modal dialog with title, arbitrary content (default
// property) and up to three buttons. Empty button text hides the button.
// Result: primaryClicked / secondaryClicked / closeClicked, then the dialog closes
// (unless closeOnPrimary is false, e.g. for validation). Escape == close button.
Dialog {
    id: root
    enum Result { None, Primary, Secondary }

    property string primaryButtonText: ""
    property string secondaryButtonText: ""
    property string closeButtonText: qsTr("Cancel")
    property bool primaryButtonEnabled: true
    property bool secondaryButtonEnabled: true
    property bool closeOnPrimary: true
    property int dialogResult: ContentDialog.Result.None
    default property alias content: contentColumn.data
    signal primaryClicked()
    signal secondaryClicked()
    signal closeClicked()

    function clickPrimary() {
        if (!primaryButtonEnabled || primaryButtonText.length === 0) return
        dialogResult = ContentDialog.Result.Primary
        primaryClicked()
        if (closeOnPrimary) close()
    }
    function clickSecondary() {
        if (!secondaryButtonEnabled || secondaryButtonText.length === 0) return
        dialogResult = ContentDialog.Result.Secondary
        secondaryClicked()
        close()
    }
    function clickClose() {
        dialogResult = ContentDialog.Result.None
        closeClicked()
        close()
    }

    modal: true
    focus: true
    closePolicy: Popup.NoAutoClose   // Escape handled below so it emits closeClicked()
    anchors.centerIn: parent
    width: Math.min(parent ? parent.width - 2 * Theme.spacingXL : 548, 548)
    padding: Theme.spacingXL
    onAboutToShow: dialogResult = ContentDialog.Result.None


    background: Rectangle {
        color: Theme.card
        radius: Theme.radius
        border.color: Theme.cardBorder
    }
    Overlay.modal: Rectangle { color: Theme.overlay }

    header: Label {
        text: root.title
        visible: text.length > 0
        font.pixelSize: Theme.fontSubtitle
        font.weight: Font.DemiBold
        color: Theme.textPrimary
        padding: Theme.spacingXL
        bottomPadding: 0
        wrapMode: Text.WordWrap
    }

    contentItem: ColumnLayout {
        id: contentColumn
        spacing: Theme.spacingM
        Keys.onEscapePressed: root.clickClose()
    }

    footer: Rectangle {
        color: Theme.background
        implicitHeight: buttonRow.implicitHeight + 2 * Theme.spacingXL
        radius: Theme.radius
        RowLayout {
            id: buttonRow
            anchors.fill: parent
            anchors.margins: Theme.spacingXL
            spacing: Theme.spacing
            Button {
                objectName: "primaryButton"
                text: root.primaryButtonText
                visible: text.length > 0
                enabled: root.primaryButtonEnabled
                highlighted: true
                Layout.fillWidth: true
                onClicked: root.clickPrimary()
            }
            Button {
                objectName: "secondaryButton"
                text: root.secondaryButtonText
                visible: text.length > 0
                enabled: root.secondaryButtonEnabled
                Layout.fillWidth: true
                onClicked: root.clickSecondary()
            }
            Button {
                objectName: "closeButton"
                text: root.closeButtonText
                visible: text.length > 0
                Layout.fillWidth: true
                onClicked: root.clickClose()
            }
        }
    }

    Shortcut {
        sequences: [StandardKey.Cancel]
        enabled: root.opened
        context: Qt.WindowShortcut
        onActivated: root.clickClose()
    }
}
