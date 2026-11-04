import QtQuick
import QtQuick.Controls
import QtTest
import EasePass

Item {
    id: top
    width: 600; height: 500

    Component { id: searchC; SearchPasswordsBox { width: 300; infoLabel: "1 / 3" } }
    Component { id: suggestC; TextWithSuggestionBox { width: 300; suggestionItems: ["alice", "bob", "Alicia", "malice"] } }
    Component { id: spyC; SignalSpy {} }

    TestCase {
        name: "Suggestions"
        when: windowShown
        function init() { failOnWarning(/.?/) }

        function test_textWithSuggestionBox() {
            const b = createTemporaryObject(suggestC, top)
            const textSpy = createTemporaryObject(spyC, top, { target: b, signalName: "userTextChanged" })
            const chosen = createTemporaryObject(spyC, top, { target: b, signalName: "suggestionChosen" })
            b.forceActiveFocus()
            keyClick(Qt.Key_A); keyClick(Qt.Key_L)
            compare(textSpy.count, 2)
            verify(textSpy.signalArguments[1][0]) // user change
            compare(b.suggestions, ["alice", "Alicia", "malice"])
            tryVerify(() => b.suggestionsOpen)
            keyClick(Qt.Key_Down); keyClick(Qt.Key_Down)
            compare(b.highlightedIndex, 1)
            keyClick(Qt.Key_Up)
            compare(b.highlightedIndex, 0)
            keyClick(Qt.Key_Return)
            compare(chosen.count, 1)
            compare(chosen.signalArguments[0][1], 0)
            compare(b.text, "alice")
            verify(!textSpy.signalArguments[textSpy.count - 1][0]) // programmatic change
            tryVerify(() => !b.suggestionsOpen)
            compare(b.suggestions, [])
            // exact match is not suggested again; Escape closes
            b.setTextProgrammatically("")
            keyClick(Qt.Key_B); keyClick(Qt.Key_O); keyClick(Qt.Key_B)
            compare(b.text, "bob")
            compare(b.suggestions, [])
            b.setTextProgrammatically("")
            keyClick(Qt.Key_Backspace)
            keyClick(Qt.Key_I)
            tryVerify(() => b.suggestionsOpen)
            keyClick(Qt.Key_Escape)
            tryVerify(() => !b.suggestionsOpen)
            compare(b.filterSuggestions(""), [])
        }

        function test_searchPasswordsBox() {
            const s = createTemporaryObject(searchC, top)
            compare(s.Accessible.name, "Search passwords")
            const keys = createTemporaryObject(spyC, top, { target: s, signalName: "searchKeyPressed" })
            s.forceActiveFocus()
            keyClick(Qt.Key_Slash)
            verify(s.isTagSearch)
            compare(keys.count, 1)
            verify(!keys.signalArguments[0][0]) // reported before the text changes (C# PreviewKeyDown)
            keyClick(Qt.Key_W)
            verify(keys.signalArguments[1][0])
            s.suggestions = [{ text: "/work" }, { text: "/web" }]
            tryVerify(() => s.suggestionsOpen)
            const chosen = createTemporaryObject(spyC, top, { target: s, signalName: "suggestionChosen" })
            const list = findChild(s, "suggestionList")
            verify(list)
            tryVerify(() => list.count === 2)
            keyClick(Qt.Key_Down); keyClick(Qt.Key_Down); keyClick(Qt.Key_Enter)
            compare(chosen.count, 1)
            compare(chosen.signalArguments[0][0].text, "/web")
            compare(s.text, "/web")
            s.setTextProgrammatically("abc")
            verify(!s.isTagSearch)
        }
    }
}
