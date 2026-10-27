#include "ItemLogicTest.h"
#include "CommonPasswordSequence.h"
#include "DatabaseFile.h"
#include "DatabaseService.h"
#include "EmailAddressIndexer.h"
#include "FakeDatabaseSource.h"
#include "IClock.h"
#include "PasswordItem.h"
#include "PasswordItemTagHelper.h"
#include "PasswordStrength.h"
#include "SearchPasswordsManager.h"
#include "SortingHelper.h"
#include "TagSearchManager.h"

#include <QDate>
#include <QDateTime>
#include <QSignalSpy>
#include <QTimeZone>
#include <QTest>

using namespace EasePass::Core;

namespace {

class FakeClock : public IClock {
public:
    explicit FakeClock(qint64 secs = 0) : m_secs(secs) {}
    explicit FakeClock(const QDate &date, const QTime &time = QTime(12, 0, 0))
        : m_secs(QDateTime(date, time, QTimeZone::UTC).toSecsSinceEpoch()) {}

    qint64 currentSecsSinceEpoch() const override { return m_secs; }
    void setSecs(qint64 secs) { m_secs = secs; }
    void setDate(const QDate &date, const QTime &time = QTime(12, 0, 0)) {
        m_secs = QDateTime(date, time, QTimeZone::UTC).toSecsSinceEpoch();
    }

private:
    qint64 m_secs = 0;
};

PasswordItem makeItem(const QString &name, const QString &username = QString(), const QString &password = QString(),
                      const QString &website = QString(), const QString &notes = QString(),
                      const QStringList &tags = {}, const QStringList &clicks = {}, const QString &email = QString()) {
    PasswordItem item;
    item.displayName = name;
    item.username = username;
    item.password = password;
    item.website = website;
    item.notes = notes;
    item.tags = tags;
    item.clicks = clicks;
    item.email = email;
    return item;
}

QStringList extractNames(const QList<PasswordItem> &items) {
    QStringList names;
    names.reserve(items.size());
    for (const PasswordItem &item : items) {
        names.append(item.displayName);
    }
    return names;
}

} // namespace

// --- PasswordStrength & CommonPasswordSequence --------------------------------------------------

void ItemLogicTest::testCommonPasswordSequenceModes() {
    // Whole sequence mode (case-insensitive)
    CommonPasswordSequence whole(QStringLiteral("password"));
    QVERIFY(whole.isEntireSequence());
    QVERIFY(whole.containsSequence(u"myPassWord123"));
    QVERIFY(whole.containsSequence(u"PASSWORD"));
    QVERIFY(!whole.containsSequence(u"passw0rd"));
    QVERIFY(!whole.containsSequence(u""));

    // Sliding window mode (minLength = 5 on keyboard walk)
    CommonPasswordSequence qwerty(QStringLiteral("qwertyuiopasdfghjklzxcvbnm"), 5);
    QVERIFY(!qwerty.isEntireSequence());
    QCOMPARE(qwerty.minLength(), 5);
    QVERIFY(qwerty.containsSequence(u"xxQwertYY")); // "qwert" (length 5)
    QVERIFY(qwerty.containsSequence(u"12asdfg34")); // "asdfg" (length 5)
    QVERIFY(qwerty.containsSequence(u"!!cvbnm!!")); // final window "cvbnm" (length 5)
    QVERIFY(!qwerty.containsSequence(u"qwer123"));  // only length 4

    // Numeric forward/backward run (minLength = 4)
    CommonPasswordSequence digits(QStringLiteral("012345678909876543210"), 4);
    QVERIFY(digits.containsSequence(u"ab1234cd"));
    QVERIFY(digits.containsSequence(u"ab9876cd"));
    QVERIFY(digits.containsSequence(u"ab3210cd"));
    QVERIFY(!digits.containsSequence(u"ab123cd"));
}

