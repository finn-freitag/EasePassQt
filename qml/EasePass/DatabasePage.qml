import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// C#: Views/ManageDatabasePage - the known databases with their state on the left, the actions
// for the selected one on the right. Actions that need the decrypted database (second factor,
// import into, export, backup, change password, print) are available for the loaded database;
// other databases can be loaded (via the login page), renamed, deleted or have their path copied.
//
// manager: DatabaseManagementViewModel, controller: DatabaseController, printer: PrintViewModel.
Item {
    id: root
    required property var manager
    required property var controller
    property var printer: null
    property var autoLogout: null
    property bool showBack: true
    // Tests: used instead of opening a file dialog when non-empty.
    property string fileOverride: ""

    property string selectedPath: ""
    readonly property var selected: {
        const list = manager ? manager.databases : []
        for (let i = 0; i < list.length; ++i)
            if (list[i].path === selectedPath) return list[i]
        return null
    }
    readonly property bool selectedLoaded: selected !== null && selected.loaded === true
    readonly property bool canModify: selectedLoaded && manager && !manager.readOnly

    signal backRequested()
    signal copyRequested(string text)

    function select(path) { selectedPath = path }
    function selectLoaded() {
        const list = manager ? manager.databases : []
        for (let i = 0; i < list.length; ++i)
            if (list[i].loaded) { selectedPath = list[i].path; return }
        selectedPath = list.length > 0 ? list[0].path : ""
    }
    function showMessage(severity, text) {
        pageInfo.severity = severity
        pageInfo.message = text
        pageInfo.show()
    }

    // --- File picking ---------------------------------------------------------------------
    property var _fileCallback: null
    function pickFile(save, title, filters, defaultName, callback) {
        if (fileOverride.length > 0) {
            callback(fileOverride)
            return
        }
        _fileCallback = callback
        fileLoader.save = save
        fileLoader.title = title
        fileLoader.filters = filters
        fileLoader.defaultName = defaultName
        fileLoader.active = true
    }
    readonly property var databaseFilters: [qsTr("Ease Pass database (*.epdb)"), qsTr("All files (*)")]

    // --- Flows ----------------------------------------------------------------------------
    // Export (C#: ExportPasswordsHelper.Export): [new password] -> selection -> target file.
    property bool _exportNewPassword: false
    property string _exportPassword: ""
    property string _exportRepeat: ""
    property var _exportIndexes: []
    function startExport(withNewPassword) {
        _exportNewPassword = withNewPassword
        _exportPassword = ""
        _exportRepeat = ""
        if (withNewPassword) {
            newPasswordDialog.open()
        } else {
            exportSelection.open()
        }
    }
    function exportTo(target) {
        if (_exportNewPassword)
            manager.exportWithNewPassword(target, _exportIndexes, _exportPassword, _exportRepeat)
        else
            manager.exportDatabase(target, _exportIndexes)
        _exportPassword = ""
        _exportRepeat = ""
    }
    function startBackup() {
        pickFile(true, qsTr("Backup database"), databaseFilters, manager.defaultBackupFileName(),
                 (f) => manager.backup(f))
    }
    // Import into (C#: ManageDatabaseHelper.ImportIntoDatabase): file -> password -> selection.
    property string _importFile: ""
    function startImportInto() {
        pickFile(false, qsTr("Import passwords"), databaseFilters, "", (f) => {
            root._importFile = f
            importPasswordDialog.title = qsTr("Enter the password of %1").arg(manager.nameForPath(f))
            importPasswordDialog.open()
        })
    }

    // --- Layout ---------------------------------------------------------------------------
    Rectangle {
        anchors.fill: parent
        color: Theme.background
    }
    ScrollView {
        id: scroll
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: scroll.availableWidth
            spacing: Theme.spacing

            RowLayout {
                Layout.fillWidth: true
                Layout.margins: Theme.spacingL
                IconButton {
                    objectName: "backButton"
                    iconName: "chevron-right"
                    rotation: 180
                    toolTip: qsTr("Back")
                    visible: root.showBack
                    onClicked: root.backRequested()
                }
                Label {
                    text: qsTr("Manage database")
                    font.pixelSize: Theme.fontTitle
                    color: Theme.textPrimary
                    horizontalAlignment: Text.AlignHCenter
                    Layout.fillWidth: true
                }
                Item { implicitWidth: Theme.controlHeight }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.spacingL
                Layout.rightMargin: Theme.spacingL
                SettingsItemSeparator {
                    text: qsTr("Your databases")
                    Layout.fillWidth: true
                }
                IconButton {
                    objectName: "createDatabaseButton"
                    iconName: "add"
                    text: qsTr("Create database")
                    toolTip: qsTr("Create a new database")
                    onClicked: createDialog.open()
                }
                IconButton {
                    objectName: "importDatabaseButton"
                    iconName: "folder"
                    text: qsTr("Import database")
                    toolTip: qsTr("Add an existing database file")
                    onClicked: importFlow.start()
                }
            }

            GridLayout {
                columns: scroll.availableWidth > 700 ? 2 : 1
                columnSpacing: Theme.spacingL
                rowSpacing: Theme.spacingL
                Layout.fillWidth: true
                Layout.leftMargin: Theme.spacingL
                Layout.rightMargin: Theme.spacingL
                Layout.bottomMargin: Theme.spacingL

                // Database cards
                Flow {
                    id: databaseFlow
                    spacing: Theme.spacing
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    Repeater {
                        id: databaseRepeater
                        objectName: "databaseRepeater"
                        model: root.manager ? root.manager.databases : []
                        delegate: ItemDelegate {
                            id: card
                            required property var modelData
                            width: 250
                            height: 130
                            padding: Theme.spacingM
                            highlighted: root.selectedPath === modelData.path
                            Accessible.name: modelData.name
                            Accessible.description: modelData.state
                            background: Rectangle {
                                radius: Theme.radius
                                color: card.down ? Theme.cardPressed : card.hovered ? Theme.cardHover : Theme.card
                                border.width: card.highlighted ? 2 : 1
                                border.color: card.highlighted ? Theme.accent : Theme.cardBorder
                            }
                            contentItem: ColumnLayout {
                                spacing: Theme.spacingXS
                                Label {
                                    objectName: "databaseState"
                                    text: card.modelData.state
                                    visible: text.length > 0
                                    font.pixelSize: Theme.fontCaption
                                    color: card.modelData.exists ? Theme.successForeground : Theme.errorForeground
                                }
                                Item { Layout.fillHeight: true }
                                Label {
                                    text: card.modelData.name
                                    font.pixelSize: Theme.fontSubtitle
                                    color: card.modelData.exists ? Theme.textPrimary : Theme.textDisabled
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Label {
                                    text: card.modelData.path
                                    font.pixelSize: Theme.fontCaption
                                    color: Theme.textSecondary
                                    elide: Text.ElideMiddle
                                    Layout.fillWidth: true
                                }
                            }
                            onClicked: root.select(modelData.path)
                            TapHandler {
                                acceptedButtons: Qt.RightButton
                                onTapped: (point) => {
                                    root.select(card.modelData.path)
                                    cardMenu.popup(card, point.position.x, point.position.y)
                                }
                            }
                        }
                    }
                }

                // Actions for the selected database
                Pane {
                    id: actionPane
                    objectName: "actionPane"
                    visible: root.selected !== null
                    padding: Theme.spacingL
                    Layout.preferredWidth: 300
                    Layout.fillWidth: parent.columns === 1
                    Layout.alignment: Qt.AlignTop
                    background: Rectangle {
                        radius: Theme.radius
                        color: Theme.card
                        border.color: Theme.cardBorder
                    }

                    ColumnLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        spacing: Theme.spacing

                        Label {
                            objectName: "selectedName"
                            text: root.selectedLoaded ? root.manager.databaseName
                                                      : (root.selected ? root.selected.name : "")
                            font.pixelSize: Theme.fontSubtitle
                            color: Theme.textPrimary
                            wrapMode: Text.WrapAnywhere
                            Layout.fillWidth: true
                        }
                        Label {
                            text: root.selectedLoaded
                                  ? (root.manager.readOnly ? qsTr("This database is read-only.")
                                                           : qsTr("This database is loaded."))
                                  : qsTr("Load this database to manage its passwords and security settings.")
                            wrapMode: Text.WordWrap
                            color: Theme.textSecondary
                            Layout.fillWidth: true
                        }

                        // Not loaded
                        Button {
                            objectName: "loadButton"
                            text: qsTr("Load this database")
                            icon.source: Theme.icon("key")
                            icon.color: Theme.accentText
                            highlighted: true
                            visible: !root.selectedLoaded
                            enabled: root.selected !== null && root.selected.exists
                            Layout.fillWidth: true
                            onClicked: root.controller.switchTo(root.selectedPath)
                        }
                        // Loaded temporary database
                        Button {
                            objectName: "makePermanentButton"
                            text: qsTr("Load temp database")
                            icon.source: Theme.icon("add")
                            icon.color: Theme.textPrimary
                            visible: root.selectedLoaded && root.manager.temporary
                            Layout.fillWidth: true
                            onClicked: root.controller.makePermanent()
                        }

                        // Loaded database
                        Button {
                            objectName: "secondFactorButton"
                            text: qsTr("Manage second factor")
                            icon.source: Theme.icon("lock")
                            icon.color: Theme.textPrimary
                            visible: root.selectedLoaded
                            enabled: root.canModify
                            Layout.fillWidth: true
                            onClicked: secondFactorDialog.open()
                        }
                        Button {
                            objectName: "changePasswordButton"
                            text: qsTr("Change password")
                            icon.source: Theme.icon("key")
                            icon.color: Theme.textPrimary
                            visible: root.selectedLoaded
                            enabled: root.canModify
                            Layout.fillWidth: true
                            onClicked: changePasswordDialog.open()
                        }
                        Button {
                            objectName: "importIntoButton"
                            text: qsTr("Import passwords")
                            icon.source: Theme.icon("add")
                            icon.color: Theme.textPrimary
                            visible: root.selectedLoaded
                            enabled: root.canModify
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Import passwords from another database into this one")
                            Layout.fillWidth: true
                            onClicked: root.startImportInto()
                        }
                        Button {
                            objectName: "exportButton"
                            text: qsTr("Export database")
                            icon.source: Theme.icon("export")
                            icon.color: Theme.textPrimary
                            visible: root.selectedLoaded
                            Layout.fillWidth: true
                            onClicked: root.startExport(false)
                        }
                        Button {
                            objectName: "exportNewPasswordButton"
                            text: qsTr("Export with new password")
                            icon.source: Theme.icon("export")
                            icon.color: Theme.textPrimary
                            visible: root.selectedLoaded
                            Layout.fillWidth: true
                            onClicked: root.startExport(true)
                        }
                        Button {
                            objectName: "backupButton"
                            text: qsTr("Backup")
                            icon.source: Theme.icon("copy")
                            icon.color: Theme.textPrimary
                            visible: root.selectedLoaded
                            Layout.fillWidth: true
                            onClicked: root.startBackup()
                        }
                        Button {
                            objectName: "printButton"
                            text: qsTr("Print database")
                            icon.source: Theme.icon("export")
                            icon.color: Theme.textPrimary
                            visible: root.selectedLoaded && root.printer !== null
                            Layout.fillWidth: true
                            onClicked: printDialog.open()
                        }

                        MenuSeparator { Layout.fillWidth: true }

                        // Any database
                        Button {
                            objectName: "copyPathButton"
                            text: qsTr("Copy path")
                            icon.source: Theme.icon("copy")
                            icon.color: Theme.textPrimary
                            enabled: root.selectedPath.length > 0
                            Layout.fillWidth: true
                            onClicked: root.manager.copyPath(root.selectedPath)
                        }
                        Button {
                            objectName: "renameButton"
                            text: qsTr("Rename")
                            icon.source: Theme.icon("edit")
                            icon.color: Theme.textPrimary
                            enabled: !root.selectedLoaded && root.selected !== null && root.selected.exists
                            Layout.fillWidth: true
                            onClicked: renameDialog.openFor(root.selectedPath)
                        }
                        Button {
                            objectName: "deleteDatabaseButton"
                            text: qsTr("Delete database")
                            icon.source: Theme.icon("delete")
                            icon.color: Theme.textPrimary
                            enabled: !root.selectedLoaded && root.selected !== null
                            ToolTip.visible: hovered && root.selectedLoaded
                            ToolTip.text: qsTr("The loaded database cannot be deleted")
                            Layout.fillWidth: true
                            onClicked: deleteDialog.openFor(root.selectedPath)
                        }
                    }
                }
            }
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

    Component.onCompleted: selectLoaded()

    Connections {
        target: root.manager
        function onMessage(severity, text) { root.showMessage(severity, text) }
        function onDatabasesChanged() {
            if (root.selected === null) root.selectLoaded()
        }
        function onImportFileRead(ok, message, needsSecondFactor) {
            if (!importPasswordDialog.visible)
                return
            if (!ok) {
                importPasswordDialog.errorText = message
                importPasswordDialog.showSecondFactor = importPasswordDialog.showSecondFactor || needsSecondFactor
                return
            }
            importPasswordDialog.close()
            importSelection.names = root.manager.importCandidates
            importSelection.open()
        }
    }
    Connections {
        target: root.controller
        function onCreated(path) { root.select(path) }
    }

    // --- Context menu (C#: card MenuFlyout: Delete, Copy path) -------------------------------
    FlyoutMenu {
        id: cardMenu
        objectName: "cardMenu"
        MenuItem {
            text: qsTr("Delete")
            enabled: !root.selectedLoaded
            onTriggered: deleteDialog.openFor(root.selectedPath)
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Copy path")
            onTriggered: root.manager.copyPath(root.selectedPath)
        }
    }

    // --- Dialogs ----------------------------------------------------------------------------
    CreateDatabaseDialog {
        id: createDialog
        objectName: "createDialog"
        parent: Overlay.overlay
        controller: root.controller
        openAfterCreate: false
    }
    ImportDatabaseFlow {
        id: importFlow
        objectName: "importFlow"
        controller: root.controller
        onImported: (path) => root.select(path)
    }
    ChangePasswordDialog {
        id: changePasswordDialog
        objectName: "changePasswordDialog"
        parent: Overlay.overlay
        manager: root.manager
    }
    ManageSecondFactorDialog {
        id: secondFactorDialog
        objectName: "secondFactorDialog"
        parent: Overlay.overlay
        manager: root.manager
        autoLogout: root.autoLogout
        onCopyRequested: (t) => root.copyRequested(t)
    }
    DeleteDatabaseDialog {
        id: deleteDialog
        objectName: "deleteDialog"
        parent: Overlay.overlay
        manager: root.manager
        onDeleted: root.selectLoaded()
    }
    RenameDatabaseDialog {
        id: renameDialog
        objectName: "renameDialog"
        parent: Overlay.overlay
        manager: root.manager
    }
    PasswordPromptDialog {
        id: newPasswordDialog
        objectName: "newPasswordDialog"
        parent: Overlay.overlay
        title: qsTr("New password for the export")
        askRepeat: true
        onSubmitted: (pw, repeat) => {
            errorText = root.manager.validateNewPassword(pw, repeat)
            if (errorText.length > 0)
                return
            root._exportPassword = pw
            root._exportRepeat = repeat
            close()
            exportSelection.open()
        }
    }
    SelectItemsDialog {
        id: exportSelection
        objectName: "exportSelection"
        parent: Overlay.overlay
        title: qsTr("Select items to export")
        primaryButtonText: qsTr("Export")
        names: root.manager ? root.manager.itemNames : []
        onSelectionAccepted: (indexes) => {
            root._exportIndexes = indexes
            root.pickFile(true, qsTr("Export database"), root.databaseFilters,
                          root.manager.nameForPath(root.selectedPath) + "_export.epdb",
                          (f) => root.exportTo(f))
        }
        onCloseClicked: { root._exportPassword = ""; root._exportRepeat = "" }
    }
    PasswordPromptDialog {
        id: importPasswordDialog
        objectName: "importPasswordDialog"
        parent: Overlay.overlay
        working: root.manager ? root.manager.busy : false
        onSubmitted: (pw, repeat, sf) => root.manager.readImportFile(root._importFile, pw, sf)
    }
    SelectItemsDialog {
        id: importSelection
        objectName: "importSelection"
        parent: Overlay.overlay
        title: qsTr("Import passwords")
        primaryButtonText: qsTr("Add")
        allowOverride: true
        onSelectionAccepted: (indexes, overrideExisting) => root.manager.importSelected(indexes, overrideExisting)
        onCloseClicked: root.manager.cancelImport()
    }
    PrintDatabaseDialog {
        id: printDialog
        objectName: "printDialog"
        parent: Overlay.overlay
        printer: root.printer
        onPdfRequested: (include) => root.pickFile(true, qsTr("Export PDF"), [qsTr("PDF (*.pdf)")],
                                                   root.manager.nameForPath(root.selectedPath) + ".pdf", (f) => {
            if (root.printer.exportPdf(f, include))
                root.showMessage(Theme.Severity.Success, qsTr("PDF exported"))
            else
                root.showMessage(Theme.Severity.Error, root.printer.lastError)
        })
    }

    Loader {
        id: fileLoader
        property bool save: false
        property string title: ""
        property var filters: []
        property string defaultName: ""
        active: false
        sourceComponent: FileDialog {
            title: fileLoader.title
            nameFilters: fileLoader.filters
            fileMode: fileLoader.save ? FileDialog.SaveFile : FileDialog.OpenFile
            selectedFile: fileLoader.save && fileLoader.defaultName.length > 0
                          ? currentFolder + "/" + fileLoader.defaultName : ""
            onAccepted: {
                const cb = root._fileCallback
                const f = root.controller.toLocalPath(selectedFile.toString())
                root._fileCallback = null
                fileLoader.active = false
                if (cb) cb(f)
            }
            onRejected: { root._fileCallback = null; fileLoader.active = false }
        }
        onLoaded: item.open()
    }
}
