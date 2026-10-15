#pragma once

#include <QObject>

class WebsiteIconManagerTest : public QObject {
    Q_OBJECT

private slots:
    void testUrlNormalization();
    void testCacheMissAndAsyncLoad();
    void testMemoryCacheHit();
    void testDiskCacheHit();
    void testFailureFallbackToGoogleFavicons();
    void testEmptyDomainHandling();
};
