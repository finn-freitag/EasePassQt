#pragma once

#include <QObject>

// T8: HIBP k-anonymity leak check.
class LeakCheckServiceTest : public QObject {
    Q_OBJECT

private slots:
    void testSha1HexAndSuffixParsing();
    void testLeakedPassword();
    void testNotLeakedPassword();
    void testOnlyPrefixIsSent();
    void testNetworkErrorGivesUnknown();
    void testMalformedResponseGivesUnknown();
    void testCacheHitAvoidsRequest();
    void testCacheExpires();
    void testConcurrentChecksShareRequest();
    void testDisabledBySettingNoRequest();
    void testEmptyPasswordUnknown();
};
