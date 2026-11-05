import QtQuick
import QtQuick.Controls
import QtTest
import EasePass

// Database management page (steps.txt T16) with the real App layer (testContext) on real
// .epdb files in a temporary directory (cheap KDF). Sample items: Banking, amazon, Cloud.
// File dialogs are bypassed with DatabasePage.fileOverride.
Item {
    id: top
    width: 1100; height: 900

    Component {
        id: pageC
        DatabasePage {
            anchors.fill: parent
            manager: testContext.databaseManagement
            controller: testContext.controller
            printer: testContext.printer
        }
    }
    Component { id: spyC; SignalSpy {} }

    TestCase {
        id: tc
        name: "DatabasePage"
        when: windowShown

        property string pathA: ""
        property string pathB: ""

        function init() {
            pathB = testContext.createSampleFile("Beta")
            pathA = testContext.createSampleFile("Alpha")
            verify(pathA.length > 0 && pathB.length > 0)
            testContext.setKnownPaths([pathA, pathB])
            verify(testContext.openFile(pathA))
            testContext.clearClipboard()
            failOnWarning(/.?/)
        }
        function cleanup() {
            testContext.waitForBackground()
        }

        // --- helpers ------------------------------------------------------------------
        function typeInto(field, text) {
            field.forceActiveFocus()
            field.selectAll()
            keyClick(Qt.Key_Delete)
            for (const ch of text)
                keyClick(ch)
        }
        function createPage() {
            const p = createTemporaryObject(pageC, top)
            verify(p)
            waitForRendering(p)   // Flow/GridLayout positions are set on the first polish
            return p
        }
        function card(page, name) {
            const rep = findChild(page, "databaseRepeater")
            for (let i = 0; i < rep.count; ++i)
                if (rep.itemAt(i).modelData.name === name) return rep.itemAt(i)
            return null
        }
        function click(page, objectName) {
            const b = findChild(page, objectName)
            verify(b, objectName)
            verify(b.visible && b.enabled, objectName + " clickable")
            mouseClick(b)
        }
        function openedDialog(page, objectName) {
            const d = findChild(page, objectName)
            verify(d, objectName)
            tryCompare(d, "opened", true)
            return d
        }
        function waitExport(page, action) {
            const spy = createTemporaryObject(spyC, top, { target: testContext.databaseManagement,
                                                           signalName: "exportFinished" })
            action()
            tryCompare(spy, "count", 1, 10000)
            return spy.signalArguments[0]
        }

        // --- list -----------------------------------------------------------------------
        function test_listShowsDatabasesWithState() {
            const missing = testContext.tempPath("Gone.epdb")
            testContext.setKnownPaths([pathA, pathB, missing])
            const page = createPage()
            compare(findChild(page, "databaseRepeater").count, 3)
            compare(card(page, "Alpha").modelData.state, "Loaded")
            compare(card(page, "Beta").modelData.state, "")
            compare(card(page, "Gone").modelData.state, "File not found")
            compare(page.selectedPath, pathA)             // the loaded database is preselected
            verify(findChild(page, "secondFactorButton").visible)
            verify(!findChild(page, "loadButton").visible)
            verify(!findChild(page, "deleteDatabaseButton").enabled) // loaded database
            verify(!findChild(page, "renameButton").enabled)
        }

        function test_copyPath() {
            const page = createPage()
            click(page, "copyPathButton")
            compare(testContext.clipboardText, pathA)
        }

        function test_loadOtherDatabaseGoesToLogin() {
            const page = createPage()
            mouseClick(card(page, "Beta"))
            compare(page.selectedPath, pathB)
            verify(!findChild(page, "secondFactorButton").visible)
            click(page, "loadButton")
            tryCompare(testContext.controller, "isOpen", false)
            compare(testContext.controller.lastDatabase, pathB)
        }

        function test_missingFileCannotBeLoaded() {
            const missing = testContext.tempPath("Gone.epdb")
            testContext.setKnownPaths([pathA, missing])
            const page = createPage()
            mouseClick(card(page, "Gone"))
            verify(!findChild(page, "loadButton").enabled)
            verify(findChild(page, "deleteDatabaseButton").enabled) // removes it from the list
        }

        // --- delete / rename --------------------------------------------------------------
        function test_deleteNeedsTypedName() {
            const page = createPage()
            mouseClick(card(page, "Beta"))
            click(page, "deleteDatabaseButton")
            const d = openedDialog(page, "deleteDialog")
            const del = findChild(d, "primaryButton")
            verify(!del.enabled)
            typeInto(findChild(d, "deleteConfirmName"), "beta")   // case matters
            verify(!del.enabled)
            typeInto(findChild(d, "deleteConfirmName"), "Beta")
            verify(del.enabled)
            mouseClick(del)
            tryCompare(d, "visible", false)
            verify(!testContext.fileExists(pathB))
            compare(testContext.knownPaths(), [pathA])
            compare(page.selectedPath, pathA)
        }

        function test_deleteCancelKeepsFile() {
            const page = createPage()
            mouseClick(card(page, "Beta"))
            click(page, "deleteDatabaseButton")
            const d = openedDialog(page, "deleteDialog")
            typeInto(findChild(d, "deleteConfirmName"), "Beta")
            mouseClick(findChild(d, "closeButton"))
            tryCompare(d, "visible", false)
            verify(testContext.fileExists(pathB))
            compare(testContext.knownPaths().length, 2)
        }

        function test_renameDatabase() {
            const page = createPage()
            mouseClick(card(page, "Beta"))
            click(page, "renameButton")
            const d = openedDialog(page, "renameDialog")
            compare(findChild(d, "renameName").text, "Beta")
            typeInto(findChild(d, "renameName"), "Renamed")
            mouseClick(findChild(d, "primaryButton"))
            tryCompare(d, "visible", false)
            const renamed = testContext.tempPath("Renamed.epdb")
            verify(testContext.fileExists(renamed))
            verify(!testContext.fileExists(pathB))
            verify(testContext.knownPaths().indexOf(renamed) >= 0)
            verify(card(page, "Renamed") !== null)
        }

        function test_renameToExistingNameFails() {
            const page = createPage()
            mouseClick(card(page, "Beta"))
            click(page, "renameButton")
            const d = openedDialog(page, "renameDialog")
            typeInto(findChild(d, "renameName"), "Alpha")
            mouseClick(findChild(d, "primaryButton"))
            verify(d.visible)
            verify(findChild(d, "renameError").open)
            verify(testContext.fileExists(pathB))
        }

        // --- create / import database -------------------------------------------------------
        function test_createDatabaseAddsToList() {
            const page = createPage()
            const d = findChild(page, "createDialog")
            d.folder = testContext.tempPath("")
            click(page, "createDatabaseButton")
            tryCompare(d, "opened", true)
            typeInto(findChild(d, "databaseName"), "Delta")
            typeInto(findChild(d, "createPassword"), "abcd1234")
            typeInto(findChild(d, "createRepeat"), "abcd1234")
            mouseClick(findChild(d, "primaryButton"))
            tryCompare(d, "visible", false, 10000)
            const delta = testContext.tempPath("Delta.epdb")
            verify(testContext.fileExists(delta))
            verify(card(page, "Delta") !== null)
            compare(page.selectedPath, delta)
            verify(testContext.controller.isOpen)           // still the old one loaded
        }

        // --- change password ----------------------------------------------------------------
        function test_changePasswordValidation() {
            const page = createPage()
            click(page, "changePasswordButton")
            const d = openedDialog(page, "changePasswordDialog")
            const err = findChild(d, "changePasswordError")
            const ok = findChild(d, "primaryButton")

            typeInto(findChild(d, "currentPassword"), testContext.password())
            typeInto(findChild(d, "newPassword"), "abc")
            typeInto(findChild(d, "repeatPassword"), "abc")
            mouseClick(ok)
            tryVerify(() => err.open)
            verify(err.message.indexOf("at least") >= 0)

            typeInto(findChild(d, "newPassword"), "abcd1234")
            typeInto(findChild(d, "repeatPassword"), "abcd9999")
            mouseClick(ok)
            tryVerify(() => err.message.indexOf("do not match") >= 0)

            typeInto(findChild(d, "currentPassword"), "wrong")
            typeInto(findChild(d, "repeatPassword"), "abcd1234")
            mouseClick(ok)
            tryVerify(() => err.message.indexOf("wrong") >= 0)
            verify(d.visible)

            typeInto(findChild(d, "currentPassword"), testContext.password())
            mouseClick(ok)
            tryCompare(d, "visible", false, 10000)
            testContext.waitForBackground()
            compare(testContext.readNames(pathA, "abcd1234").length, 3)
            compare(testContext.readNames(pathA, testContext.password()).length, 0)
        }

        function test_changePasswordEmptyFieldsDisableButton() {
            const page = createPage()
            click(page, "changePasswordButton")
            const d = openedDialog(page, "changePasswordDialog")
            verify(!findChild(d, "primaryButton").enabled)
            typeInto(findChild(d, "currentPassword"), "x")
            verify(!findChild(d, "primaryButton").enabled)     // new password still empty
            typeInto(findChild(d, "newPassword"), "y")
            verify(findChild(d, "primaryButton").enabled)
        }

        // --- second factor -------------------------------------------------------------------
        function test_enableSecondFactorNeedsConfirmation() {
            const page = createPage()
            click(page, "secondFactorButton")
            const d = openedDialog(page, "secondFactorDialog")
            const sw = findChild(d, "secondFactorSwitch")
            verify(!sw.checked)
            verify(!findChild(d, "secondFactorToken").visible)
            mouseClick(sw)
            verify(sw.checked)
            const token = findChild(d, "secondFactorToken").token
            compare(token.length, 12)
            mouseClick(findChild(d, "primaryButton"))
            const confirm = findChild(d, "confirmSecondFactor")
            tryCompare(confirm, "opened", true)
            mouseClick(findChild(confirm, "closeButton"))          // cancel: nothing changes
            tryCompare(confirm, "visible", false)
            compare(testContext.secondFactorType(), 0)
            verify(d.visible)

            mouseClick(findChild(d, "primaryButton"))
            tryCompare(confirm, "opened", true)
            mouseClick(findChild(confirm, "primaryButton"))
            tryCompare(d, "visible", false, 10000)
            compare(testContext.secondFactorType(), 1)
            compare(testContext.secondFactorToken(), token)
            testContext.waitForBackground()
            compare(testContext.readNames(pathA, testContext.password()).length, 0)
            compare(testContext.readNames(pathA, testContext.password(), token).length, 3)
        }

        function test_secondFactorOtpAndRemove() {
            const page = createPage()
            click(page, "secondFactorButton")
            let d = openedDialog(page, "secondFactorDialog")
            mouseClick(findChild(d, "secondFactorSwitch"))
            findChild(d, "secondFactorType").currentIndex = 1        // OTP
            mouseClick(findChild(d, "primaryButton"))
            const confirm = findChild(d, "confirmSecondFactor")
            tryCompare(confirm, "opened", true)
            mouseClick(findChild(confirm, "primaryButton"))
            tryCompare(d, "visible", false, 10000)
            compare(testContext.secondFactorType(), 2)
            const token = testContext.secondFactorToken()

            // Reopening shows the stored token; switching off needs no confirmation.
            click(page, "secondFactorButton")
            d = openedDialog(page, "secondFactorDialog")
            verify(findChild(d, "secondFactorSwitch").checked)
            compare(findChild(d, "secondFactorType").currentIndex, 1)
            compare(findChild(d, "secondFactorToken").token, token)
            mouseClick(findChild(d, "secondFactorSwitch"))
            mouseClick(findChild(d, "primaryButton"))
            tryCompare(d, "visible", false, 10000)
            compare(testContext.secondFactorType(), 0)
        }

        // --- export / backup -----------------------------------------------------------------
        function test_exportSelectedItems() {
            const page = createPage()
            page.fileOverride = testContext.tempPath("export.epdb")
            click(page, "exportButton")
            const d = openedDialog(page, "exportSelection")
            compare(findChild(d, "selectionCount").text, "3 / 3")
            const list = findChild(d, "selectionList")
            tryVerify(() => d.itemAt(1) !== null)
            mouseClick(d.itemAt(1))                                   // deselect "amazon"
            compare(findChild(d, "selectionCount").text, "2 / 3")
            verify(!findChild(d, "selectAll").checked)
            const args = waitExport(page, () => mouseClick(findChild(d, "primaryButton")))
            verify(args[0], args[1])
            compare(testContext.readNames(page.fileOverride, testContext.password()), ["Banking", "Cloud"])
        }

        function test_exportSelectAllToggle() {
            const page = createPage()
            click(page, "exportButton")
            const d = openedDialog(page, "exportSelection")
            mouseClick(findChild(d, "selectAll"))
            compare(findChild(d, "selectionCount").text, "0 / 3")
            verify(!findChild(d, "primaryButton").enabled)
            mouseClick(findChild(d, "selectAll"))
            compare(findChild(d, "selectionCount").text, "3 / 3")
            mouseClick(findChild(d, "closeButton"))
            tryCompare(d, "visible", false)
        }

        function test_exportWithNewPassword() {
            const page = createPage()
            page.fileOverride = testContext.tempPath("newpw.epdb")
            click(page, "exportNewPasswordButton")
            const pw = openedDialog(page, "newPasswordDialog")
            typeInto(findChild(pw, "promptPassword"), "newpass1")
            typeInto(findChild(pw, "promptRepeat"), "newpass2")
            mouseClick(findChild(pw, "primaryButton"))
            verify(pw.visible)
            verify(findChild(pw, "promptError").message.indexOf("do not match") >= 0)
            typeInto(findChild(pw, "promptRepeat"), "newpass1")
            mouseClick(findChild(pw, "primaryButton"))
            tryCompare(pw, "visible", false)
            const d = openedDialog(page, "exportSelection")
            const args = waitExport(page, () => mouseClick(findChild(d, "primaryButton")))
            verify(args[0], args[1])
            compare(testContext.readNames(page.fileOverride, "newpass1").length, 3)
            compare(testContext.readNames(page.fileOverride, testContext.password()).length, 0)
            compare(testContext.readNames(pathA, testContext.password()).length, 3) // original unchanged
        }

        function test_backup() {
            const page = createPage()
            page.fileOverride = testContext.tempPath("backup.epdb")
            const args = waitExport(page, () => click(page, "backupButton"))
            verify(args[0], args[1])
            compare(testContext.readNames(page.fileOverride, testContext.password()).length, 3)
            verify(findChild(page, "pageInfo").open)
        }

        function test_exportToLoadedFileRefused() {
            const page = createPage()
            page.fileOverride = pathA
            const args = waitExport(page, () => click(page, "backupButton"))
            verify(!args[0])
        }

        // --- import into ------------------------------------------------------------------------
        function startImport(page, path) {
            page.fileOverride = path
            click(page, "importIntoButton")
            return openedDialog(page, "importPasswordDialog")
        }

        function test_importIntoAppend() {
            const page = createPage()
            const pw = startImport(page, pathB)
            typeInto(findChild(pw, "promptPassword"), "wrong")
            mouseClick(findChild(pw, "primaryButton"))
            tryVerify(() => findChild(pw, "promptError").open, 10000)
            verify(pw.visible)
            typeInto(findChild(pw, "promptPassword"), testContext.password())
            mouseClick(findChild(pw, "primaryButton"))
            tryCompare(pw, "visible", false, 10000)
            const d = openedDialog(page, "importSelection")
            compare(findChild(d, "selectionCount").text, "3 / 3")
            tryVerify(() => d.itemAt(0) !== null)
            mouseClick(d.itemAt(0))                              // skip "Banking"
            mouseClick(findChild(d, "primaryButton"))
            tryCompare(d, "visible", false)
            compare(testContext.itemCount, 5)
            compare(testContext.nameOf(3), "amazon")
            compare(testContext.nameOf(4), "Cloud")
        }

        function test_importIntoOverrideNeedsConfirmation() {
            const page = createPage()
            const pw = startImport(page, pathB)
            typeInto(findChild(pw, "promptPassword"), testContext.password())
            mouseClick(findChild(pw, "primaryButton"))
            const d = openedDialog(page, "importSelection")
            tryVerify(() => d.itemAt(2) !== null)
            mouseClick(d.itemAt(2))                              // only Banking + amazon
            const override = findChild(d, "secondaryButton")
            verify(override.visible)
            mouseClick(override)
            verify(d.visible)                                    // first click shows the check box
            const confirm = findChild(d, "confirmOverride")
            verify(confirm.visible)
            mouseClick(override)
            verify(d.visible)                                    // still unchecked
            compare(testContext.itemCount, 3)
            mouseClick(confirm)
            mouseClick(override)
            tryCompare(d, "visible", false)
            compare(testContext.itemCount, 2)
            compare(testContext.nameOf(0), "Banking")
            compare(testContext.nameOf(1), "amazon")
        }

        function test_importAsksForSecondFactor() {
            const pathC = testContext.createSampleFile("Gamma", "123456789012")
            verify(testContext.openFile(pathA))
            const page = createPage()
            const pw = startImport(page, pathC)
            verify(!findChild(pw, "promptSecondFactor").visible)
            typeInto(findChild(pw, "promptPassword"), testContext.password())
            mouseClick(findChild(pw, "primaryButton"))
            tryVerify(() => findChild(pw, "promptSecondFactor").visible, 10000)
            verify(findChild(pw, "promptError").open)
            typeInto(findChild(pw, "promptSecondFactor"), "123456789012")
            mouseClick(findChild(pw, "primaryButton"))
            tryCompare(pw, "visible", false, 10000)
            const d = openedDialog(page, "importSelection")
            mouseClick(findChild(d, "closeButton"))              // cancel imports nothing
            tryCompare(d, "visible", false)
            compare(testContext.itemCount, 3)
            compare(testContext.databaseManagement.importCandidates.length, 0)
        }

        // --- read-only / temporary --------------------------------------------------------
        function test_readOnlyDisablesChanges() {
            verify(testContext.openSample())
            testContext.setReadOnly(true)
            const page = createPage()
            tryVerify(() => page.selectedLoaded)
            verify(!findChild(page, "changePasswordButton").enabled)
            verify(!findChild(page, "secondFactorButton").enabled)
            verify(!findChild(page, "importIntoButton").enabled)
            verify(findChild(page, "exportButton").enabled)      // reading is fine
            verify(findChild(page, "backupButton").enabled)
            testContext.setReadOnly(false)
        }

        function test_temporaryDatabaseCanBeKept() {
            const pathT = testContext.createSampleFile("Temp")
            testContext.setKnownPaths([pathA])
            verify(testContext.openTemporary(pathT))
            const page = createPage()
            verify(card(page, "Temp") !== null)
            compare(page.selectedPath, pathT)
            verify(testContext.databaseManagement.temporary)
            verify(!findChild(page, "changePasswordButton").enabled)
            click(page, "makePermanentButton")
            verify(!testContext.databaseManagement.temporary)
            verify(testContext.knownPaths().indexOf(pathT) >= 0)
            verify(findChild(page, "changePasswordButton").enabled)
            verify(!findChild(page, "makePermanentButton").visible)
        }

        // --- print -------------------------------------------------------------------------
        function test_printDialogWarnsAndExportsPdf() {
            const page = createPage()
            page.fileOverride = testContext.tempPath("print.pdf")
            click(page, "printButton")
            const d = openedDialog(page, "printDialog")
            verify(!findChild(d, "plaintextWarning").visible)
            mouseClick(findChild(d, "includePasswords"))
            verify(findChild(d, "plaintextWarning").visible)
            mouseClick(findChild(d, "secondaryButton"))          // Export PDF
            tryCompare(d, "visible", false)
            verify(testContext.fileExists(page.fileOverride))
            verify(findChild(page, "pageInfo").open)
        }
    }
}
