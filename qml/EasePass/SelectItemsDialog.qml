import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// C#: Dialogs/SelectExportPasswordsDialog and Dialogs/ImportPasswordsDialog - multi-selection
// of password entries with "Select all" and a counter; everything is selected initially.
// allowOverride adds the "Override" button of the import dialog, which needs the confirmation
// check box first.
ContentDialog {
    id: root
    property var names: []
    property var checked: []
    property bool allowOverride: false
    readonly property int selectedCount: {
        let n = 0
        for (let i = 0; i < checked.length; ++i) if (checked[i]) ++n
        return n
    }
    signal selectionAccepted(var indexes, bool overrideExisting)

    function selectedIndexes() {
        const result = []
        for (let i = 0; i < checked.length; ++i) if (checked[i]) result.push(i)
        return result
    }
    function setAll(on) {
        const c = []
        for (let i = 0; i < names.length; ++i) c.push(on)
        checked = c
    }
    function setChecked(i, on) {
        const c = checked.slice()
        c[i] = on
        checked = c
    }
    function itemAt(i) { return itemList.itemAtIndex(i) }

    closeButtonText: qsTr("Cancel")
    secondaryButtonText: allowOverride ? qsTr("Override") : ""
    primaryButtonEnabled: selectedCount > 0
    secondaryButtonEnabled: selectedCount > 0
    width: Math.min(parent ? parent.width - 2 * Theme.spacingXL : 600, 600)

    onAboutToShow: {
        setAll(true)
        confirmOverride.checked = false
        confirmOverride.visible = false
    }
    onPrimaryClicked: selectionAccepted(selectedIndexes(), false)

    // Override needs the confirmation check box (C#: ImportPasswordsDialog.Dialog_Closing).
    closeOnSecondary: false
    onSecondaryClicked: {
        if (!confirmOverride.visible || !confirmOverride.checked) {
            confirmOverride.visible = true
            return
        }
        selectionAccepted(selectedIndexes(), true)
        close()
    }

    Label {
        objectName: "selectionCount"
        text: root.selectedCount + " / " + root.names.length
        color: Theme.textSecondary
    }
    ListView {
        id: itemList
        objectName: "selectionList"
        model: root.names
        clip: true
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(contentHeight, 320)
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}
        delegate: CheckDelegate {
            required property int index
            required property string modelData
            width: ListView.view.width
            text: modelData
            checked: root.checked[index] === true
            onToggled: root.setChecked(index, checked)
        }
    }
    CheckBox {
        objectName: "selectAll"
        text: qsTr("Select all")
        checked: root.names.length > 0 && root.selectedCount === root.names.length
        onToggled: root.setAll(checked)
    }
    CheckBox {
        id: confirmOverride
        objectName: "confirmOverride"
        visible: false
        text: qsTr("All existing passwords of this database will be replaced. Confirm override?")
        Layout.fillWidth: true
        contentItem: Label {
            leftPadding: confirmOverride.indicator.width + confirmOverride.spacing
            text: confirmOverride.text
            wrapMode: Text.WordWrap
            color: confirmOverride.visible && !confirmOverride.checked ? Theme.errorForeground : Theme.textPrimary
        }
    }
}
