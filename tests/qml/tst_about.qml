import QtQuick
import QtQuick.Controls
import QtTest
import EasePass

// About page (steps.txt T18) with the real AboutViewModel; links go to a fake opener
// (testContext.openedUrls), no browser is started.
Item {
    id: top
    width: 1000; height: 1200

    Component {
        id: pageC
        AboutPage {
            anchors.fill: parent
            about: testContext.about
        }
    }
    Component { id: spyC; SignalSpy {} }

    TestCase {
        id: tc
        name: "AboutPage"
        when: windowShown

        function init() {
            testContext.clearOpenedUrls()
            failOnWarning(/.?/)
        }

        function createPage(props) {
            const p = createTemporaryObject(pageC, top, props || {})
            verify(p)
            waitForRendering(p)
            return p
        }
        // Clicks an item inside the scroll view after scrolling it into view.
        function clickVisible(page, it) {
            const flick = findChild(page, "aboutScroll").contentItem
            const p = it.mapToItem(flick.contentItem, 0, 0)
            flick.contentY = Math.max(0, Math.min(p.y - 20, flick.contentHeight - flick.height))
            wait(0)
            mouseClick(it)
        }
        function lastUrl() {
            const urls = testContext.openedUrls
            return urls.length > 0 ? urls[urls.length - 1] : ""
        }

        function test_rendersVersion() {
            const page = createPage()
            const version = findChild(page, "versionLabel").text
            verify(/^\d+\.\d+\.\d+/.test(version), version)
            compare(version, testContext.about.version)
        }

        function test_developerLinks() {
            const page = createPage()
            const rep = findChild(page, "developerRepeater")
            compare(rep.count, 2)
            compare(rep.itemAt(0).text, "Finn Freitag")
            mouseClick(rep.itemAt(0))
            compare(lastUrl(), "https://github.com/finn-freitag")
            mouseClick(rep.itemAt(1))
            compare(lastUrl(), "https://github.com/frozenassassine")
        }

        function test_linkCards() {
            const page = createPage()
            clickVisible(page, findChild(page, "websiteLink"))
            compare(lastUrl(), testContext.about.websiteUrl)
            clickVisible(page, findChild(page, "githubLink"))
            compare(lastUrl(), testContext.about.repositoryUrl)
            clickVisible(page, findChild(page, "originalLink"))
            compare(lastUrl(), "https://github.com/FrozenAssassine/EasePass")
            clickVisible(page, findChild(page, "reportBugButton"))
            verify(lastUrl().endsWith("/issues/new"))
            clickVisible(page, findChild(page, "requestFeatureButton"))
            compare(testContext.openedUrls.length, 5)
        }

        function test_changelog() {
            const page = createPage()
            const exp = findChild(page, "changelogExpander")
            verify(!exp.expanded)
            clickVisible(page, findChild(exp, "expanderHeader"))
            verify(exp.expanded)
            const rep = findChild(page, "changelogRepeater")
            verify(rep.count >= 5, rep.count)
            verify(rep.itemAt(0).modelData.title.startsWith("Version " + testContext.about.version))
            verify(rep.itemAt(0).modelData.lines.length > 0)
            waitForRendering(page)
            clickVisible(page, findChild(exp, "expanderHeader"))
            verify(!exp.expanded)
        }

        function test_changelogCanStartExpanded() {
            const page = createPage({ changelogExpanded: true })
            verify(findChild(page, "changelogExpander").expanded)
        }

        function test_dependenciesListed() {
            const page = createPage()
            const exp = findChild(page, "dependenciesExpander")
            clickVisible(page, findChild(exp, "expanderHeader"))
            const rep = findChild(page, "dependencyRepeater")
            compare(rep.count, 4)
            const names = []
            for (let i = 0; i < rep.count; ++i) names.push(rep.itemAt(i).modelData.name)
            compare(names, ["Qt", "OpenSSL", "Argon2", "zxing-cpp"])
            verify(rep.itemAt(0).modelData.version.length > 0)        // Qt runtime version
            verify(rep.itemAt(0).modelData.license.length > 0)
        }

        function test_privacyContactMail() {
            const page = createPage()
            const exp = findChild(page, "privacyExpander")
            clickVisible(page, findChild(exp, "expanderHeader"))
            clickVisible(page, findChild(page, "contactLink"))
            compare(lastUrl(), "mailto:contact@frozenassassine.de")
        }

        function test_onlyWebLinksOpen() {
            verify(!testContext.about.openUrl("file:///etc/passwd"))
            verify(!testContext.about.openUrl("javascript:alert(1)"))
            verify(!testContext.about.openUrl(""))
            compare(testContext.openedUrls.length, 0)
            verify(testContext.about.openUrl("https://example.org"))
            compare(testContext.openedUrls.length, 1)
        }

        function test_backButton() {
            const page = createPage()
            const spy = createTemporaryObject(spyC, top, { target: page, signalName: "backRequested" })
            mouseClick(findChild(page, "backButton"))
            compare(spy.count, 1)
        }
    }
}
