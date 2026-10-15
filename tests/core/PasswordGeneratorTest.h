#pragma once

#include <QObject>

class PasswordGeneratorTest : public QObject {
    Q_OBJECT

private slots:
    void testLengthBounds();
    void testEachEnabledClassPresent();
    void testSelectiveClasses();
    void testEmptyPoolFallback();
    void testCustomSymbolsOnly();
    void testDistributionSanity();
};