void ItemLogicTest::testSpecialCharClassification() {
    // All characters from C# CharExtension.IsSpecialChar
    const QString csharpSpecials = QStringLiteral("!\"§$%&/{([)]=}?\\`´*+~³'#-_.:,;<>|²");
    for (QChar ch : csharpSpecials) {
        QVERIFY2(PasswordStrength::isSpecialChar(ch), qPrintable(QString("Expected special: %1").arg(ch)));
    }
    QVERIFY(!PasswordStrength::isSpecialChar(u'a'));
    QVERIFY(!PasswordStrength::isSpecialChar(u'Z'));
    QVERIFY(!PasswordStrength::isSpecialChar(u'5'));
    QVERIFY(!PasswordStrength::isSpecialChar(u' '));
}

void ItemLogicTest::testPasswordStrengthKnownSamples() {
    // 1. "password": only lowercase -> 1/6 criteria met (hasLower=true, noCommonSequences=false because "password")
    {
        auto eval = PasswordStrength::evaluate(u"password");
        QCOMPARE(eval.hasLower, true);
        QCOMPARE(eval.hasUpper, false);
        QCOMPARE(eval.meetsMinLength, false);
        QCOMPARE(eval.hasSpecial, false);
        QCOMPARE(eval.hasNumber, false);
        QCOMPARE(eval.noCommonSequences, false);
        QCOMPARE(eval.passedCriteria, 1);
        QCOMPARE(eval.score, -4); // 1 true (+1), 5 false (-5) => -4
        QCOMPARE(eval.rating, PasswordStrength::Rating::VeryWeak);
    }

    // 2. "Tr0ub4dor&3": has lower, upper, special ('&'), number, no common sequence, but length 11 < 15 -> 5/6 criteria
    {
        auto eval = PasswordStrength::evaluate(u"Tr0ub4dor&3");
        QCOMPARE(eval.hasLower, true);
        QCOMPARE(eval.hasUpper, true);
        QCOMPARE(eval.meetsMinLength, false);
        QCOMPARE(eval.hasSpecial, true);
        QCOMPARE(eval.hasNumber, true);
        QCOMPARE(eval.noCommonSequences, true);
        QCOMPARE(eval.passedCriteria, 5);
        QCOMPARE(eval.score, 4); // 5 true (+5), 1 false (-1) => +4
        QCOMPARE(eval.rating, PasswordStrength::Rating::Good);
    }

    // 3. Long random password (>= 15 chars, all 4 classes, no common sequences) -> 6/6 criteria
    {
        auto eval = PasswordStrength::evaluate(u"K9#mP2$vL7!xQ4&zR8");
        QCOMPARE(eval.hasLower, true);
        QCOMPARE(eval.hasUpper, true);
        QCOMPARE(eval.meetsMinLength, true);
        QCOMPARE(eval.hasSpecial, true);
        QCOMPARE(eval.hasNumber, true);
        QCOMPARE(eval.noCommonSequences, true);
        QCOMPARE(eval.passedCriteria, 6);
        QCOMPARE(eval.score, 6);
        QCOMPARE(eval.rating, PasswordStrength::Rating::Strong);
        QVERIFY(PasswordStrength::isSecure(u"K9#mP2$vL7!xQ4&zR8"));
    }

    // 4. Long password containing a common sequence ("qwertz" or "welcome") fails predictability
    {
        auto eval = PasswordStrength::evaluate(u"Welcome_To_My_Vault_99!");
        QCOMPARE(eval.hasLower, true);
        QCOMPARE(eval.hasUpper, true);
        QCOMPARE(eval.meetsMinLength, true);
        QCOMPARE(eval.hasSpecial, true);
        QCOMPARE(eval.hasNumber, true);
        QCOMPARE(eval.noCommonSequences, false);
        QCOMPARE(eval.passedCriteria, 5);
        QCOMPARE(eval.score, 4);
        QVERIFY(!PasswordStrength::isSecure(u"Welcome_To_My_Vault_99!"));
    }

    // 5. Empty password
    {
        auto eval = PasswordStrength::evaluate(u"");
        QCOMPARE(eval.hasLower, false);
        QCOMPARE(eval.hasUpper, false);
        QCOMPARE(eval.meetsMinLength, false);
        QCOMPARE(eval.hasSpecial, false);
        QCOMPARE(eval.hasNumber, false);
        QCOMPARE(eval.noCommonSequences, true); // empty has no common sequences
        QCOMPARE(eval.passedCriteria, 1);
        QCOMPARE(eval.score, -4);
        QCOMPARE(eval.rating, PasswordStrength::Rating::VeryWeak);
    }
}

