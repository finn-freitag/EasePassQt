import QtQuick
import QtQuick.Controls
import QtTest
import EasePass

// PasswordsPage driven by the real App layer (testContext: AppContext with fakes).
// Sample items (database order): 0 Banking (zoe, notes c, tag money, bank.example, strong pw),
// 1 amazon (alice, notes a, tag shop, c-amazon.example, weak pw, 2FA), 2 Cloud (bob, notes b,
// tag backup, a-cloud.example).
Item {
    id: top
    width: 1100; height: 800

    Component {
        id: pageC
        PasswordsPage {
            anchors.fill: parent
            listModel: testContext.listModel
            item: testContext.item
            actions: testContext.actions
            settings: testContext.settings
            controller: testContext.controller
            clipboard: testContext.clipboard
        }
    }
    Component { id: spyC; SignalSpy {} }

    TestCase {
        id: tc
        name: "PasswordsPage"
        when: windowShown
        property var page: null

        function init() {
            verify(testContext.openSample())
            testContext.setReadOnly(false)
            testContext.clearClipboard()
            testContext.settings.doubleClickCopy = true
            page = createTemporaryObject(pageC, top)
            waitForRendering(page)
            failOnWarning(/.?/)
        }

        function list() { return findChild(page, "passwordList") }
        function names() {
            const l = list(), out = []
            for (let r = 0; r < l.count; ++r) {
                l.positionViewAtIndex(r, ListView.Contain)
                out.push(l.itemAtIndex(r).displayName)
            }
            return out
        }
        function rowOf(name) { return names().indexOf(name) }
        function clickRow(name) {
            const r = rowOf(name)
            verify(r >= 0, name)
            mouseClick(list().itemAtIndex(r))
            waitForRendering(page) // let the detail pane lay out before clicking into it
        }

        function test_listAndSelection() {
            compare(names(), ["Banking", "amazon", "Cloud"])
            verify(findChild(page, "searchBox").activeFocus)
            verify(!findChild(page, "details").visible)
            clickRow("Cloud")
            compare(page.selectedSource, 2)
            compare(list().currentIndex, 2)
            verify(findChild(page, "details").visible)
            compare(findChild(page, "detailTitle").text, "Cloud")
            compare(findChild(page, "usernameBox").text, "bob")
            verify(!findChild(page, "emailBox").visible)   // empty fields hidden
            compare(testContext.clicksOf(2), 1)            // C#: selecting counts as a click
            verify(list().itemAtIndex(1).has2FA)
            verify(findChild(list().itemAtIndex(1), "twoFactorBadge").visible)
            verify(!findChild(list().itemAtIndex(0), "twoFactorBadge").visible)
        }

        function test_searchFiltersRows() {
            const search = findChild(page, "searchBox")
            search.forceActiveFocus()
            keyClick(Qt.Key_C); keyClick(Qt.Key_L); keyClick(Qt.Key_O)
            compare(names(), ["Cloud"])
            compare(search.infoLabel, "1")
            compare(findChild(page, "detailTitle").text, "Cloud")  // single hit is selected
            search.text = ""
            compare(names().length, 3)
            search.text = "zoe"                                    // username search
            compare(names(), ["Banking"])
            search.text = "/shop"                                  // tag search (C#: '/')
            compare(names(), ["amazon"])
            search.text = "nothing here"
            compare(list().count, 0)
            verify(!findChild(page, "emptyState").visible)         // no OOBE while searching
            tryVerify(() => !findChild(page, "details").visible)   // selection was filtered out (deferred)
        }

        // Regression (2026-11-10): typing "lemon" showed "Vercel/v0" (later "Reddit") in the
        // list while the details showed the right item. The page reacted to the model's row
        // signals (single hit -> select, currentIndex) before the ListView had processed them.
        function test_searchKeepsRowsAndDelegatesInSync() {
            const all = ["Amazon", "Apple", "Bank", "Cloudflare", "Discord", "eBay", "GitHub", "Google",
                         "Hetzner", "Lemon Squeezy", "LinkedIn", "Mail", "Netflix", "Nextcloud", "Paypal",
                         "Reddit", "Slack", "Spotify", "Steam", "Telegram", "Vercel/v0", "Wikipedia", "Zoom"]
            verify(testContext.openNames(all))
            waitForRendering(page)
            const search = findChild(page, "searchBox")
            const model = testContext.listModel
            function check(step) {
                waitForRendering(page)
                const l = list()
                compare(l.count, model.count, step + ": count")
                for (let r = 0; r < l.count; ++r) {
                    l.positionViewAtIndex(r, ListView.Contain)
                    const expected = testContext.nameOf(model.sourceIndex(r))
                    compare(l.itemAtIndex(r).displayName, expected, step + ": row " + r)
                }
                if (l.currentIndex >= 0)
                    compare(l.currentItem.displayName, findChild(page, "detailTitle").text, step + ": current row")
            }
            for (let round = 0; round < 2; ++round) {
                search.forceActiveFocus()
                for (const ch of "lemon") {
                    keyClick(ch)
                    check("typed " + search.text)
                }
                compare(findChild(page, "detailTitle").text, "Lemon Squeezy")
                while (search.text.length > 0) {
                    keyClick(Qt.Key_Backspace)
                    check("erased to '" + search.text + "'")
                }
            }
        }

        function test_searchDownMovesIntoList() {
            const search = findChild(page, "searchBox")
            search.forceActiveFocus()
            keyClick(Qt.Key_Down)
            verify(list().activeFocus)
            compare(list().currentIndex, 0)
            keyClick(Qt.Key_Down)
            compare(page.selectedSource, 1)
        }

        function test_eachSortModeOrdersRows() {
            const menu = findChild(page, "sortMenu")
            const expected = [
                ["amazon", "Banking", "Cloud"],   // 0 Name
                ["amazon", "Cloud", "Banking"],   // 1 Username
                ["amazon", "Cloud", "Banking"],   // 2 Notes
                ["Cloud", "Banking", "amazon"],   // 3 First tag
                ["Cloud", "Banking", "amazon"]    // 4 Website
            ]
            for (let m = 0; m < expected.length; ++m) {
                menu.itemAt(m).triggered()
                compare(testContext.listModel.sortMode, m)
                compare(names(), expected[m], "mode " + m)
            }
            // 5/6 popularity: clicks Cloud 2, amazon 1, Banking 0 -> Banking at one end, Cloud at the other
            menu.itemAt(4).triggered()          // toggle off -> database order
            compare(testContext.listModel.sortMode, -1)
            compare(names(), ["Banking", "amazon", "Cloud"])
            clickRow("Cloud"); clickRow("amazon"); clickRow("Cloud")
            for (const m of [5, 6]) {
                menu.itemAt(m).triggered()
                const n = names()
                verify((n[0] === "Cloud" && n[2] === "Banking") || (n[0] === "Banking" && n[2] === "Cloud"), "mode " + m + ": " + n)
                compare(n[1], "amazon")
            }
            // 7 strength: strongest (Banking) and weakest (amazon) at opposite ends
            menu.itemAt(7).triggered()
            const s = names()
            verify((s[0] === "Banking" && s[2] === "amazon") || (s[0] === "amazon" && s[2] === "Banking"), s)
            // direction switch reverses
            menu.itemAt(0).triggered()
            const asc = names()
            menu.itemAt(9).triggered()          // after the separator
            verify(testContext.listModel.descending)
            compare(names(), asc.slice().reverse())
        }

        function test_selectionSurvivesResort() {
            clickRow("Banking")
            findChild(page, "sortMenu").itemAt(0).triggered()   // Name: Banking moves to row 1
            tryCompare(list(), "currentIndex", 1)               // re-synced after the layout change
            compare(page.selectedSource, 0)
            compare(findChild(page, "detailTitle").text, "Banking")
        }

        function test_contextMenuActions() {
            const menu = findChild(page, "contextMenu")
            const row = list().itemAtIndex(rowOf("amazon"))
            mouseClick(row, 20, 10, Qt.RightButton)
            tryCompare(menu, "opened", true)
            compare(menu.target, 1)
            verify(findChild(page, "ctxCopyTotp").visible)
            verify(!findChild(page, "ctxCopyEmail").visible)     // amazon has no email
            findChild(page, "ctxCopyPassword").triggered()
            compare(testContext.clipboardText, "weak")
            findChild(page, "ctxCopyUsername").triggered()
            compare(testContext.clipboardText, "alice")
            findChild(page, "ctxCopyTotp").triggered()
            compare(testContext.clipboardText, testContext.totpFor(testContext.now()))
            menu.close()

            menu.openFor(list().itemAtIndex(rowOf("Banking")), 5, 5)
            tryCompare(menu, "opened", true)
            verify(!findChild(page, "ctxCopyTotp").visible)
            findChild(page, "ctxCopyEmail").triggered()
            compare(testContext.clipboardText, "zoe@bank.example")

            const edit = createTemporaryObject(spyC, top, { target: page, signalName: "editRequested" })
            findChild(page, "ctxEdit").triggered()
            compare(edit.count, 1)
            compare(edit.signalArguments[0][0], 0)              // right-clicked row, not the selection
            menu.close()
        }

        function test_deleteAsksConfirmation() {
            const menu = findChild(page, "contextMenu")
            menu.openFor(list().itemAtIndex(rowOf("Cloud")), 5, 5)
            findChild(page, "ctxDelete").triggered()
            const dlg = findChild(page, "deleteDialog")
            tryCompare(dlg, "opened", true)
            compare(testContext.itemCount, 3)                   // nothing deleted yet
            dlg.clickClose()
            tryCompare(dlg, "visible", false)
            compare(testContext.itemCount, 3)

            clickRow("Cloud")
            mouseClick(findChild(page, "deleteButton"))
            tryCompare(dlg, "opened", true)
            dlg.clickPrimary()
            tryCompare(dlg, "visible", false)
            compare(testContext.itemCount, 2)
            compare(names(), ["Banking", "amazon"])
            verify(!findChild(page, "details").visible)         // deleted item deselected
            compare(page.selectedSource, -1)
        }

        function test_readOnlyDisablesButtons() {
            clickRow("Banking")
            verify(findChild(page, "addButton").enabled)
            verify(findChild(page, "editButton").enabled)
            testContext.setReadOnly(true)
            tryVerify(() => page.readOnly)
            verify(!findChild(page, "addButton").enabled)
            verify(!findChild(page, "editButton").enabled)
            verify(!findChild(page, "deleteButton").enabled)
            verify(!findChild(page, "twoFactorButton").enabled)
            verify(findChild(page, "generateButton").enabled)  // generating changes nothing
            const add = createTemporaryObject(spyC, top, { target: page, signalName: "addRequested" })
            const edit = createTemporaryObject(spyC, top, { target: page, signalName: "editRequested" })
            keySequence("Ctrl+N")
            keySequence("Ctrl+E")
            keyClick(Qt.Key_F2)
            compare(add.count, 0)
            compare(edit.count, 0)
            const menu = findChild(page, "contextMenu")
            menu.openFor(list().itemAtIndex(0), 5, 5)
            verify(!findChild(page, "ctxEdit").enabled)
            verify(!findChild(page, "ctxDelete").enabled)
            verify(findChild(page, "ctxCopyPassword").enabled)  // copying still works
            menu.close()
        }

        function test_doubleClickCopyOnlyWhenEnabled() {
            clickRow("Cloud")
            const user = findChild(page, "usernameBox")
            mouseDoubleClickSequence(user, 20, user.height / 2)
            compare(testContext.clipboardText, "bob")
            testContext.clearClipboard()
            testContext.settings.doubleClickCopy = false
            mouseDoubleClickSequence(user, 20, user.height / 2)
            compare(testContext.clipboardText, "")
            wait(Qt.styleHints.mouseDoubleClickInterval + 50)  // next click is not part of a double click
            mouseClick(findChild(user, "copyButton"))           // explicit copy still works
            compare(testContext.clipboardText, "bob")
        }

        function test_totpBarUpdatesWithFakeClock() {
            clickRow("amazon")
            const box = findChild(page, "tokenBox")
            const bar = findChild(page, "totpBar")
            verify(box.visible && bar.visible)
            compare(box.token, testContext.totpFor(testContext.now()))
            const remaining = 30 - (testContext.now() % 30)
            compare(bar.value, remaining)
            testContext.advanceClock(3)
            compare(bar.value, remaining - 3)
            const before = box.token
            testContext.advanceClock(remaining - 3)             // next 30 s window
            waitForRendering(page)
            compare(bar.value, 30)
            verify(box.token !== before)
            compare(box.token, testContext.totpFor(testContext.now()))
            mouseClick(findChild(box, "copyButton"))
            compare(testContext.clipboardText, box.token)
            clickRow("Banking")                                 // no 2FA -> hidden
            verify(!findChild(page, "tokenBox").visible)
        }

        function test_shortcuts() {
            clickRow("Banking")
            findChild(page, "detailPane").forceActiveFocus()
            keySequence("Ctrl+F")
            verify(findChild(page, "searchBox").activeFocus)
            keySequence("Ctrl+Down")
            compare(page.selectedSource, 1)
            keySequence("Ctrl+Up")
            compare(page.selectedSource, 0)
            const edit = createTemporaryObject(spyC, top, { target: page, signalName: "editRequested" })
            keyClick(Qt.Key_F2)
            compare(edit.count, 1)
            const settings = createTemporaryObject(spyC, top, { target: page, signalName: "settingsRequested" })
            keyClick(Qt.Key_F1)
            compare(settings.count, 1)
            list().forceActiveFocus()
            keyClick(Qt.Key_Escape)
            compare(page.selectedSource, -1)
            verify(!findChild(page, "details").visible)
            keySequence("Ctrl+L")
            verify(!testContext.controller.isOpen)              // logout locks
        }

        function test_emptyState() {
            verify(!findChild(page, "emptyState").visible)
            verify(testContext.openEmpty())
            tryVerify(() => findChild(page, "emptyState").visible)
            compare(list().count, 0)
        }

        function test_splitterWidthPersisted() {
            const left = findChild(page, "leftPane")
            testContext.settings.splitterWidth = 300
            const page2 = createTemporaryObject(pageC, top)
            waitForRendering(page2)
            compare(findChild(page2, "leftPane").width, 300)
            const split = findChild(page2, "splitter")
            mouseDrag(split, 302, 300, 80, 0)
            tryVerify(() => testContext.settings.splitterWidth > 340)
            verify(testContext.settings.splitterWidth <= 500)   // C#: MaxWidth 500
        }
    }
}
