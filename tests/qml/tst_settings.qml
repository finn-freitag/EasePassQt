import QtQuick
import QtQuick.Controls
import QtTest
import EasePass

// Settings page (steps.txt T17) on the real SettingsViewModel / AppSettings (in-memory store).
Item {
    id: top
    width: 1000; height: 1400

    Component {
        id: pageC
        SettingsPage {
            anchors.fill: parent
            viewModel: testContext.settingsViewModel
        }
    }
    Component { id: spyC; SignalSpy {} }

    TestCase {
        id: tc
        name: "SettingsPage"
        when: windowShown

        readonly property var settings: testContext.settings
        readonly property var vm: testContext.settingsViewModel

        function init() {
            vm.resetToDefaults()
            settings.verifyNotLeaked = false   // no HIBP lookups in UI tests
            failOnWarning(/.?/)
        }
        function cleanup() {
            vm.resetToDefaults()
            settings.verifyNotLeaked = false
        }

        function createPage(props) {
            const p = createTemporaryObject(pageC, top, props || {})
            verify(p)
            waitForRendering(p)
            return p
        }
        function typeInto(field, text) {
            field.forceActiveFocus()
            field.selectAll()
            keyClick(Qt.Key_Delete)
            for (const ch of text)
                keyClick(ch)
        }
        function stepUp(box, times) {
            box.forceActiveFocus()
            for (let i = 0; i < (times || 1); ++i)
                keyClick(Qt.Key_Up)
        }

        function test_controlsShowStoredValues() {
            settings.inactivityLogoutMinutes = 12
            settings.clipboardClearSeconds = 40
            settings.passwordLength = 21
            settings.passwordChars = "abcXYZ123"
            settings.doubleClickCopy = false
            settings.showIcons = false
            settings.fetchFavicons = true
            settings.verifyNotLeaked = true
            settings.databaseDeferredSaveSeconds = 9
            const page = createPage()
            compare(findChild(page, "inactivityBox").value, 12)
            compare(findChild(page, "clipboardBox").value, 40)
            compare(findChild(page, "saveDelayBox").value, 9)
            compare(findChild(page, "passwordLengthField").text, "21")
            compare(findChild(page, "passwordCharsField").text, "abcXYZ123")
            verify(!findChild(page, "doubleClickSwitch").checked)
            verify(!findChild(page, "showIconsSwitch").checked)
            verify(findChild(page, "fetchFaviconsSwitch").checked)
            verify(findChild(page, "verifyLeakedSwitch").checked)
            compare(findChild(page, "languageBox").currentText, "English")
            settings.verifyNotLeaked = false
        }

        function test_spinBoxesWriteSettingsAndFollowResets() {
            const page = createPage()
            const box = findChild(page, "inactivityBox")
            stepUp(box, 2)
            compare(settings.inactivityLogoutMinutes, 5)        // default 3
            vm.resetToDefaults()                                 // external change still shown
            compare(box.value, 3)
            stepUp(findChild(page, "clipboardBox"))
            compare(settings.clipboardClearSeconds, 16)
            stepUp(findChild(page, "saveDelayBox"))
            compare(settings.databaseDeferredSaveSeconds, 6)
        }

        function test_rangeLimits() {
            const page = createPage()
            const inactivity = findChild(page, "inactivityBox")
            compare(inactivity.from, 1)
            compare(inactivity.to, 1000)
            settings.inactivityLogoutMinutes = 1000
            stepUp(inactivity)
            compare(settings.inactivityLogoutMinutes, 1000)
            settings.inactivityLogoutMinutes = 1
            inactivity.forceActiveFocus()
            keyClick(Qt.Key_Down)
            compare(settings.inactivityLogoutMinutes, 1)
            const clip = findChild(page, "clipboardBox")
            compare(clip.from, 1)
            compare(clip.to, 100000)
            // Typed values outside the range are clamped by the spin box.
            typeInto(clip.contentItem, "999999")
            keyClick(Qt.Key_Return)
            compare(settings.clipboardClearSeconds, 100000)
        }

        function test_passwordLengthTextSemantics() {
            const page = createPage()
            const field = findChild(page, "passwordLengthField")
            typeInto(field, "2a0")
            compare(field.text, "20")                            // non-digits dropped (C#)
            compare(settings.passwordLength, 20)
            typeInto(field, "5")
            compare(settings.passwordLength, 8)                  // minimum
            compare(field.text, "5")                             // not replaced while typing
            typeInto(field, "")
            compare(settings.passwordLength, 15)                 // empty -> default
            typeInto(field, "0")
            compare(settings.passwordLength, 15)
            typeInto(field, "5000")
            compare(settings.passwordLength, 999)                // maximum
            typeInto(field, "5")
            findChild(page, "passwordCharsField").forceActiveFocus()
            compare(field.text, "8")                             // shows the stored value afterwards
            vm.resetPasswordLength()
            compare(field.text, "15")                            // follows external changes
        }

        function test_passwordCharsAndReset() {
            const page = createPage()
            const field = findChild(page, "passwordCharsField")
            const defaults = settings.passwordChars
            typeInto(field, "ab12")
            compare(settings.passwordChars, "ab12")
            verify(!findChild(page, "charsWarning").visible)
            typeInto(field, "aaa")
            verify(findChild(page, "charsWarning").visible)
            typeInto(field, "")
            compare(settings.passwordChars, defaults)            // empty -> default (C#)
            compare(field.text, "")                              // ... but the field stays empty
            typeInto(field, "xyz")
            mouseClick(findChild(page, "resetCharsButton"))
            compare(settings.passwordChars, defaults)
            compare(field.text, defaults)
        }

        function test_switchesWriteSettings() {
            const page = createPage()
            mouseClick(findChild(page, "doubleClickSwitch"))
            verify(!settings.doubleClickCopy)
            mouseClick(findChild(page, "verifyLeakedSwitch"))
            verify(settings.verifyNotLeaked)
            mouseClick(findChild(page, "verifyLeakedSwitch"))
            verify(!settings.verifyNotLeaked)
            const fetch = findChild(page, "fetchFaviconsSwitch")
            verify(fetch.enabled)
            mouseClick(fetch)
            verify(settings.fetchFavicons)
            mouseClick(findChild(page, "showIconsSwitch"))
            verify(!settings.showIcons)
            verify(!fetch.enabled)                               // favicons need icons
        }

        function test_languageSwitchesLive() {
            const page = createPage()
            const box = findChild(page, "languageBox")
            const card = findChild(page, "languageCard")
            compare(box.count, 3)
            compare(card.header, "Language")
            box.currentIndex = 1
            box.activated(1)
            compare(settings.language, "de-DE")
            compare(box.currentText, "Deutsch")
            tryCompare(card, "header", "Sprache")                 // no restart needed (T20)
            compare(findChild(page, "resetCharsButton").text, "Zurücksetzen")
            box.currentIndex = 2
            box.activated(2)
            tryCompare(card, "header", "Idioma")
            box.currentIndex = 0
            box.activated(0)
            compare(settings.language, "en-US")
            tryCompare(card, "header", "Language")
        }

        function test_resetPopularity() {
            verify(testContext.openSample())
            testContext.clickAll()
            compare(testContext.clicksOf(0), 1)
            const page = createPage()
            const button = findChild(page, "resetPopularityButton")
            verify(button.enabled)
            mouseClick(button)
            compare(testContext.clicksOf(0), 0)
            compare(testContext.clicksOf(2), 0)
            verify(findChild(page, "pageInfo").open)

            testContext.setReadOnly(true)
            tryVerify(() => !button.enabled)
            testContext.setReadOnly(false)
            testContext.controller.lock()
            tryVerify(() => !button.enabled)
        }

        function test_manageDatabaseButton() {
            const page = createPage({ databaseOpen: false })
            const button = findChild(page, "manageDatabaseButton")
            verify(!button.enabled)
            page.databaseOpen = true
            const spy = createTemporaryObject(spyC, top, { target: page, signalName: "manageDatabaseRequested" })
            mouseClick(button)
            compare(spy.count, 1)
            const back = createTemporaryObject(spyC, top, { target: page, signalName: "backRequested" })
            mouseClick(findChild(page, "backButton"))
            compare(back.count, 1)
        }
    }
}