void ItemLogicTest::testPasswordStrengthRepeatedChars() {
    // 5 repeated letters ("aaaaa" or "ZZZZZ") -> common sequence
    QVERIFY(PasswordStrength::hasRepeatedCharacters(u"abcaaaaadef"));
    QVERIFY(PasswordStrength::hasRepeatedCharacters(u"abcBBBBBdef"));
    QVERIFY(!PasswordStrength::hasRepeatedCharacters(u"abcaaaadef")); // 4 letters is below 5

    // 4 repeated digits ("5555") -> common sequence
    QVERIFY(PasswordStrength::hasRepeatedCharacters(u"abc5555def"));
    QVERIFY(!PasswordStrength::hasRepeatedCharacters(u"abc555def")); // 3 digits is below 4

    auto eval = PasswordStrength::evaluate(u"Str0ng!Pass_aaaaa_92");
    QCOMPARE(eval.noCommonSequences, false);

    auto evalDigits = PasswordStrength::evaluate(u"Str0ng!Pass_7777_xy");
    QCOMPARE(evalDigits.noCommonSequences, false);
}

void ItemLogicTest::testPasswordStrengthReuseCount() {
    DatabaseFile db;
    db.addItem(makeItem("One", "u1", "SharedPass!123456"));
    db.addItem(makeItem("Two", "u2", "SharedPass!123456"));
    db.addItem(makeItem("Three", "u3", "UniquePass!987654"));

    QCOMPARE(db.countOccurrences("SharedPass!123456"), 2);
    QCOMPARE(db.countOccurrences("UniquePass!987654"), 1);
    QCOMPARE(db.countOccurrences("BrandNewPass!0000"), 0);

    // When checking an existing item in the DB (existingSingleTime = true):
    // 1 occurrence means only this item uses it -> notReused = true
    // 2 occurrences means another item also uses it -> notReused = false
    auto evalUniqueExisting = PasswordStrength::evaluate(u"UniquePass!987654", db, true);
    QCOMPARE(evalUniqueExisting.reuseCount, 1);
    QCOMPARE(evalUniqueExisting.notReused, true);
    QVERIFY(PasswordStrength::checkPasswordNotReused("UniquePass!987654", db, true));

    auto evalSharedExisting = PasswordStrength::evaluate(u"SharedPass!123456", db, true);
    QCOMPARE(evalSharedExisting.reuseCount, 2);
    QCOMPARE(evalSharedExisting.notReused, false);
    QVERIFY(!PasswordStrength::checkPasswordNotReused("SharedPass!123456", db, true));

    // When checking a new password before adding (existingSingleTime = false):
    // 1 occurrence already in DB means reused -> notReused = false
    auto evalUniqueAsNew = PasswordStrength::evaluate(u"UniquePass!987654", db, false);
    QCOMPARE(evalUniqueAsNew.reuseCount, 1);
    QCOMPARE(evalUniqueAsNew.notReused, false);
    QVERIFY(!PasswordStrength::checkPasswordNotReused("UniquePass!987654", db, false));

    auto evalBrandNew = PasswordStrength::evaluate(u"BrandNewPass!0000", db, false);
    QCOMPARE(evalBrandNew.reuseCount, 0);
    QCOMPARE(evalBrandNew.notReused, true);
    QVERIFY(PasswordStrength::checkPasswordNotReused("BrandNewPass!0000", db, false));
}

void ItemLogicTest::testIsSecureWithSelectiveClasses() {
    const auto inc = PasswordStrength::getAllowedIncludes(u"abcdef123456");
    QCOMPARE(inc.includeLower, true);
    QCOMPARE(inc.includeNumber, true);
    QCOMPARE(inc.includeUpper, false);
    QCOMPARE(inc.includeSpecial, false);

    // Only lower + number required when upper and special are disabled
    QVERIFY(PasswordStrength::isSecure(u"a8f3k9m2x7p4q1w6", 15, false, true, false, true));
    // Fails if contains keyboard walk "qwerty"
    QVERIFY(!PasswordStrength::isSecure(u"qwerty8f3k9m2x7p4", 15, false, true, false, true));
}

