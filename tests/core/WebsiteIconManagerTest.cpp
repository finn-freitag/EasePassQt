#include "WebsiteIconManagerTest.h"
#include "WebsiteIconManager.h"
#include "INetworkClient.h"
#include "AppSettings.h"
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QImage>
#include <QBuffer>
#include <QMap>

using namespace EasePass::Core;

namespace {
class FakeNetworkClient : public INetworkClient {
public:
    using INetworkClient::INetworkClient;

    void setResponse(const QUrl &url, int statusCode, const QByteArray &data, const QString &error = QString()) {
        NetworkResponse resp;
        resp.statusCode = statusCode;
        resp.data = data;
        resp.errorString = error;
        m_responses[url] = resp;
    }

    void get(const QNetworkRequest &request, ResponseCallback callback) override {
        m_requestedUrls.append(request.url());
        NetworkResponse resp;
        if (m_responses.contains(request.url())) {
            resp = m_responses.value(request.url());
        } else {
            resp.statusCode = 404;
            resp.errorString = "Not Found";
        }
        if (callback) {
            callback(resp);
        }
    }

    QList<QUrl> m_requestedUrls;
    QMap<QUrl, NetworkResponse> m_responses;
};

QByteArray createSamplePngData() {
    QImage img(16, 16, QImage::Format_ARGB32);
    img.fill(Qt::blue);
    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
    return bytes;
}
} // namespace

void WebsiteIconManagerTest::testUrlNormalization() {
    QCOMPARE(WebsiteIconManager::extractDomain("https://www.github.com/login"), QString("github.com"));
    QCOMPARE(WebsiteIconManager::extractDomain("http://WWW.GOOGLE.COM"), QString("google.com"));
    QCOMPARE(WebsiteIconManager::extractDomain("https://sub.domain.org/path?q=1"), QString("sub.domain.org"));
    QCOMPARE(WebsiteIconManager::extractDomain("example.org"), QString("example.org"));
    QCOMPARE(WebsiteIconManager::extractDomain(""), QString(""));
    QCOMPARE(WebsiteIconManager::extractDomain("   "), QString(""));
}

void WebsiteIconManagerTest::testCacheMissAndAsyncLoad() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    auto fakeNet = std::make_shared<FakeNetworkClient>();
    QUrl faviconUrl("https://example.com/favicon.ico");
    fakeNet->setResponse(faviconUrl, 200, createSamplePngData());

    WebsiteIconManager mgr(fakeNet);
    auto settings = AppSettings::createInMemory();
    settings->setFetchFavicons(true);
    mgr.setSettings(settings.get());
    mgr.setCacheDir(tempDir.path());

    QSignalSpy spyLoaded(&mgr, &WebsiteIconManager::iconLoaded);

    // Initial get: miss
    QPixmap icon = mgr.getIcon("https://example.com");
    QVERIFY(icon.isNull());

    // Signal emitted once network response completes
    QCOMPARE(spyLoaded.count(), 1);
    QCOMPARE(spyLoaded.first().first().toString(), QString("example.com"));

    // Icon should now be loaded in memory
    QPixmap loadedIcon = mgr.getIcon("https://example.com");
    QVERIFY(!loadedIcon.isNull());
}

void WebsiteIconManagerTest::testMemoryCacheHit() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    auto fakeNet = std::make_shared<FakeNetworkClient>();
    QUrl faviconUrl("https://fast-service.com/favicon.ico");
    fakeNet->setResponse(faviconUrl, 200, createSamplePngData());

    WebsiteIconManager mgr(fakeNet);
    auto settings = AppSettings::createInMemory();
    settings->setFetchFavicons(true);
    mgr.setSettings(settings.get());
    mgr.setCacheDir(tempDir.path());

    // First request triggers download
    mgr.getIcon("https://fast-service.com");
    QCOMPARE(fakeNet->m_requestedUrls.size(), 1);

    // Second request is a memory cache hit: no new network requests made
    QPixmap cached = mgr.getIcon("https://fast-service.com");
    QVERIFY(!cached.isNull());
    QCOMPARE(fakeNet->m_requestedUrls.size(), 1);
}

void WebsiteIconManagerTest::testDiskCacheHit() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    auto fakeNet = std::make_shared<FakeNetworkClient>();
    QUrl faviconUrl("https://disk-service.org/favicon.ico");
    fakeNet->setResponse(faviconUrl, 200, createSamplePngData());

    WebsiteIconManager mgr(fakeNet);
    auto settings = AppSettings::createInMemory();
    settings->setFetchFavicons(true);
    mgr.setSettings(settings.get());
    mgr.setCacheDir(tempDir.path());

    // Fetch and save to disk
    mgr.getIcon("https://disk-service.org");
    QCOMPARE(fakeNet->m_requestedUrls.size(), 1);

    // Clear memory cache
    mgr.clearCache();

    // Next request should hit disk cache without new network call
    QPixmap diskIcon = mgr.getIcon("https://disk-service.org");
    QVERIFY(!diskIcon.isNull());
    QCOMPARE(fakeNet->m_requestedUrls.size(), 1);
}

void WebsiteIconManagerTest::testFailureFallbackToGoogleFavicons() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    auto fakeNet = std::make_shared<FakeNetworkClient>();
    QUrl directUrl("https://nofavicon.com/favicon.ico");
    fakeNet->setResponse(directUrl, 404, QByteArray(), "Not found");

    QUrl fallbackUrl("https://www.google.com/s2/favicons?domain=nofavicon.com&sz=64");
    fakeNet->setResponse(fallbackUrl, 200, createSamplePngData());

    WebsiteIconManager mgr(fakeNet);
    auto settings = AppSettings::createInMemory();
    settings->setFetchFavicons(true);
    mgr.setSettings(settings.get());
    mgr.setCacheDir(tempDir.path());

    QSignalSpy spyLoaded(&mgr, &WebsiteIconManager::iconLoaded);

    mgr.getIcon("https://nofavicon.com");

    // Both direct URL and fallback URL must have been requested
    QCOMPARE(fakeNet->m_requestedUrls.size(), 2);
    QCOMPARE(fakeNet->m_requestedUrls[0], directUrl);
    QCOMPARE(fakeNet->m_requestedUrls[1], fallbackUrl);

    // Fallback succeeded, so iconLoaded was emitted
    QCOMPARE(spyLoaded.count(), 1);
    QCOMPARE(spyLoaded.first().first().toString(), QString("nofavicon.com"));

    QPixmap icon = mgr.getIcon("https://nofavicon.com");
    QVERIFY(!icon.isNull());
}

void WebsiteIconManagerTest::testEmptyDomainHandling() {
    auto fakeNet = std::make_shared<FakeNetworkClient>();
    WebsiteIconManager mgr(fakeNet);
    auto settings = AppSettings::createInMemory();
    settings->setFetchFavicons(true);
    mgr.setSettings(settings.get());

    QPixmap emptyIcon = mgr.getIcon("");
    QVERIFY(emptyIcon.isNull());
    QCOMPARE(fakeNet->m_requestedUrls.size(), 0);
}
