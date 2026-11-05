import QtQuick
import QtQuick.Controls
import QtTest
import EasePass

Item {
    id: top
    width: 900; height: 800

    Component { id: fakeC; FakeDatabaseController { knownDatabases: []; lastDatabase: "" } }
    Component { id: pageC; RegisterPage { anchors.fill: parent } }
    Component { id: tokenC; OneTimeTokenDialog { parent: top } }
    Component { id: spyC; SignalSpy {} }

    TestCase {
        name: "RegisterPage"
        when: windowShown
        function init() { failOnWarning(/.?/) }

        function makePage() {
            const ctl = createTemporaryObject(fakeC, top)
            const page = createTemporaryObject(pageC, top, { controller: ctl })
            waitForRendering(page)
            return { ctl: ctl, page: page, pw: findChild(page, "registerPassword"),
                     repeat: findChild(page, "registerRepeat"), button: findChild(page, "registerButton") }
        }

        function test_registerWithEnterKeys() {
            const t = makePage()
            verify(t.pw.activeFocus)
            verify(!t.button.enabled)
            keyClick(Qt.Key_A); keyClick(Qt.Key_B); keyClick(Qt.Key_C); keyClick(Qt.Key_D)
            keyClick(Qt.Key_Return)                       // Enter moves to the repeat box
            verify(t.repeat.activeFocus)
            keyClick(Qt.Key_A); keyClick(Qt.Key_B); keyClick(Qt.Key_C); keyClick(Qt.Key_D)
            keyClick(Qt.Key_Return)                       // Enter registers
            verify(t.ctl.createAndOpenUsed)
            compare(t.ctl.lastCreate[0], t.ctl.defaultDatabasePath())
            verify(t.ctl.isOpen)
        }

        function test_validationErrorShown() {
            const t = makePage()
            t.pw.text = "ab"
            t.repeat.text = "ab"
            mouseClick(t.button)
            const err = findChild(t.page, "errorBar")
            verify(err.visible)
            compare(err.message, "Password too short")
            verify(!t.ctl.isOpen)
            t.pw.text = "abcd"
            t.repeat.text = "abce"
            t.page.register()
            compare(err.message, "Passwords do not match")
        }

        function test_advancedAndImport() {
            const t = makePage()
            mouseClick(findChild(t.page, "advancedLink"))
            const dlg = findChild(t.page, "createDialog")
            tryCompare(dlg, "opened", true)
            verify(dlg.openAfterCreate)
            dlg.close()
            tryCompare(dlg, "visible", false)

            const finished = createTemporaryObject(spyC, top, { target: t.page, signalName: "importFinished" })
            const flow = findChild(t.page, "importFlow")
            flow.askPassword("/tmp/Existing.epdb")
            const pdlg = findChild(flow, "importPasswordDialog")
            tryCompare(pdlg, "opened", true)
            findChild(flow, "importPassword").text = "secret"
            keyClick(Qt.Key_Return)
            tryCompare(finished, "count", 1)
            compare(finished.signalArguments[0][0], "/tmp/Existing.epdb")
        }

        function test_oneTimeTokenDialog() {
            const d = createTemporaryObject(tokenC, top)
            const copy = createTemporaryObject(spyC, top, { target: d, signalName: "copyRequested" })
            d.showToken("123456789012")
            tryCompare(d, "opened", true)
            const box = findChild(d, "tokenBox")
            compare(box.text, "123456789012")             // shown in clear once
            mouseClick(findChild(box, "copyButton"))
            compare(copy.signalArguments[0][0], "123456789012")
            d.clickClose()
            tryCompare(d, "visible", false)
            compare(d.token, "")
        }
    }
}