// --- Sorting ------------------------------------------------------------------------------------

void ItemLogicTest::testSortByName() {
    QList<PasswordItem> items = {
        makeItem("Charlie"),
        makeItem("alpha"),
        makeItem("Bravo"),
    };

    QCOMPARE(extractNames(SortingHelper::sorted(items, SortMode::Name, SortOrder::Ascending)),
             QStringList({"alpha", "Bravo", "Charlie"}));
    QCOMPARE(extractNames(SortingHelper::sorted(items, SortMode::Name, SortOrder::Descending)),
             QStringList({"Charlie", "Bravo", "alpha"}));
}

void ItemLogicTest::testSortByUsername() {
    QList<PasswordItem> items = {
        makeItem("Item1", "zoe"),
        makeItem("Item2", "alice"),
        makeItem("Item3", "bob"),
    };

    QCOMPARE(extractNames(SortingHelper::sorted(items, SortMode::Username, SortOrder::Ascending)),
             QStringList({"Item2", "Item3", "Item1"}));
    QCOMPARE(extractNames(SortingHelper::sorted(items, SortMode::Username, SortOrder::Descending)),
             QStringList({"Item1", "Item3", "Item2"}));
}

void ItemLogicTest::testSortByNotes() {
    QList<PasswordItem> items = {
        makeItem("Item1", "", "", "", "Zeta note"),
        makeItem("Item2", "", "", "", "Alpha note"),
        makeItem("Item3", "", "", "", "Middle note"),
    };

    QCOMPARE(extractNames(SortingHelper::sorted(items, SortMode::Notes, SortOrder::Ascending)),
             QStringList({"Item2", "Item3", "Item1"}));
    QCOMPARE(extractNames(SortingHelper::sorted(items, SortMode::Notes, SortOrder::Descending)),
             QStringList({"Item1", "Item3", "Item2"}));
}

void ItemLogicTest::testSortByFirstTag() {
    // C# SortingHelper.ByFirstTag: items with tags come first sorted by tags[0], items without tags come last
    QList<PasswordItem> items = {
        makeItem("NoTag1", "", "", "", "", {}),
        makeItem("WorkItem", "", "", "", "", {"Work", "Alpha"}),
        makeItem("NoTag2", "", "", "", "", {}),
        makeItem("BankingItem", "", "", "", "", {"Banking", "Zeta"}),
        makeItem("SocialItem", "", "", "", "", {"Social"}),
    };

    QCOMPARE(extractNames(SortingHelper::sorted(items, SortMode::FirstTag, SortOrder::Ascending)),
             QStringList({"BankingItem", "SocialItem", "WorkItem", "NoTag1", "NoTag2"}));

    QCOMPARE(extractNames(SortingHelper::sorted(items, SortMode::FirstTag, SortOrder::Descending)),
             QStringList({"NoTag1", "NoTag2", "WorkItem", "SocialItem", "BankingItem"}));
}

void ItemLogicTest::testSortByWebsite() {
    QList<PasswordItem> items = {
        makeItem("GitHub", "", "", "https://github.com"),
        makeItem("Apple", "", "", "https://apple.com"),
        makeItem("Zillow", "", "", "https://zillow.com"),
    };

    QCOMPARE(extractNames(SortingHelper::sorted(items, SortMode::Website, SortOrder::Ascending)),
             QStringList({"Apple", "GitHub", "Zillow"}));
    QCOMPARE(extractNames(SortingHelper::sorted(items, SortMode::Website, SortOrder::Descending)),
             QStringList({"Zillow", "GitHub", "Apple"}));
}

