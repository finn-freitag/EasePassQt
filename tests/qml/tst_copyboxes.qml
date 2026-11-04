import QtQuick
import QtQuick.Controls
import QtTest
import EasePass

Item {
    id: top
    width: 600; height: 400

    Component { id: copyTextC; CopyTextbox { text: "hello"; accessibleName: "Email"; width: 300 } }
    Component { id: copyPwC; CopyPasswordbox { password: "s3cret"; width: 350 } }
    Component { id: tokenC; CopyTokenBox { token: "123456"; width: 350 } }
    Component { id: editPwC; EditPasswordBox { width: 300 } }

    TestCase {
        name: "CopyBoxes"
        when: windowShown
        function init() { failOnWarning(/.?/) }

        SignalSpy { id: spy; signalName: "copyRequested" }

        function test_copyTextbox() {
            const t = createTemporaryObject(copyTextC, top)
            compare(t.Accessible.name, "Email")
            verify(t.readOnly)
            spy.target = t; spy.clear()
            mouseClick(findChild(t, "copyButton"))
            compare(spy.count, 1)
            compare(spy.signalArguments[0][0], "hello")
            t.forceActiveFocus()
            keySequence(StandardKey.Copy)
            compare(spy.count, 2)
            mouseDoubleClickSequence(t, 20, t.height / 2)
            compare(spy.count, 3)
            t.doubleClickToCopy = false
            mouseDoubleClickSequence(t, 20, t.height / 2)
            compare(spy.count, 3)
        }

        function test_copyPasswordbox() {
            const p = createTemporaryObject(copyPwC, top)
            compare(p.text, "••••••")
            verify(p.Accessible.passwordEdit)
            const show = findChild(p, "showButton")
            mouseClick(show)
            verify(p.showPassword)
            compare(p.text, "s3cret")
            mouseClick(show)
            compare(p.text.length, 6)
            verify(p.text !== "s3cret")
            spy.target = p; spy.clear()
            mouseClick(findChild(p, "copyButton"))
            compare(spy.signalArguments[0][0], "s3cret")
            p.forceActiveFocus()
            keySequence(StandardKey.Copy)
            compare(spy.count, 2)
            compare(spy.signalArguments[1][0], "s3cret") // copies the password, not the mask
            p.safetyChecks = [true, true, true, true, true, true, true, true]
            compare(findChild(p, "safetyChart").segmentColor(0), Theme.chartGood)
            p.showSafetyChart = false
            verify(!findChild(p, "safetyChart").visible)
        }

        function test_copyTokenBox() {
            const t = createTemporaryObject(tokenC, top)
            compare(t.text, "******")
            mouseClick(findChild(t, "showButton"))
            compare(t.text, "123456")
            spy.target = t; spy.clear()
            mouseClick(findChild(t, "copyButton"))
            compare(spy.signalArguments[0][0], "123456")
            t.progress = 1.7
            compare(findChild(t, "progressRing").value, 1)
            t.progress = -1
            compare(findChild(t, "progressRing").value, 0)
        }

        function test_editPasswordBox() {
            const e = createTemporaryObject(editPwC, top)
            compare(e.echoMode, TextInput.Password)
            compare(e.Accessible.name, "Password")
            const changed = createTemporaryObject(spyC, top, { target: e, signalName: "passwordChanged" })
            e.forceActiveFocus()
            keyClick(Qt.Key_X)
            keyClick(Qt.Key_Y)
            compare(e.password, "xy")
            compare(changed.count, 2)
            mouseClick(findChild(e, "revealButton"))
            compare(e.echoMode, TextInput.Normal)
            spy.target = e; spy.clear()
            e.forceActiveFocus()
            keySequence(StandardKey.Copy)
            compare(spy.signalArguments[0][0], "xy")
        }
    }
    Component { id: spyC; SignalSpy {} }
}
