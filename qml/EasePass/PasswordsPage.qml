import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// C#: Views/PasswordsPage.xaml(.cs) - password list with search and sort on the left, details of
// the selected item on the right, splitter between them (width persisted), context menu,
// bottom command bar and the C# keyboard shortcuts:
//   Ctrl+F search, Ctrl+N new, Ctrl+E / F2 edit, Ctrl+Up/Down select, Ctrl+L logout,
//   F1 settings, Esc deselect.
// Adding/editing items and the 2FA/generator dialogs are T15; the page emits requests for them.
FocusScope {
    id: root
    required property var listModel     // App::PasswordFilterModel
    required property var item          // App::ItemViewModel
    required property var actions       // App::PasswordActions
    required property var settings      // Core::AppSettings
    required property var controller    // App::DatabaseController
    required property var clipboard     // App::ClipboardService
    property var icons: null            // App::WebsiteIconService (optional)

    signal addRequested()
    signal editRequested(int sourceIndex)
    signal add2FARequested(int sourceIndex)
    signal generateRequested()
    signal settingsRequested()
    signal aboutRequested()
    signal manageDatabaseRequested()

    property int selectedSource: -1
    readonly property bool readOnly: actions ? actions.readOnly : false
    readonly property bool doubleClickCopy: settings ? settings.doubleClickCopy : true
    readonly property int totpInterval: item && parseInt(item.interval) > 0 ? parseInt(item.interval) : 30

    // --- Selection (database index; list rows are mapped through the proxy) -------------
    function select(sourceIndex) {
        if (sourceIndex === selectedSource && item.index === sourceIndex)
            return
        selectedSource = sourceIndex
        if (sourceIndex < 0) {
            item.unload()
            passwordList.currentIndex = -1
            return
        }
        item.load(sourceIndex)
        actions.registerClick(sourceIndex) // C#: selecting counts as a click (popularity sort)
        syncCurrentRow()
    }
    function selectRow(row) {
        if (row < 0 || row >= listModel.count)
            return
        select(listModel.sourceIndex(row))
    }
    function syncCurrentRow() {
        const row = selectedSource >= 0 ? listModel.proxyRow(selectedSource) : -1
        passwordList.currentIndex = row
        if (row < 0 && selectedSource >= 0) { // filtered out or deleted
            selectedSource = -1
            item.unload()
        }
    }
    function toggleSort(mode) {
        listModel.sortMode = listModel.sortMode === mode ? -1 : mode
    }
    function copy(text, secret) {
        if (text && text.length > 0)
            clipboard.copy(text, secret)
    }
    function editSelected() {
        if (!readOnly && selectedSource >= 0)
            editRequested(selectedSource)
    }
    function requestDelete(sourceIndex, name) {
        if (readOnly || sourceIndex < 0)
            return
        deleteDialog.target = sourceIndex
        deleteDialog.itemName = name !== undefined ? name : item.displayName
        deleteDialog.open()
    }
    function requestExport(sourceIndex, withNewPassword) {
        exportFlow.target = sourceIndex
        if (withNewPassword)
            exportPasswordDialog.open()
        else
            exportFlow.pickFile("")
    }

    Accessible.name: qsTr("Passwords")
    Component.onCompleted: searchBox.forceActiveFocus()

    // Reactions to model changes run deferred (Qt.callLater, coalesced): the row signals arrive
    // in the middle of a proxy update, before the ListView has processed them. Changing the
    // selection/currentIndex there made list rows show other items (bug 2026-11-10).
    function selectSingleHit() {
        // C#: a search with exactly one hit selects it.
        if (listModel.filterText.length > 0 && listModel.count === 1)
            selectRow(0)
    }
    Connections {
        target: root.listModel
        function onModelReset() { Qt.callLater(root.syncCurrentRow) }
        function onLayoutChanged() { Qt.callLater(root.syncCurrentRow) }
        function onRowsInserted() { Qt.callLater(root.syncCurrentRow) }
        function onRowsRemoved() { Qt.callLater(root.syncCurrentRow) }
        function onCountChanged() { Qt.callLater(root.selectSingleHit) }
    }
    Connections {
        target: root.item
        function onDeleted(index) { root.selectedSource = -1; root.syncCurrentRow() }
        function onCommitted(index) { root.select(index) }
    }

    // --- Shortcuts (C#: PasswordsPage.Page_KeyDown) --------------------------------------
    Shortcut { sequence: "Ctrl+F"; enabled: root.visible; onActivated: searchBox.forceActiveFocus() }
    Shortcut { sequence: "Ctrl+N"; enabled: root.visible && !root.readOnly; onActivated: root.addRequested() }
    Shortcut { sequences: ["Ctrl+E", "F2"]; enabled: root.visible; onActivated: root.editSelected() }
    Shortcut {
        sequence: "Ctrl+Down"; enabled: root.visible
        onActivated: root.selectRow(Math.min(root.listModel.count - 1, passwordList.currentIndex + 1))
    }
    Shortcut {
        sequence: "Ctrl+Up"; enabled: root.visible
        onActivated: if (passwordList.currentIndex > 0) root.selectRow(passwordList.currentIndex - 1)
    }
    Shortcut { sequence: "Ctrl+L"; enabled: root.visible; onActivated: root.controller.lock() }
    Shortcut { sequence: "F1"; enabled: root.visible; onActivated: root.settingsRequested() }
    Shortcut {
        sequence: "Escape"; enabled: root.visible && !searchBox.suggestionsOpen
        onActivated: root.select(-1)
    }

    Splitter {
        id: splitter
        objectName: "splitter"
        anchors.fill: parent
        orientation: Qt.Horizontal
        onResizingChanged: if (!resizing && root.settings) root.settings.splitterWidth = Math.round(leftPane.width)

        // ================= Left: search, sort, list, command bar =================
        Pane {
            id: leftPane
            objectName: "leftPane"
            SplitView.preferredWidth: root.settings ? root.settings.splitterWidth : 300
            SplitView.minimumWidth: 250
            SplitView.maximumWidth: 500
            padding: Theme.spacing
            background: Rectangle { color: Theme.background }

            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.spacing

                RowLayout {
                    spacing: Theme.spacingS
                    Layout.fillWidth: true
                    Button {
                        objectName: "addButton"
                        icon.source: Theme.icon("add")
                        icon.color: Theme.accentText
                        highlighted: true
                        enabled: !root.readOnly
                        Accessible.name: qsTr("New entry")
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("New entry") + " (Ctrl+N)"
                        onClicked: root.addRequested()
                    }
                    SearchPasswordsBox {
                        id: searchBox
                        objectName: "searchBox"
                        placeholderText: qsTr("Search ('/' for tags)")
                        infoLabel: root.listModel.infoLabel
                        suggestions: root.listModel.suggestions
                        Layout.fillWidth: true
                        onUserTextChanged: (isUserChange, t) => {
                            if (isUserChange) root.listModel.filterText = t
                            else root.listModel.setFilterTextProgrammatically(t)
                        }
                        onSearchKeyPressed: (isTagSearch, event) => {
                            // C#: Down moves into the list (unless choosing a tag suggestion).
                            if (event.key === Qt.Key_Down && !(isTagSearch && suggestionsOpen)) {
                                passwordList.forceActiveFocus()
                                if (passwordList.currentIndex < 0) root.selectRow(0)
                                event.accepted = true
                            }
                        }
                    }
                    IconButton {
                        id: sortButton
                        objectName: "sortButton"
                        iconName: "sort"
                        toolTip: qsTr("Sort")
                        checked: root.listModel.sortMode !== -1
                        onClicked: sortMenu.openAt(sortButton)
                    }
                }

                ListView {
                    id: passwordList
                    objectName: "passwordList"
                    model: root.listModel
                    clip: true
                    currentIndex: -1
                    keyNavigationEnabled: true
                    boundsBehavior: Flickable.StopAtBounds
                    Accessible.role: Accessible.List
                    Accessible.name: qsTr("Password entries")
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ScrollBar.vertical: ScrollBar {}
                    Keys.onUpPressed: if (currentIndex > 0) root.selectRow(currentIndex - 1)
                    Keys.onDownPressed: root.selectRow(Math.min(count - 1, currentIndex + 1))
                    Keys.onMenuPressed: if (currentIndex >= 0) contextMenu.openFor(currentItem, currentItem.width / 2, currentItem.height / 2)

                    delegate: PasswordRow {
                        icons: root.icons
                        onClicked: root.select(sourceIndex)
                        onContextMenuRequested: (src, x, y) => contextMenu.openFor(this, x, y)
                    }
                }

                // C#: bottom CommandBar (Logout, About, Settings)
                RowLayout {
                    spacing: Theme.spacingXS
                    Layout.fillWidth: true
                    IconButton {
                        objectName: "logoutButton"
                        iconName: "logout"
                        text: qsTr("Logout")
                        toolTip: qsTr("Save and lock the database") + " (Ctrl+L)"
                        onClicked: root.controller.lock()
                    }
                    // C#: loadTempDBButton (TemporaryDatabaseHelper.LoadImportedDatabase)
                    IconButton {
                        objectName: "loadTempButton"
                        iconName: "add"
                        text: qsTr("Load temp database")
                        toolTip: qsTr("Add this temporary database to your databases")
                        visible: root.controller.temporary === true
                        onClicked: root.controller.makePermanent()
                    }
                    IconButton {
                        objectName: "aboutButton"
                        iconName: "info"
                        text: qsTr("About")
                        onClicked: root.aboutRequested()
                    }
                    IconButton {
                        objectName: "settingsButton"
                        iconName: "settings"
                        text: qsTr("Settings")
                        toolTip: qsTr("Settings") + " (F1)"
                        onClicked: root.settingsRequested()
                    }
                    Item { Layout.fillWidth: true }
                }
            }
        }

        // ================= Right: details of the selected entry =================
        Pane {
            id: detailPane
            objectName: "detailPane"
            SplitView.fillWidth: true
            padding: Theme.spacingXL
            background: Rectangle { color: Theme.card }

            // C#: oobe_Grid - empty database
            ColumnLayout {
                objectName: "emptyState"
                anchors.centerIn: parent
                width: Math.min(parent.width, 420)
                visible: root.listModel.count === 0 && root.listModel.filterText.length === 0
                spacing: Theme.spacing
                Label {
                    text: qsTr("Welcome to Ease Pass.")
                    font.pixelSize: Theme.fontTitle
                    color: Theme.textPrimary
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                Label {
                    text: qsTr("Use the buttons in the top to create a new entry!")
                    font.pixelSize: Theme.fontSubtitle - 2
                    color: Theme.textSecondary
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                Button {
                    text: qsTr("Manage or import databases")
                    flat: true
                    onClicked: root.manageDatabaseRequested()
                }
            }

            ColumnLayout {
                objectName: "details"
                anchors.fill: parent
                visible: root.item.hasItem && !root.item.isNew
                spacing: Theme.spacingM

                // Header: action buttons + title
                RowLayout {
                    spacing: Theme.spacingS
                    Layout.fillWidth: true
                    Button {
                        objectName: "editButton"
                        text: qsTr("Edit")
                        icon.source: Theme.icon("edit")
                        icon.color: Theme.textPrimary
                        enabled: !root.readOnly
                        onClicked: root.editSelected()
                    }
                    Button {
                        objectName: "deleteButton"
                        text: qsTr("Delete")
                        icon.source: Theme.icon("delete")
                        icon.color: Theme.errorForeground
                        enabled: !root.readOnly
                        onClicked: root.requestDelete(root.selectedSource)
                    }
                    Button {
                        objectName: "twoFactorButton"
                        text: qsTr("2FA")
                        icon.source: Theme.icon("key")
                        icon.color: Theme.textPrimary
                        enabled: !root.readOnly
                        onClicked: root.add2FARequested(root.selectedSource)
                    }
                    Button {
                        objectName: "generateButton"
                        text: qsTr("Generate")
                        icon.source: Theme.icon("generate")
                        icon.color: Theme.textPrimary
                        onClicked: root.generateRequested()
                    }
                    Item { Layout.fillWidth: true }
                }
                Label {
                    objectName: "detailTitle"
                    text: root.item.displayName
                    font.pixelSize: Theme.fontTitle
                    color: Theme.textPrimary
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentWidth: availableWidth

                    ColumnLayout {
                        width: parent.width
                        spacing: Theme.spacing

                        Label { text: qsTr("Username"); color: Theme.textSecondary; visible: usernameBox.visible }
                        CopyTextbox {
                            id: usernameBox
                            objectName: "usernameBox"
                            text: root.item.username
                            visible: text.length > 0
                            accessibleName: qsTr("Username")
                            doubleClickToCopy: root.doubleClickCopy
                            Layout.fillWidth: true
                            onCopyRequested: (t) => root.copy(t, false)
                        }
                        Label { text: qsTr("Email"); color: Theme.textSecondary; visible: emailBox.visible }
                        CopyTextbox {
                            id: emailBox
                            objectName: "emailBox"
                            text: root.item.email
                            visible: text.length > 0
                            accessibleName: qsTr("Email")
                            doubleClickToCopy: root.doubleClickCopy
                            Layout.fillWidth: true
                            onCopyRequested: (t) => root.copy(t, false)
                        }
                        Label { text: qsTr("Password"); color: Theme.textSecondary; visible: passwordBox.visible }
                        CopyPasswordbox {
                            id: passwordBox
                            objectName: "passwordBox"
                            password: root.item.password
                            visible: password.length > 0
                            safetyChecks: root.item.safetyChecks
                            doubleClickToCopy: root.doubleClickCopy
                            Layout.fillWidth: true
                            onCopyRequested: (t) => root.copy(t, true)
                        }
                        Label {
                            objectName: "leakBadge"
                            visible: root.item.leakState === 3 // ItemViewModel.LeakState.Leaked
                            text: qsTr("This password appeared in a data breach. Change it!")
                            color: Theme.errorForeground
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                        Label { text: qsTr("2FA"); color: Theme.textSecondary; visible: tokenBox.visible }
                        CopyTokenBox {
                            id: tokenBox
                            objectName: "tokenBox"
                            visible: root.item.has2FA && token.length > 0
                            token: root.item.totpCode
                            showToken: true
                            progress: root.item.totpRemaining / root.totpInterval
                            Layout.fillWidth: true
                            onCopyRequested: (t) => root.copy(t, true)
                        }
                        ProgressBar {
                            objectName: "totpBar"
                            visible: tokenBox.visible
                            from: 0
                            to: root.totpInterval
                            value: root.item.totpRemaining
                            Accessible.name: qsTr("%n second(s) remaining", "", root.item.totpRemaining)
                            Layout.fillWidth: true
                        }
                        Label { text: qsTr("Website"); color: Theme.textSecondary; visible: websiteBox.visible }
                        CopyTextbox {
                            id: websiteBox
                            objectName: "websiteBox"
                            text: root.item.website
                            visible: text.length > 0
                            accessibleName: qsTr("Website")
                            doubleClickToCopy: root.doubleClickCopy
                            Layout.fillWidth: true
                            onCopyRequested: (t) => root.copy(t, false)
                        }
                        Label { text: qsTr("Tags"); color: Theme.textSecondary; visible: detailTags.visible }
                        TagRenderControl {
                            id: detailTags
                            tags: root.item.tags
                            visible: root.item.tags.length > 0
                            Layout.fillWidth: true
                        }
                        Label { text: qsTr("Notes"); color: Theme.textSecondary; visible: notesArea.visible }
                        TextArea {
                            id: notesArea
                            objectName: "notesArea"
                            text: root.item.notes
                            visible: text.length > 0
                            readOnly: true
                            wrapMode: TextEdit.Wrap
                            selectByMouse: true
                            Accessible.name: qsTr("Notes")
                            Layout.fillWidth: true
                        }
                    }
                }
            }
        }
    }

    // --- Sort menu (C#: SortPasswordListFlyout) -----------------------------------------
    FlyoutMenu {
        id: sortMenu
        objectName: "sortMenu"
        // Values of PasswordFilterModel::SortMode
        MenuItem { text: qsTr("Name"); checkable: true; checked: root.listModel.sortMode === 0; onTriggered: root.toggleSort(0) }
        MenuItem { text: qsTr("Username"); checkable: true; checked: root.listModel.sortMode === 1; onTriggered: root.toggleSort(1) }
        MenuItem { text: qsTr("Notes"); checkable: true; checked: root.listModel.sortMode === 2; onTriggered: root.toggleSort(2) }
        MenuItem { text: qsTr("First tag"); checkable: true; checked: root.listModel.sortMode === 3; onTriggered: root.toggleSort(3) }
        MenuItem { text: qsTr("Website"); checkable: true; checked: root.listModel.sortMode === 4; onTriggered: root.toggleSort(4) }
        MenuItem { text: qsTr("Popular last year"); checkable: true; checked: root.listModel.sortMode === 5; onTriggered: root.toggleSort(5) }
        MenuItem { text: qsTr("Popular last 30 days"); checkable: true; checked: root.listModel.sortMode === 6; onTriggered: root.toggleSort(6) }
        MenuItem { text: qsTr("Password strength"); checkable: true; checked: root.listModel.sortMode === 7; onTriggered: root.toggleSort(7) }
        MenuSeparator {}
        MenuItem {
            text: root.listModel.descending ? qsTr("Sort ascending") : qsTr("Sort descending")
            onTriggered: root.listModel.descending = !root.listModel.descending
        }
    }

    // --- Context menu (C#: RightclickedItem_*) -------------------------------------------
    FlyoutMenu {
        id: contextMenu
        objectName: "contextMenu"
        property int target: -1
        property string name: ""
        property bool hasPassword: false
        property bool hasUsername: false
        property bool hasEmail: false
        property bool has2FA: false

        function openFor(row, x, y) {
            target = row.sourceIndex
            name = row.displayName
            hasPassword = row.hasPassword
            hasUsername = row.username.length > 0
            hasEmail = row.email.length > 0
            has2FA = row.has2FA
            popup(row, x, y)
        }

        MenuItem { objectName: "ctxCopyPassword"; text: qsTr("Copy password"); visible: contextMenu.hasPassword; height: visible ? implicitHeight : 0; onTriggered: root.actions.copyPassword(contextMenu.target) }
        MenuItem { objectName: "ctxCopyEmail"; text: qsTr("Copy email"); visible: contextMenu.hasEmail; height: visible ? implicitHeight : 0; onTriggered: root.actions.copyEmail(contextMenu.target) }
        MenuItem { objectName: "ctxCopyUsername"; text: qsTr("Copy username"); visible: contextMenu.hasUsername; height: visible ? implicitHeight : 0; onTriggered: root.actions.copyUsername(contextMenu.target) }
        MenuItem { objectName: "ctxCopyTotp"; text: qsTr("Copy TOTP token"); visible: contextMenu.has2FA; height: visible ? implicitHeight : 0; onTriggered: root.actions.copyTotp(contextMenu.target) }
        MenuSeparator {}
        MenuItem { objectName: "ctxEdit"; text: qsTr("Edit"); enabled: !root.readOnly; onTriggered: root.editRequested(contextMenu.target) }
        MenuItem { objectName: "ctxDelete"; text: qsTr("Delete"); enabled: !root.readOnly; onTriggered: root.requestDelete(contextMenu.target, contextMenu.name) }
        MenuItem { objectName: "ctxExport"; text: qsTr("Export"); onTriggered: root.requestExport(contextMenu.target, false) }
        MenuItem { objectName: "ctxExportNewPassword"; text: qsTr("Export with new password"); onTriggered: root.requestExport(contextMenu.target, true) }
    }

    // --- Delete confirmation (C#: DeleteConfirmationDialog) ------------------------------
    ContentDialog {
        id: deleteDialog
        objectName: "deleteDialog"
        property int target: -1
        property string itemName: ""
        parent: Overlay.overlay
        title: qsTr("Delete entry")
        primaryButtonText: qsTr("Delete")
        closeButtonText: qsTr("Cancel")
        onPrimaryClicked: {
            if (root.actions.deleteItem(target) && target === root.selectedSource) {
                root.selectedSource = -1
                root.item.unload()
            }
            root.syncCurrentRow()
        }
        Label {
            text: qsTr("Do you really want to delete \"%1\"? This cannot be undone.").arg(deleteDialog.itemName)
            wrapMode: Text.WordWrap
            color: Theme.textPrimary
            Layout.fillWidth: true
        }
    }

    // --- Export (C#: ExportPasswordsHelper; "Export new PW" asks for a password first) ---
    ContentDialog {
        id: exportPasswordDialog
        objectName: "exportPasswordDialog"
        parent: Overlay.overlay
        title: qsTr("Password for the exported database")
        primaryButtonText: qsTr("Continue")
        closeButtonText: qsTr("Cancel")
        primaryButtonEnabled: exportPassword.text.length >= 4
        onAboutToShow: { exportPassword.text = ""; exportPassword.forceActiveFocus() }
        onPrimaryClicked: exportFlow.pickFile(exportPassword.text)
        onClosed: exportPassword.text = "" // T23: do not keep the password in the hidden field
        EditPasswordBox {
            id: exportPassword
            objectName: "exportPassword"
            placeholderText: qsTr("New password")
            Layout.fillWidth: true
        }
    }
    Item {
        id: exportFlow
        objectName: "exportFlow"
        property int target: -1
        property string newPassword: ""
        function pickFile(pw) {
            newPassword = pw
            saveLoader.active = true
        }
        function exportTo(file) {
            root.actions.exportItems([target], file, newPassword)
            newPassword = ""
        }
        Loader {
            id: saveLoader
            active: false
            sourceComponent: FileDialog {
                title: qsTr("Export entry")
                fileMode: FileDialog.SaveFile
                defaultSuffix: "epdb"
                nameFilters: [qsTr("Ease Pass database (*.epdb)")]
                onAccepted: { exportFlow.exportTo(selectedFile.toString()); saveLoader.active = false }
                onRejected: { exportFlow.newPassword = ""; saveLoader.active = false }
            }
            onLoaded: item.open()
        }
    }
    InfoBar {
        id: exportInfo
        objectName: "exportInfo"
        open: false
        autoCloseInterval: 6000
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Theme.spacingL
        width: Math.min(420, parent.width - 2 * Theme.spacingL)
        z: 5
    }
    Connections {
        target: root.actions
        function onExportFinished(ok, message, path) {
            exportInfo.severity = ok ? Theme.Severity.Success : Theme.Severity.Error
            exportInfo.title = ok ? qsTr("Exported") : qsTr("Export failed")
            exportInfo.message = ok ? path : message
            exportInfo.show()
        }
    }
}