void ItemLogicTest::testSortByPopularLastYear() {
    FakeClock clock(QDate(2026, 10, 27));

    QList<PasswordItem> items = {
        makeItem("Rare", "", "", "", "", {}, {"1.5.2026"}),
        makeItem("Frequent", "", "", "", "", {}, {"1.5.2026", "2.5.2026", "3.5.2026"}),
        makeItem("MediumB", "", "", "", "", {}, {"1.5.2026", "2.5.2026"}),
        makeItem("MediumA", "", "", "", "", {}, {"1.5.2026", "2.5.2026"}),
        makeItem("AncientClicksOnly", "", "", "", "", {}, {"1.1.2020", "2.1.2020", "3.1.2020", "4.1.2020"}),
    };

    // With injected clock, AncientClicksOnly (> 365 days ago) has 0 clicks in the last year;
    // ties break by DisplayName ascending ("MediumA" before "MediumB").
    QCOMPARE(extractNames(SortingHelper::sorted(items, SortMode::PopularLastYear, SortOrder::Ascending, &clock)),
             QStringList({"Frequent", "MediumA", "MediumB", "Rare", "AncientClicksOnly"}));

    // Without clock (C# ByPopularAllTime after ClearOldClicksCache), raw click count is used
    QCOMPARE(extractNames(SortingHelper::sorted(items, SortMode::PopularLastYear, SortOrder::Ascending, nullptr)),
             QStringList({"AncientClicksOnly", "Frequent", "MediumA", "MediumB", "Rare"}));
}

void ItemLogicTest::testSortByPopularLast30DaysWithClock() {
    // Anchor clock to 2026-10-27 12:00 UTC
    FakeClock clock(QDate(2026, 10, 27), QTime(12, 0, 0));

    // 27.10.2026 minus 29 days = 28.9.2026 (inside 30-day window: 29.5 days < 30 days)
    // 27.10.2026 minus 30 days = 27.9.2026 (outside 30-day window: 30.5 days >= 30 days)
    QList<PasswordItem> items = {
        makeItem("OldBurst", "", "", "", "", {}, {"1.8.2026", "2.8.2026", "3.8.2026", "27.9.2026"}), // 0 in last 30d
        makeItem("RecentTwo", "", "", "", "", {}, {"28.9.2026", "27.10.2026"}),                      // 2 in last 30d
        makeItem("RecentThree", "", "", "", "", {}, {"10.10.2026", "15.10.2026", "26.10.2026"}),     // 3 in last 30d
        makeItem("RecentOne", "", "", "", "", {}, {"27.9.2026", "5.10.2026"}),                       // 1 in last 30d
    };

    QCOMPARE(SortingHelper::countPopularityLast30Days(items[0], &clock), 0);
    QCOMPARE(SortingHelper::countPopularityLast30Days(items[1], &clock), 2);
    QCOMPARE(SortingHelper::countPopularityLast30Days(items[2], &clock), 3);
    QCOMPARE(SortingHelper::countPopularityLast30Days(items[3], &clock), 1);

    QCOMPARE(extractNames(SortingHelper::sorted(items, SortMode::PopularLast30Days, SortOrder::Ascending, &clock)),
             QStringList({"RecentThree", "RecentTwo", "RecentOne", "OldBurst"}));

    QCOMPARE(extractNames(SortingHelper::sorted(items, SortMode::PopularLast30Days, SortOrder::Descending, &clock)),
             QStringList({"OldBurst", "RecentOne", "RecentTwo", "RecentThree"}));
}

void ItemLogicTest::testSortByPasswordStrength() {
    QList<PasswordItem> items = {
        makeItem("VeryWeak", "", "password"),         // score -4 (1/6)
        makeItem("Strong", "", "K9#mP2$vL7!xQ4&zR8"), // score +6 (6/6)
        makeItem("Good", "", "Tr0ub4dor&3"),          // score +4 (5/6)
        makeItem("Weak", "", "Abcdef1"), // score 0  (3/6: lower, upper, digit, no common -> 4/6 = +2; wait: A-b-c-d-e-f
                                         // has "abcde" common sequence! -> 3/6 = 0)
    };

    // Ascending in C# ByPasswordStrength puts strongest first (-pmi1_res.CompareTo(pmi2_res))
    QCOMPARE(extractNames(SortingHelper::sorted(items, SortMode::PasswordStrength, SortOrder::Ascending)),
             QStringList({"Strong", "Good", "Weak", "VeryWeak"}));

    QCOMPARE(extractNames(SortingHelper::sorted(items, SortMode::PasswordStrength, SortOrder::Descending)),
             QStringList({"VeryWeak", "Weak", "Good", "Strong"}));
}

