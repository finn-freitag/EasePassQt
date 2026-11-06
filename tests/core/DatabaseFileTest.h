#pragma once

#include <QObject>

class DatabaseFileTest : public QObject {
    Q_OBJECT

private slots:
    // Slow tests (Argon2 production cost)
    void testLoadCSharpTestDbFixture();
    void testLoadCSharpOfficialTestEpdbFixture();
    void testLoadWrongPassword();
    void testSaveAndLoadRoundtrip();
    void testSecondFactorHandling();

    // Fast tests (unit)
    void testLoadDatabaseNotFound();
    void testLoadWrongFormatNotEpdb();
    void testLoadWrongFormatBadVersionTag();
    void testLoadWrongFormatInvalidOuterJson();
    void testLoadWrongFormatInvalidInnerJson();
    void testSaveReadOnlyFails();
    void testSaveEmptyPathFails();
    void testItemAddUpdateDeleteBounds();
    void testCountOccurrences();
    void testWrongPasswordPassingPaddingCheck();
    void testWrongSecondFactorPassingPaddingCheck();
    void testOldAssociatedDataWhenNewKeyPassesPadding();
};
