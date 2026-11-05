import QtQuick
import QtQuick.Controls
import QtTest
import EasePass

Item {
    id: top
    width: 900; height: 800

    Component { id: fakeC; FakeDatabaseController {} }
    Component { id: pageC; LoginPage { anchors.fill: parent } }
    Component { id: spyC; SignalSpy {} }

    TestCase {
        name: "LoginPage"
        when: windowShown
        function init() { failOnWarning(/.?/) }

        function makePage(props) {
            const ctl = createTemporaryObject(fakeC, top, props || {})
            const page = createTemporaryObject(pageC, top, { controller: ctl, dailyTip: "Use long passwords." })
            waitForRendering(page)
            return { ctl: ctl, page: page,
                     pw: findChild(page, "passwordBox"), button: findChild(page, "loginButton"),
                     second: findChild(page, "secondFactorBox"), error: findChild(page, "errorBar") }
        }

        function test_initialState() {
            const t = makePage()
            const box = findChild(t.page, "databaseBox")
            compare(box.count, 2)
            compare(box.currentIndex, 1)                 // last database preselected
            compare(box.displayText, "Work")
            compare(t.page.selectedPath, "/home/u/Work.epdb")
            verify(t.pw.activeFocus)
            verify(!t.button.enabled)                    // empty password
            verify(!t.second.visible)
            verify(!t.error.visible)
        }

        function test_wrongPasswordShowsErrorAndStays() {
            const t = makePage()
            const unlocked = createTemporaryObject(spyC, top, { target: t.page, signalName: "unlocked" })
            t.pw.forceActiveFocus()
            keyClick(Qt.Key_X)
            mouseClick(t.button)
            compare(t.ctl.openCalls, 1)
            compare(t.ctl.lastOpen[0], "/home/u/Work.epdb")
            tryVerify(() => t.error.visible)
            compare(t.error.message, "Wrong password")
            compare(unlocked.count, 0)
            verify(t.pw.activeFocus)                      // ready for the next attempt
            compare(t.pw.text, "x")
            // no attempt limit (D6): many wrong tries keep working
            for (let i = 0; i < 5; ++i) {
                t.page.login()
                tryVerify(() => !t.ctl.busy)
            }
            compare(t.ctl.openCalls, 6)
            findChild(t.error, "closeButton").clicked()
            compare(t.ctl.errorMessage, "")
            verify(!t.error.visible)
            t.page.login()                                // a new error shows the bar again
            tryVerify(() => t.error.visible)
        }

        function test_correctPasswordUnlocks() {
            const t = makePage()
            const unlocked = createTemporaryObject(spyC, top, { target: t.page, signalName: "unlocked" })
            t.pw.text = "secret"
            mouseClick(t.button)
            tryCompare(unlocked, "count", 1)
            compare(t.pw.text, "")                        // password cleared from the UI
        }

        function test_enterKeySubmits() {
            const t = makePage()
            t.pw.forceActiveFocus()
            keyClick(Qt.Key_S); keyClick(Qt.Key_E)
            keyClick(Qt.Key_Return)
            compare(t.ctl.openCalls, 1)
            compare(t.ctl.lastOpen[1], "se")
            tryVerify(() => !t.ctl.busy)
            keyClick(Qt.Key_Enter)
            compare(t.ctl.openCalls, 2)
        }

        function test_buttonDisabledWhileBusy() {
            const t = makePage()
            t.pw.text = "secret"
            verify(t.button.enabled)
            t.page.login()
            verify(t.ctl.busy)
            verify(!t.button.enabled)
            verify(!t.pw.enabled)
            verify(findChild(t.page, "busyIndicator").running)
            verify(!findChild(t.page, "createButton").enabled)
            verify(!t.page.login())                       // ignored while busy
            compare(t.ctl.openCalls, 1)
            tryVerify(() => t.button.enabled || t.ctl.isOpen)
        }

        function test_secondFactorPrompt() {
            const t = makePage({ correctSecondFactor: "123456789012" })
            t.pw.text = "secret"
            t.page.login()
            tryVerify(() => t.second.visible)
            verify(t.second.activeFocus)
            verify(!t.button.enabled)                     // needs the second factor now
            t.second.text = "123456789012"
            verify(t.button.enabled)
            const unlocked = createTemporaryObject(spyC, top, { target: t.page, signalName: "unlocked" })
            mouseClick(t.button)
            compare(t.ctl.lastOpen[2], "123456789012")
            tryCompare(unlocked, "count", 1)
        }

        function test_dailyTip() {
            const t = makePage()
            const tip = findChild(t.page, "dailyTip")
            verify(tip.visible)
            compare(findChild(tip, "tipText").text, "Use long passwords.")
            t.page.dailyTip = ""
            verify(!tip.visible)
        }

        function test_createDatabaseDialog() {
            const t = makePage()
            mouseClick(findChild(t.page, "createButton"))
            const dlg = findChild(t.page, "createDialog")
            tryCompare(dlg, "opened", true)
            compare(dlg.folder, "/home/u/.local/share/EasePass")
            verify(!dlg.primaryButtonEnabled)
            findChild(dlg, "databaseName").text = "Family"
            findChild(dlg, "createPassword").text = "abcd"
            findChild(dlg, "createRepeat").text = "abce"
            dlg.clickPrimary()
            compare(t.ctl.errorMessage, "Passwords do not match")
            verify(dlg.visible)                           // stays open on errors
            findChild(dlg, "createRepeat").text = "abcd"
            dlg.clickPrimary()
            compare(t.ctl.lastCreate[0], "/home/u/.local/share/EasePass/Family.epdb")
            verify(!t.ctl.createAndOpenUsed)
            tryCompare(dlg, "visible", false)
            compare(findChild(t.page, "databaseBox").displayText, "Family")
        }

        function test_importFlow() {
            const t = makePage()
            const flow = findChild(t.page, "importFlow")
            flow.askPassword("file:///tmp/Old.epdb")      // as if picked in the FileDialog
            const dlg = findChild(flow, "importPasswordDialog")
            tryCompare(dlg, "opened", true)
            compare(dlg.title, "Enter the password of Old")
            findChild(flow, "importPassword").text = "nope"
            dlg.clickPrimary()
            compare(t.ctl.lastImport[0], "/tmp/Old.epdb")
            verify(dlg.visible)
            findChild(flow, "importPassword").text = "secret"
            dlg.clickPrimary()
            tryCompare(dlg, "visible", false)
            compare(findChild(t.page, "databaseBox").displayText, "Old")
        }
    }
}