void ItemLogicTest::testSortStabilityAndUnicodeCase() {
    // Items with identical sort key (same username) must preserve their relative order (std::stable_sort)
    QList<PasswordItem> items = {
        makeItem("FirstSame", "same_user"),
        makeItem("SecondSame", "same_user"),
        makeItem("ThirdSame", "same_user"),
        makeItem("EarlierUser", "aaa_user"),
    };

    QCOMPARE(extractNames(SortingHelper::sorted(items, SortMode::Username, SortOrder::Ascending)),
             QStringList({"EarlierUser", "FirstSame", "SecondSame", "ThirdSame"}));

    // Unicode and case handling
    QList<PasswordItem> unicodeItems = {
        makeItem(QStringLiteral("Zürich")), makeItem(QStringLiteral("äpfel")),  makeItem(QStringLiteral("Apfel")),
        makeItem(QStringLiteral("apfel")),  makeItem(QStringLiteral("Берлин")),
    };

    const QList<PasswordItem> sortedUnicode = SortingHelper::sorted(unicodeItems, SortMode::Name, SortOrder::Ascending);
    QCOMPARE(sortedUnicode.size(), 5);
    // Deterministic ordering: "Apfel" and "apfel" sort before "Zürich"
    const QStringList names = extractNames(sortedUnicode);
    QVERIFY(names.indexOf("Apfel") < names.indexOf("Zürich"));
    QVERIFY(names.indexOf("apfel") < names.indexOf("Zürich"));
}

void ItemLogicTest::testClearOldClicksCacheAndResetPopularity() {
    FakeClock clock(QDate(2026, 10, 27), QTime(12, 0, 0));

    DatabaseFile db;
    PasswordItem item1 = makeItem("One", "", "", "", "", {}, {"1.1.2025", "15.10.2025", "28.10.2025", "27.10.2026"});
    PasswordItem item2 = makeItem("Two", "", "", "", "", {}, {"invalid", "10.10.2026"});
    db.addItem(item1);
    db.addItem(item2);

    // Clear clicks > 365 days before 2026-10-27 12:00:
    // 28.10.2025 is 364.5 days ago (kept), 15.10.2025 is 377.5 days ago (removed), 1.1.2025 (removed)
    db.clearOldClicksCache(&clock);
    QCOMPARE(db.items[0].clicks, QStringList({"28.10.2025", "27.10.2026"}));
    QCOMPARE(db.items[1].clicks, QStringList({"invalid", "10.10.2026"}));

    // Read-only database ignores clearOldClicksCache and resetPopularity
    db.isReadOnly = true;
    db.resetPopularity();
    QCOMPARE(db.items[0].clicks.size(), 2);

    db.isReadOnly = false;
    db.resetPopularity();
    QVERIFY(db.items[0].clicks.isEmpty());
    QVERIFY(db.items[1].clicks.isEmpty());

    // Also test DatabaseService::resetPopularity and registerClick with FakeClock
    const KdfParams fastKdf{1, 64, 1};
    DatabaseFile seedDb;
    seedDb.masterPassword = SecretString("pw");
    seedDb.items = {makeItem("SvcItem")};
    auto src = std::make_shared<FakeDatabaseSource>(seedDb.serialize(fastKdf));

    DatabaseService svc(fastKdf);
    QSignalSpy openSpy(&svc, &DatabaseService::openFinished);
    QVERIFY(svc.open(src, SecretString("pw")));
    QVERIFY(openSpy.wait(5000));

    QVERIFY(svc.registerClick(0, &clock));
    QCOMPARE(svc.items().at(0).clicks, QStringList({"27.10.2026"}));

    QVERIFY(svc.resetPopularity());
    QVERIFY(svc.items().at(0).clicks.isEmpty());
}

// --- Search & TagSearchManager ------------------------------------------------------------------

