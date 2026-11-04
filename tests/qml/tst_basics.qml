import QtQuick
import QtQuick.Controls
import QtTest
import EasePass

Item {
    id: top
    width: 600; height: 400

    Component { id: iconButtonC; IconButton { iconName: "copy"; toolTip: "Copy it" } }
    Component { id: separatorC; SettingsItemSeparator { text: "General" } }
    Component { id: settingsC; SettingsControl { header: "Header"; description: "Desc"; iconName: "key"
                                                  Switch { objectName: "inner" } } }
    Component { id: labeledC; LabeledTextBox { placeholderText: "Name"; infoLabel: "2 found" } }
    Component { id: tagsC; TagRenderControl { tags: ["work", "email"] } }

    TestCase {
        name: "Basics"
        when: windowShown
        function init() { failOnWarning(/.?/) }

        SignalSpy { id: spy }

        function test_theme() {
            verify(Theme.dark === (Theme.themeMode === Theme.Mode.Dark
                                   || (Theme.themeMode === Theme.Mode.System && Theme.systemDark)))
            const old = Theme.themeMode
            Theme.themeMode = Theme.Mode.Dark
            verify(Theme.dark)
            compare(Theme.background, Qt.color("#202020"))
            Theme.themeMode = Theme.Mode.Light
            verify(!Theme.dark)
            compare(Theme.background, Qt.color("#F3F3F3"))
            Theme.themeMode = old
            verify(Theme.icon("copy").toString().endsWith("icons/copy.svg"))
            verify(Theme.severityForeground(Theme.Severity.Error) !== Theme.severityForeground(Theme.Severity.Success))
            verify(Theme.radius > 0 && Theme.spacing > 0 && Theme.fontBody > 0)
        }

        function test_iconButton() {
            const b = createTemporaryObject(iconButtonC, top)
            verify(b)
            compare(b.Accessible.name, "Copy it")
            compare(b.display, AbstractButton.IconOnly)
            spy.target = b; spy.signalName = "clicked"; spy.clear()
            mouseClick(b)
            compare(spy.count, 1)
            b.forceActiveFocus()
            keyClick(Qt.Key_Space)
            compare(spy.count, 2)
            b.text = "Copy"
            compare(b.display, AbstractButton.TextBesideIcon)
            compare(b.Accessible.name, "Copy")
        }

        function test_separator() {
            const s = createTemporaryObject(separatorC, top)
            compare(s.Accessible.name, "General")
            verify(s.implicitHeight > 0)
        }

        function test_settingsControl() {
            const s = createTemporaryObject(settingsC, top, { width: 400 })
            compare(s.Accessible.name, "Header")
            compare(s.Accessible.description, "Desc")
            verify(findChild(s, "inner"))
            spy.target = s; spy.signalName = "clicked"; spy.clear()
            mouseClick(s, 20, s.height / 2)
            compare(spy.count, 0) // not clickable
            s.clickable = true
            mouseClick(s, 20, s.height / 2)
            compare(spy.count, 1)
            s.forceActiveFocus()
            keyClick(Qt.Key_Return)
            compare(spy.count, 2)
            compare(s.Accessible.role, Accessible.Button)
        }

        function test_labeledTextBox() {
            const t = createTemporaryObject(labeledC, top, { width: 300 })
            compare(t.Accessible.name, "Name")
            t.forceActiveFocus()
            keyClick(Qt.Key_A)
            compare(t.text, "a")
            verify(t.rightPadding > 20)
            t.infoLabel = ""
            verify(t.rightPadding < 20)
        }

        function test_tags() {
            const t = createTemporaryObject(tagsC, top, { width: 300 })
            // MD5("work") = 67e92c87... -> rgb(bytes[1], bytes[0], bytes[2])
            compare(t.tagColor("work"), Qt.rgba(0xe9 / 255, 0x67 / 255, 0x2c / 255, 1))
            compare(t.children.length, 3) // 2 chips + repeater
            t.tags = []
            compare(t.children.length, 1)
            t.tags = ["a", "b", "c"]
            compare(t.children.length, 4)
            compare(t.children[0].Accessible.name, "a")
        }
    }
}
