import QtQuick
import QtQuick.Controls
import QtTest
import EasePass

// Inactivity logout (steps.txt T19) with the real AutoLogoutService / InactivityManager driven
// by the fake clock of testContext (inactivity logout set to 1 minute, 30 s warning).
Item {
    id: top
    width: 800; height: 600

    Component {
        id: dialogC
        AutoLogoutDialog {
            autoLogout: testContext.autoLogout
            controller: testContext.controller
        }
    }

    TestCase {
        id: tc
        name: "AutoLogout"
        when: windowShown

        function init() {
            testContext.settings.inactivityLogoutMinutes = 1
            verify(testContext.openSample())
            verify(testContext.autoLogout.enabled)
            failOnWarning(/.?/)
        }
        function cleanup() {
            testContext.settings.inactivityLogoutMinutes = 3
        }

        function createDialog() {
            const d = createTemporaryObject(dialogC, top)
            verify(d)
            return d
        }
        function idle(seconds) {
            testContext.advanceClock(seconds)
            testContext.tickInactivity()
        }

        function test_warningThenStay() {
            const d = createDialog()
            idle(20)
            verify(!d.visible)                                   // warning starts 30 s before
            idle(15)                                             // 25 s left
            tryCompare(d, "opened", true)
            compare(testContext.autoLogout.secondsLeft, 25)
            verify(findChild(d, "countdownLabel").text.indexOf("25") >= 0)
            idle(5)
            compare(testContext.autoLogout.secondsLeft, 20)
            compare(findChild(d, "countdownBar").value, 20)
            mouseClick(findChild(d, "primaryButton"))            // Stay logged in
            tryCompare(d, "visible", false)
            verify(testContext.controller.isOpen)
            idle(35)                                             // a full minute again
            verify(testContext.controller.isOpen)
        }

        function test_logoutAfterTimeout() {
            const d = createDialog()
            idle(40)
            tryCompare(d, "opened", true)
            idle(20)
            tryCompare(testContext.controller, "isOpen", false)
            tryCompare(d, "visible", false)
            verify(!testContext.autoLogout.enabled)
        }

        function test_logoutNow() {
            const d = createDialog()
            idle(45)
            tryCompare(d, "opened", true)
            mouseClick(findChild(d, "secondaryButton"))          // Log out now
            tryCompare(testContext.controller, "isOpen", false)
        }

        function test_escapeKeepsSession() {
            const d = createDialog()
            idle(45)
            tryCompare(d, "opened", true)
            keyClick(Qt.Key_Escape)
            tryCompare(d, "visible", false)
            verify(testContext.controller.isOpen)
            verify(!testContext.autoLogout.warningActive)
        }

        function test_preventLogout() {
            testContext.autoLogout.preventLogout(true)
            idle(120)
            verify(testContext.controller.isOpen)
            verify(!testContext.autoLogout.warningActive)
            testContext.autoLogout.preventLogout(false)
            idle(59)
            verify(testContext.controller.isOpen)                // the period restarted
            idle(2)
            tryCompare(testContext.controller, "isOpen", false)
        }

        function test_disabledWhileLocked() {
            testContext.controller.lock()
            tryCompare(testContext.autoLogout, "enabled", false)
            const d = createDialog()
            idle(600)
            verify(!d.visible)
        }
    }
}