void ItemLogicTest::testFreeTextSearchAllFields() {
    QList<PasswordItem> items = {
        makeItem("GitHub", "octocat", "pw1", "https://github.com", "Developer platform", {"Dev", "Code"}, {},
                 "octocat@github.com"),
        makeItem("House Bank", "jdoe99", "pw2", "https://mybank.de", "PIN is in safe", {"Finance", "Important"}, {},
                 "john.doe@mail.org"),
        makeItem("Streaming", "movie_fan", "pw3", "https://watch.tv", "Shared family account", {"Entertainment"}, {},
                 "family@home.net"),
    };

    SearchPasswordsManager mgr;

    // Empty query returns all items and count label
    auto allRes = mgr.searchPasswords(items, "");
    QCOMPARE(allRes.items.size(), 3);
    QCOMPARE(allRes.infoLabel, QString("3"));
    QVERIFY(!allRes.isTagSearch);
    QVERIFY(allRes.suggestions.isEmpty());

    // Match by DisplayName
    QCOMPARE(extractNames(mgr.searchPasswords(items, "git").items), QStringList({"GitHub"}));
    // Match by Username
    QCOMPARE(extractNames(mgr.searchPasswords(items, "JDOE").items), QStringList({"House Bank"}));
    // Match by Email
    QCOMPARE(extractNames(mgr.searchPasswords(items, "family@home").items), QStringList({"Streaming"}));
    // Match by Website
    QCOMPARE(extractNames(mgr.searchPasswords(items, "mybank.de").items), QStringList({"House Bank"}));
    // Match by Notes
    QCOMPARE(extractNames(mgr.searchPasswords(items, "shared family").items), QStringList({"Streaming"}));
    // Match by Tag in free text
    QCOMPARE(extractNames(mgr.searchPasswords(items, "finance").items), QStringList({"House Bank"}));
    // Match multiple items
    QCOMPARE(extractNames(mgr.searchPasswords(items, "https://").items),
             QStringList({"GitHub", "House Bank", "Streaming"}));
    // No match
    auto noneRes = mgr.searchPasswords(items, "nonexistent_xyz");
    QVERIFY(noneRes.items.isEmpty());
    QCOMPARE(noneRes.infoLabel, QString("0"));
}

void ItemLogicTest::testTagSearchSyntaxAndSuggestions() {
    QList<PasswordItem> items = {
        makeItem("GitHub", "octocat", "pw1", "", "", {"Work", "Dev"}),
        makeItem("GitLab", "octocat", "pw2", "", "", {"Work", "CI"}),
        makeItem("Netflix", "user", "pw3", "", "", {"Personal", "Video"}),
        makeItem("Untagged", "work", "pw4", "https://work.com", "work notes", {}),
    };

    SearchPasswordsManager mgr;
    mgr.updateTagList(items);

    // Unique tags should be deduplicated and sorted
    QCOMPARE(mgr.tagSearchManager().uniqueTagList(), QStringList({"CI", "Dev", "Personal", "Video", "Work"}));

    // "/" alone returns all items that have at least one tag (Untagged is excluded!) and all tag suggestions
    auto slashRes = mgr.searchPasswords(items, "/");
    QVERIFY(slashRes.isTagSearch);
    QCOMPARE(extractNames(slashRes.items), QStringList({"GitHub", "GitLab", "Netflix"}));
    QCOMPARE(slashRes.infoLabel, QString("3"));
    QCOMPARE(slashRes.suggestions, QStringList({"/CI", "/Dev", "/Personal", "/Video", "/Work"}));

    // "/work" matches only items with a tag containing "work" (case-insensitive), NOT "Untagged" even though
    // "Untagged" has "work" in username/website/notes!
    auto workRes = mgr.searchPasswords(items, "/work");
    QVERIFY(workRes.isTagSearch);
    QCOMPARE(extractNames(workRes.items), QStringList({"GitHub", "GitLab"}));
    QCOMPARE(workRes.infoLabel, QString("2"));
    QCOMPARE(workRes.suggestions, QStringList({"/Work"}));

    // Partial tag match "/de" -> matches "Dev" and "Video"
    auto partialRes = mgr.searchPasswords(items, "/de");
    QCOMPARE(extractNames(partialRes.items), QStringList({"GitHub", "Netflix"}));
    QCOMPARE(partialRes.suggestions, QStringList({"/Dev", "/Video"}));

    // Non-user text change (e.g. suggestion selection) with "/Personal"
    auto chosenRes = mgr.searchPasswords(items, "/Personal", false);
    QVERIFY(chosenRes.isTagSearch);
    QCOMPARE(extractNames(chosenRes.items), QStringList({"Netflix"}));
    QCOMPARE(chosenRes.infoLabel, QString("1"));
}

