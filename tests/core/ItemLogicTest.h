#pragma once

#include <QObject>

class ItemLogicTest : public QObject {
    Q_OBJECT
private slots:
    // PasswordStrength & CommonPasswordSequence
    void testCommonPasswordSequenceModes();
    void testSpecialCharClassification();
    void testPasswordStrengthKnownSamples();
    void testPasswordStrengthRepeatedChars();
    void testPasswordStrengthReuseCount();
    void testIsSecureWithSelectiveClasses();

    // Sorting
    void testSortByName();
    void testSortByUsername();
    void testSortByNotes();
    void testSortByFirstTag();
    void testSortByWebsite();
    void testSortByPopularLastYear();
    void testSortByPopularLast30DaysWithClock();
    void testSortByPasswordStrength();
    void testSortStabilityAndUnicodeCase();
    void testClearOldClicksCacheAndResetPopularity();

    // Search & TagSearchManager
    void testFreeTextSearchAllFields();
    void testTagSearchSyntaxAndSuggestions();
    void testTagSearchNoResultsFound();
    void testFindItemsByNameAndTag();

    // EmailAddressIndexer & PasswordItemTagHelper
    void testEmailAddressIndexer();
    void testPasswordItemTagHelper();
};
