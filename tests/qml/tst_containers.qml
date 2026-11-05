import QtQuick
import QtQuick.Controls
import QtTest
import EasePass

Item {
    id: top
    width: 800; height: 600

    Component { id: chartC; PasswordSafetyChart { width: 100; height: 100 } }
    Component { id: infoC; InfoBar { width: 400; title: "Saved"; message: "All good" } }
    Component { id: dialogC; ContentDialog { title: "Delete?"; primaryButtonText: "Delete"
                                              secondaryButtonText: "Later"; closeButtonText: "Cancel"
                                              Label { text: "Body" } } }
    Component { id: barC; CommandBar {
            width: 400
            primaryActions: [ Action { objectName: "addAction"; text: "Add"; icon.name: "add" } ]
            secondaryActions: [ Action { text: "Settings"; icon.name: "settings" },
                                Action { text: "About" } ] } }
    Component { id: flyoutC; FlyoutMenu { MenuItem { text: "One" } } }
    Component { id: splitC; Splitter {
            width: 400; height: 100
            Item { objectName: "left"; SplitView.preferredWidth: 150 }
            Item { SplitView.fillWidth: true } } }
    Component { id: spyC; SignalSpy {} }

    TestCase {
        name: "Containers"
        when: windowShown
        function init() { failOnWarning(/.?/) }

        function test_chart() {
            const c = createTemporaryObject(chartC, top)
            for (let i = 0; i < 8; ++i) compare(c.segmentColor(i), Theme.chartUnknown)
            c.checks = [true, false, true, false, null, true, false, true]
            compare(c.segmentColor(0), Theme.chartGood)
            compare(c.segmentColor(1), Theme.chartBad)
            compare(c.segmentColor(4), Theme.chartUnknown)
            const lines = c.infoText.split("\n")
            compare(lines.length, 8)
            verify(lines[0].endsWith("True"))
            verify(lines[2].endsWith("Meets minimum"))
            verify(lines[3].endsWith("True"))    // leaked check false -> "True" (C# inverted)
            verify(lines[4].endsWith("Unknown"))
            compare(c.Accessible.description, c.infoText)
            // segment hit-testing, clockwise from 12 o'clock
            compare(c.segmentAt(Qt.point(55, 10)), 0)
            compare(c.segmentAt(Qt.point(90, 45)), 1)
            compare(c.segmentAt(Qt.point(90, 55)), 2)
            compare(c.segmentAt(Qt.point(55, 90)), 3)
            compare(c.segmentAt(Qt.point(45, 90)), 7)
            compare(c.segmentAt(Qt.point(10, 55)), 6)
            compare(c.segmentAt(Qt.point(10, 45)), 5)
            compare(c.segmentAt(Qt.point(45, 10)), 4)
            compare(c.messages.length, 8)
        }

        function test_infoBar() {
            const b = createTemporaryObject(infoC, top)
            compare(b.Accessible.role, Accessible.AlertMessage)
            compare(b.Accessible.name, "Saved: All good")
            const closed = createTemporaryObject(spyC, top, { target: b, signalName: "closed" })
            verify(b.visible)
            mouseClick(findChild(b, "closeButton"))
            verify(!b.open && !b.visible)
            compare(closed.count, 1)
            b.close()
            compare(closed.count, 1) // already closed
            b.severity = Theme.Severity.Error
            b.autoCloseInterval = 30
            b.show()
            verify(b.visible)
            tryCompare(b, "open", false, 2000)
            compare(closed.count, 2)
            b.autoCloseInterval = 0
            b.show()
            // managed: close() leaves `open` to the owner binding
            b.managed = true
            b.close()
            compare(closed.count, 3)
            verify(b.open)
            b.managed = false
            b.closable = false
            b.show()
            verify(!findChild(b, "closeButton").visible)
        }

        function test_contentDialog() {
            const d = createTemporaryObject(dialogC, top)
            const prim = createTemporaryObject(spyC, top, { target: d, signalName: "primaryClicked" })
            const sec = createTemporaryObject(spyC, top, { target: d, signalName: "secondaryClicked" })
            const cls = createTemporaryObject(spyC, top, { target: d, signalName: "closeClicked" })
            // Popup is no Item: QQuickDialog exposes role Dialog + title to a11y itself
            d.open(); tryCompare(d, "opened", true)
            mouseClick(findChild(d, "primaryButton"))
            compare(prim.count, 1)
            compare(d.dialogResult, ContentDialog.Result.Primary)
            tryCompare(d, "visible", false)

            d.open(); tryCompare(d, "opened", true)
            compare(d.dialogResult, ContentDialog.Result.None)
            mouseClick(findChild(d, "secondaryButton"))
            compare(sec.count, 1)
            tryCompare(d, "visible", false)

            d.open(); tryCompare(d, "opened", true)
            keyClick(Qt.Key_Escape)
            compare(cls.count, 1)
            tryCompare(d, "visible", false)

            d.closeOnPrimary = false
            d.open(); tryCompare(d, "opened", true)
            d.clickPrimary()
            verify(d.visible)
            d.primaryButtonEnabled = false
            d.clickPrimary()
            compare(prim.count, 2)
            d.secondaryButtonText = ""
            verify(!findChild(d, "secondaryButton").visible)
            d.clickClose()
            tryCompare(d, "visible", false)
        }

        function test_commandBar() {
            const b = createTemporaryObject(barC, top)
            const add = b.primaryActions[0]
            const trig = createTemporaryObject(spyC, top, { target: add, signalName: "triggered" })
            const more = findChild(b, "moreButton")
            verify(more.visible)
            compare(more.Accessible.name, "More options")
            mouseClick(more)
            const menu = findChild(b, "overflowMenu")
            tryCompare(menu, "opened", true)
            compare(menu.count, 2)
            menu.close()
            tryCompare(menu, "visible", false)
            add.trigger()
            compare(trig.count, 1)
        }

        function test_flyoutAndSplitter() {
            const f = createTemporaryObject(flyoutC, top)
            const anchor = createTemporaryObject(chartC, top, { x: 50, y: 40, width: 20, height: 20 })
            f.openAt(anchor)
            tryCompare(f, "opened", true)
            compare(f.y, 60)
            f.close()
            const s = createTemporaryObject(splitC, top)
            compare(s.count, 2)
            compare(findChild(s, "left").width, 150)
        }
    }
}
