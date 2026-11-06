import QtQuick
import QtQuick.Controls
import QtTest
import EasePass

// Item dialogs (steps.txt T15) driven by the real App layer (testContext: AppContext with
// fakes: clipboard, screen capture, webcam, no network). Sample items (database order):
// 0 Banking (zoe, zoe@bank.example, strong pw, no 2FA), 1 amazon (alice, pw "weak", 2FA),
// 2 Cloud (bob, pw "medium123").
Item {
    id: top
    width: 1100; height: 800

    readonly property string secret: "JBSWY3DPEHPK3PXP"
    readonly property string secret2: "GEZDGNBVGY3TQOJQ"

    Component {
        id: itemDialogC
        ItemEditDialog {
            editor: testContext.editor
            generator: testContext.generator
        }
    }
    Component {
        id: add2FAC
        Add2FADialog {
            editor: testContext.editor
            qrImport: testContext.qrImport
            screenScan: testContext.screenScan
            webcamScan: testContext.webcamScan
        }
    }
    Component {
        id: generatorC
        PasswordGeneratorDialog { generator: testContext.generator }
    }
    Component { id: spyC; SignalSpy {} }

    TestCase {
        id: tc
        name: "ItemDialogs"
        when: windowShown

        function init() {
            verify(testContext.openSample())
            testContext.clearClipboard()
            testContext.setClipboardText("")
            testContext.setGeneratorSettings(15, "")
            failOnWarning(/.?/)
        }
        function cleanup() {
            testContext.editor.unload()
        }

        // --- helpers ----------------------------------------------------------------
        function typeInto(field, text) {
            field.forceActiveFocus()
            field.selectAll()
            keyClick(Qt.Key_Delete)
            for (const ch of text)
                keyClick(ch)
        }
        // Scrolls the item dialog so that `it` can be clicked.
        function reveal(d, it) {
            const flick = findChild(d, "itemScroll").contentItem
            const p = it.mapToItem(flick.contentItem, 0, 0)
            flick.contentY = Math.max(0, Math.min(p.y - 10, flick.contentHeight - flick.height))
            wait(0)
        }
        function openItemDialog(index) {
            const d = createTemporaryObject(itemDialogC, top)
            if (index === undefined) d.openNew()
            else verify(d.openEdit(index))
            tryCompare(d, "opened", true)
            return d
        }
        function openAdd2FA(index) {
            const d = createTemporaryObject(add2FAC, top)
            verify(d.openFor(index))
            tryCompare(d, "opened", true)
            return d
        }

        // --- add / edit -------------------------------------------------------------
        function test_addItemSavesFields() {
            const d = openItemDialog()
            const spy = createTemporaryObject(spyC, top, { target: d, signalName: "committed" })
            compare(d.title, "Add Password")
            compare(findChild(d, "primaryButton").text, "Add")
            verify(!findChild(d, "twoFactorFields").visible)   // C#: no 2FA section when adding
            typeInto(findChild(d, "nameBox"), "Mail")
            typeInto(findChild(d, "usernameBox"), "max")
            typeInto(findChild(d, "passwordBox"), "Abc123xyz")
            typeInto(findChild(d, "websiteBox"), "mail.example")
            typeInto(findChild(d, "tagsBox"), "work home")
            findChild(d, "notesBox").forceActiveFocus()
            keyClick("n"); keyClick("b")
            mouseClick(findChild(d, "primaryButton"))
            tryCompare(d, "visible", false)
            compare(testContext.itemCount, 4)
            compare(spy.count, 1)
            compare(spy.signalArguments[0][0], 3)
            const it = testContext.itemAt(3)
            compare(it.DisplayName, "Mail")
            compare(it.Username, "max")
            compare(it.Password, "Abc123xyz")
            compare(it.Website, "mail.example")
            compare(it.Notes, "nb")
            compare(it.Tags, ["work", "home"])
            compare(it.Secret, "")
            verify(!testContext.editor.hasItem)                 // buffer released on close
        }

        function test_addRequiresName() {
            const d = openItemDialog()
            typeInto(findChild(d, "passwordBox"), "secret")
            mouseClick(findChild(d, "primaryButton"))
            verify(d.visible)
            verify(findChild(d, "errorBar").open)
            verify(findChild(d, "errorBar").message.indexOf("Name") >= 0)
            compare(testContext.itemCount, 3)
        }

        function test_editCancelDiscards() {
            const d = openItemDialog(0)
            compare(d.title, "Edit item")
            compare(findChild(d, "primaryButton").text, "Done")
            compare(findChild(d, "nameBox").text, "Banking")
            compare(findChild(d, "emailBox").text, "zoe@bank.example")
            compare(findChild(d, "tagsBox").text, "money")
            typeInto(findChild(d, "nameBox"), "Changed")
            compare(testContext.editor.displayName, "Changed")
            mouseClick(findChild(d, "closeButton"))
            tryCompare(d, "visible", false)
            compare(testContext.nameOf(0), "Banking")
            verify(!testContext.editor.hasItem)
            // Opening again shows the stored values, not the discarded edit.
            verify(d.openEdit(0))
            tryCompare(d, "opened", true)
            compare(findChild(d, "nameBox").text, "Banking")
        }

        function test_editSavesOnlyChangedItem() {
            const d = openItemDialog(2)
            const spy = createTemporaryObject(spyC, top, { target: d, signalName: "committed" })
            compare(findChild(d, "usernameBox").text, "bob")
            typeInto(findChild(d, "usernameBox"), "robert")
            mouseClick(findChild(d, "primaryButton"))
            tryCompare(d, "visible", false)
            compare(spy.count, 1)
            compare(spy.signalArguments[0][0], 2)
            compare(testContext.itemAt(2).Username, "robert")
            compare(testContext.itemAt(2).DisplayName, "Cloud")
            compare(testContext.itemAt(0).Username, "zoe")
            compare(testContext.itemCount, 3)
        }

        function test_emailSuggestions() {
            const d = openItemDialog(2)
            verify(findChild(d, "emailBox").suggestionItems.indexOf("zoe@bank.example") >= 0)
        }

        function test_passwordReuseWarning() {
            const d = openItemDialog()
            const warning = findChild(d, "reuseWarning")
            typeInto(findChild(d, "passwordBox"), "weak")       // amazon's password
            verify(warning.visible)
            verify(findChild(d, "reuseBorder").visible)
            typeInto(findChild(d, "passwordBox"), "unique1")
            verify(!warning.visible)
            mouseClick(findChild(d, "closeButton"))
            tryCompare(d, "visible", false)
            verify(d.openEdit(1))                                // own password does not count
            tryCompare(d, "opened", true)
            verify(!findChild(d, "reuseWarning").visible)
        }

        function test_generateButtonUsesSettings() {
            testContext.setGeneratorSettings(20, "abcdefXYZ123!")
            const d = openItemDialog()
            mouseClick(findChild(d, "generatePasswordButton"))
            const box = findChild(d, "passwordBox")
            tryCompare(box.text, "length", 20)
            compare(testContext.editor.password, box.text)
            verify(/^[abcdefXYZ123!]+$/.test(box.text), box.text)
            verify(!findChild(d, "generateProgress").visible)
        }

        // --- 2FA in the edit dialog -------------------------------------------------
        function test_twoFactorSectionOnlyWith2FA() {
            let d = openItemDialog(0)
            verify(!findChild(d, "twoFactorFields").visible)
            verify(!findChild(d, "export2FAButton").visible)
            mouseClick(findChild(d, "closeButton"))
            tryCompare(d, "visible", false)
            verify(d.openEdit(1))
            tryCompare(d, "opened", true)
            verify(findChild(d, "twoFactorFields").visible)
            compare(findChild(d, "secretBox").text, top.secret)
            compare(findChild(d, "digitsBox").text, "6")
            compare(findChild(d, "algorithmBox").currentText, "SHA1")
        }

        function test_export2FAShowsQrCode() {
            const d = openItemDialog(1)
            const button = findChild(d, "export2FAButton")
            reveal(d, button)
            mouseClick(button)
            const img = findChild(d, "qrCodeImage")
            verify(img.visible)
            tryCompare(img, "status", Image.Ready)
            verify(img.sourceSize.width > 50)
            verify(testContext.editor.otpauthUri().indexOf("secret=" + top.secret) > 0)
        }

        function test_remove2FAAsksAndSaves() {
            const d = openItemDialog(1)
            const remove = findChild(d, "remove2FAButton")
            reveal(d, remove)
            mouseClick(remove)
            const confirm = findChild(d, "confirmRemove2FAButton")
            tryVerify(() => confirm.visible) // T24: was verify(), flaky under load
            compare(testContext.itemAt(1).Secret, top.secret)    // nothing changed yet
            reveal(d, confirm)
            mouseClick(confirm)
            tryVerify(() => !findChild(d, "twoFactorFields").visible)
            mouseClick(findChild(d, "primaryButton"))
            tryCompare(d, "visible", false)
            compare(testContext.itemAt(1).Secret, "")
        }

        function test_invalidSecretBlocksSave() {
            const d = openItemDialog(1)
            typeInto(findChild(d, "secretBox"), "0189")          // not Base32
            mouseClick(findChild(d, "primaryButton"))
            verify(d.visible)
            verify(findChild(d, "errorBar").open)
            compare(testContext.itemAt(1).Secret, top.secret)
        }

        // --- Add 2FA dialog ---------------------------------------------------------
        function test_add2FAManual() {
            const d = openAdd2FA(0)
            verify(d.title.indexOf("Banking") >= 0)
            verify(!findChild(d, "primaryButton").enabled)        // no secret yet
            compare(findChild(d, "digitsBox").text, "6")
            compare(findChild(d, "intervalBox").text, "30")
            typeInto(findChild(d, "secretBox"), top.secret)
            typeInto(findChild(d, "digitsBox"), "8")
            verify(findChild(d, "primaryButton").enabled)
            mouseClick(findChild(d, "primaryButton"))
            tryCompare(d, "visible", false)
            compare(testContext.itemAt(0).Secret, top.secret)
            compare(testContext.itemAt(0).Digits, "8")
            compare(testContext.itemAt(0).Interval, "30")
        }

        function test_add2FADigitsOnly() {
            const d = openAdd2FA(0)
            typeInto(findChild(d, "intervalBox"), "4a5")
            compare(findChild(d, "intervalBox").text, "45")
        }

        function test_add2FAInvalidSecretKeepsDialog() {
            const d = openAdd2FA(0)
            typeInto(findChild(d, "secretBox"), "0189")
            mouseClick(findChild(d, "primaryButton"))
            verify(d.visible)
            verify(findChild(d, "messageBar").open)
            compare(findChild(d, "messageBar").severity, Theme.Severity.Error)
            compare(testContext.itemAt(0).Secret, "")
        }

        function test_add2FACancelDiscards() {
            const d = openAdd2FA(0)
            typeInto(findChild(d, "secretBox"), top.secret)
            mouseClick(findChild(d, "closeButton"))
            tryCompare(d, "visible", false)
            compare(testContext.itemAt(0).Secret, "")
        }

        function verifyScanned(d, secret) {
            tryCompare(findChild(d, "secretBox"), "text", secret)
            compare(findChild(d, "digitsBox").text, "8")
            compare(findChild(d, "intervalBox").text, "60")
            compare(findChild(d, "algorithmBox").currentText, "SHA256")
            compare(findChild(d, "messageBar").severity, Theme.Severity.Success)
        }

        function test_add2FAFromClipboardText() {
            const d = openAdd2FA(0)
            testContext.setClipboardText(testContext.otpauthFor(top.secret, "Bank"))
            mouseClick(findChild(d, "clipboardButton"))
            verifyScanned(d, top.secret)
            mouseClick(findChild(d, "primaryButton"))
            tryCompare(d, "visible", false)
            compare(testContext.itemAt(0).Algorithm, "SHA256")
        }

        function test_add2FAFromClipboardImage() {
            const d = openAdd2FA(0)
            testContext.setClipboardQr(testContext.otpauthFor(top.secret, "Bank"))
            mouseClick(findChild(d, "clipboardButton"))
            verifyScanned(d, top.secret)
        }

        function test_add2FAClipboardWithoutCode() {
            const d = openAdd2FA(0)
            testContext.setClipboardText("hello")
            mouseClick(findChild(d, "clipboardButton"))
            verify(findChild(d, "messageBar").open)
            compare(findChild(d, "messageBar").severity, Theme.Severity.Error)
            compare(findChild(d, "secretBox").text, "")
        }

        function test_add2FAFromFile() {
            const d = openAdd2FA(0)
            // The FileDialog is native; its result goes straight to QrImport.scanFile.
            testContext.qrImport.scanFile(testContext.writeQrFile(testContext.otpauthFor(top.secret, "Bank")))
            verifyScanned(d, top.secret)
            testContext.qrImport.scanFile(testContext.writeQrFile("https://not-a-2fa-code.example"))
            compare(findChild(d, "messageBar").severity, Theme.Severity.Error)
            compare(findChild(d, "secretBox").text, top.secret)  // unchanged
        }

        function test_add2FAFromScreen() {
            const d = openAdd2FA(0)
            testContext.setScreenQrCodes([testContext.otpauthFor(top.secret, "Bank")])
            mouseClick(findChild(d, "screenButton"))
            verifyScanned(d, top.secret)
        }

        function test_add2FAFromScreenSeveralCodes() {
            const d = openAdd2FA(0)
            testContext.setScreenQrCodes([testContext.otpauthFor(top.secret, "Bank"),
                                          testContext.otpauthFor(top.secret2, "Mail")])
            mouseClick(findChild(d, "screenButton"))
            const rep = findChild(d, "candidateRepeater")
            tryCompare(rep, "count", 2)
            compare(findChild(d, "messageBar").severity, Theme.Severity.Info)
            const texts = [rep.itemAt(0).text, rep.itemAt(1).text]
            verify(texts.indexOf("Mail (me@example.com)") >= 0, texts)
            const mail = rep.itemAt(texts.indexOf("Mail (me@example.com)"))
            mouseClick(mail)
            verifyScanned(d, top.secret2)
            compare(d.candidates.length, 0)
        }

        function test_add2FAFromScreenNoCode() {
            const d = openAdd2FA(0)
            testContext.setScreenQrCodes([])
            mouseClick(findChild(d, "screenButton"))
            tryCompare(findChild(d, "messageBar"), "open", true)
            compare(findChild(d, "messageBar").severity, Theme.Severity.Warning)
            compare(findChild(d, "secretBox").text, "")
        }

        function test_add2FAFromWebcam() {
            const d = openAdd2FA(0)
            mouseClick(findChild(d, "webcamButton"))
            const cam = findChild(d, "webcamDialog")
            tryCompare(cam, "opened", true)
            verify(testContext.webcamActive())
            compare(testContext.webcamScan.deviceNames, ["Front", "USB"])
            compare(testContext.webcamScan.deviceIndex, 1)        // the default camera
            testContext.webcamDetect(testContext.otpauthFor(top.secret, "Bank"))
            tryCompare(cam, "visible", false)
            verify(!testContext.webcamActive())
            verifyScanned(d, top.secret)
        }

        function test_webcamCancelStopsCamera() {
            const d = openAdd2FA(0)
            mouseClick(findChild(d, "webcamButton"))
            const cam = findChild(d, "webcamDialog")
            tryCompare(cam, "opened", true)
            verify(testContext.webcamActive())
            mouseClick(findChild(cam, "closeButton"))
            tryCompare(cam, "visible", false)
            verify(!testContext.webcamActive())
            verify(d.visible)
        }

        // --- Generator dialog -------------------------------------------------------
        function test_generatorDialog() {
            testContext.setGeneratorSettings(12, "abcXYZ789-")
            const d = createTemporaryObject(generatorC, top)
            d.open()
            tryCompare(d, "opened", true)
            const box = findChild(d, "generatedPassword")
            tryCompare(box.text, "length", 12)
            verify(/^[abcXYZ789-]+$/.test(box.text), box.text)
            const checks = findChild(d, "generatorSafetyChart").checks
            compare(checks[2], true)                               // length >= setting
            verify(checks[3] !== true && checks[3] !== false)      // unknown: leak check off in tests
            const first = box.text
            mouseClick(findChild(d, "primaryButton"))              // "New" keeps the dialog open
            verify(d.visible)
            tryVerify(() => box.text !== first && box.text.length === 12)
            const copySpy = createTemporaryObject(spyC, top, { target: d, signalName: "copyRequested" })
            mouseClick(findChild(d, "copyGeneratedButton"))
            compare(copySpy.count, 1)
            compare(copySpy.signalArguments[0][0], box.text)
            mouseClick(findChild(d, "closeButton"))
            tryCompare(d, "visible", false)
            compare(testContext.generator.password, "")
        }
    }
}
