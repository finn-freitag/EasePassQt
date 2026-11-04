import QtQuick
import QtTest
import EasePass

// Instantiates every component through the gallery and fails on any QML warning.
Item {
    id: top
    width: 1000; height: 900

    Component { id: galleryC; Gallery { anchors.fill: parent } }

    TestCase {
        name: "Gallery"
        when: windowShown
        function init() { failOnWarning(/.?/) }

        function test_allComponentsInstantiate() {
            const g = createTemporaryObject(galleryC, top)
            verify(g)
            const names = ["commandBar", "infoBar", "settingsControl", "labeledTextBox",
                           "copyTextbox", "copyPasswordbox", "copyTokenBox", "editPasswordBox",
                           "searchPasswordsBox", "textWithSuggestionBox", "tagRenderControl",
                           "passwordSafetyChart", "iconButton", "splitter", "flyoutMenu",
                           "contentDialog"]
            for (const n of names)
                verify(findChild(g, n), n)
            waitForRendering(g)
            mouseClick(findChild(findChild(g, "copyTextbox"), "copyButton"))
            compare(g.lastCopied, "user@example.com")
            const dlg = findChild(g, "contentDialog")
            dlg.open()
            tryCompare(dlg, "opened", true)
            waitForRendering(g)
            dlg.clickClose()
            tryCompare(dlg, "visible", false)
        }

        function test_darkTheme() {
            const old = Theme.themeMode
            Theme.themeMode = Theme.Mode.Dark
            const g = createTemporaryObject(galleryC, top)
            verify(g)
            waitForRendering(g)
            Theme.themeMode = old
        }
    }
}