void ItemLogicTest::testTagSearchNoResultsFound() {
    QList<PasswordItem> items = {
        makeItem("GitHub", "octocat", "pw1", "", "", {"Work", "Dev"}),
    };

    SearchPasswordsManager mgr;
    auto res = mgr.searchPasswords(items, "/nonexistent");
    QVERIFY(res.isTagSearch);
    QVERIFY(res.items.isEmpty());
    QCOMPARE(res.infoLabel, QString("0"));
    // C# SearchPasswordsManager adds "No results found" when no tag matches
    QCOMPARE(res.suggestions, QStringList({"No results found"}));
}

void ItemLogicTest::testFindItemsByNameAndTag() {
    QList<PasswordItem> items = {
        makeItem("Alpha Service", "bob", "pw1", "", "", {"TagA"}),
        makeItem("Beta", "alpha_user", "pw2", "", "", {"TagB"}),
    };

    // C# DatabaseItem.FindItemsByName checks DisplayName only
    QCOMPARE(extractNames(SearchPasswordsManager::findItemsByName(items, "alpha")), QStringList({"Alpha Service"}));
    QCOMPARE(extractNames(SearchPasswordsManager::findItemsByTag(items, "tagb")), QStringList({"Beta"}));
}

// --- EmailAddressIndexer & PasswordItemTagHelper ------------------------------------------------

void ItemLogicTest::testEmailAddressIndexer() {
    QList<PasswordItem> items = {
        makeItem("Item1", "", "", "", "", {}, {}, "  Alice@Example.com "),
        makeItem("Item2", "", "", "", "", {}, {}, "alice@example.com"), // duplicate with different casing
        makeItem("Item3", "", "", "", "", {}, {}, ""),                  // empty ignored
        makeItem("Item4", "", "", "", "", {}, {}, "   "),               // whitespace ignored
        makeItem("Item5", "", "", "", "", {}, {}, "bob@work.org"),
        makeItem("Item6", "", "", "", "", {}, {}, "charlie@example.com"),
    };

    EmailAddressIndexer indexer;
    indexer.indexUniqueEmail(items);

    // Preserves first-seen trimmed email casing, deduplicated case-insensitively
    QCOMPARE(indexer.uniqueEmailAddresses(), QStringList({"Alice@Example.com", "bob@work.org", "charlie@example.com"}));

    // Suggestions filter case-insensitively
    QCOMPARE(indexer.suggestions("EXAMPLE"), QStringList({"Alice@Example.com", "charlie@example.com"}));
    QCOMPARE(indexer.suggestions("  bob "), QStringList({"bob@work.org"}));
    QVERIFY(indexer.suggestions("").isEmpty());
    QVERIFY(indexer.suggestions("   ").isEmpty());
    QVERIFY(indexer.suggestions("nobody").isEmpty());
}

void ItemLogicTest::testPasswordItemTagHelper() {
    PasswordItem item;
    PasswordItemTagHelper::parseToTags("  Work   Finance\tSocial  ", item);
    QCOMPARE(item.tags, QStringList({"Work", "Finance", "Social"}));
    QCOMPARE(PasswordItemTagHelper::formatTags(item), QString("Work Finance Social"));

    PasswordItemTagHelper::parseToTags("", item);
    QVERIFY(item.tags.isEmpty());
    QCOMPARE(PasswordItemTagHelper::formatTags(item), QString(""));

    // Tag color is deterministic and follows C# MD5(tag) -> RGB(bytes[1], bytes[0], bytes[2])
    const QColor c1 = PasswordItemTagHelper::tagColor("Work");
    const QColor c2 = PasswordItemTagHelper::tagColor("Work");
    const QColor c3 = PasswordItemTagHelper::tagColor("Personal");
    QVERIFY(c1.isValid());
    QCOMPARE(c1, c2);
    QVERIFY(c1 != c3);
}
