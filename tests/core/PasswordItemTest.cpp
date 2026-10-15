#include "PasswordItemTest.h"
#include "PasswordItem.h"
#include <QTest>
#include <QJsonObject>
#include <QDate>

using namespace EasePass::Core;

void PasswordItemTest::testJsonRoundtripFull() {
    PasswordItem original;
    original.displayName = "My Work Email";
    original.username = "employee42";
    original.password = "Str0ngP@ss!";
    original.email = "work@company.com";
    original.website = "https://mail.company.com";
    original.notes = "Important corporate account";
    original.secret = "JBSWY3DPEHPK3PXP";
    original.digits = "8";
    original.interval = "60";
    original.algorithm = "SHA256";
    original.tags = {"Work", "Email", "Corporate"};
    original.clicks = {"1.1.2026", "2.1.2026"};

    QJsonObject json = original.toJson();
    PasswordItem loaded = PasswordItem::fromJson(json);

    QCOMPARE(loaded.displayName, original.displayName);
    QCOMPARE(loaded.username, original.username);
    QCOMPARE(loaded.password, original.password);
    QCOMPARE(loaded.email, original.email);
    QCOMPARE(loaded.website, original.website);
    QCOMPARE(loaded.notes, original.notes);
    QCOMPARE(loaded.secret, original.secret);
    QCOMPARE(loaded.digits, original.digits);
    QCOMPARE(loaded.interval, original.interval);
    QCOMPARE(loaded.algorithm, original.algorithm);
    QCOMPARE(loaded.tags, original.tags);
    QCOMPARE(loaded.clicks, original.clicks);
}

void PasswordItemTest::testJsonMissingFieldsDefaults() {
    QJsonObject emptyJson;
    PasswordItem item = PasswordItem::fromJson(emptyJson);

    QCOMPARE(item.digits, QString("6"));
    QCOMPARE(item.interval, QString("30"));
    QCOMPARE(item.algorithm, QString("SHA1"));
    QCOMPARE(item.displayName, QString(""));
    QCOMPARE(item.password, QString(""));
    QCOMPARE(item.tags.size(), 0);
    QCOMPARE(item.clicks.size(), 0);
}

void PasswordItemTest::testJsonUnknownFieldsIgnored() {
    QJsonObject json;
    json["DisplayName"] = "Known Name";
    json["UnknownProperty"] = "Some Unknown Value";
    json["AnotherWeirdField"] = 12345;

    PasswordItem item = PasswordItem::fromJson(json);
    QCOMPARE(item.displayName, QString("Known Name"));
}

void PasswordItemTest::testHas2FA() {
    PasswordItem item;
    QVERIFY(!item.has2FA());

    item.secret = "   ";
    QVERIFY(!item.has2FA());

    item.secret = "JBSWY3DPEHPK3PXP";
    QVERIFY(item.has2FA());
}

void PasswordItemTest::testTagsAndClicksSerialization() {
    PasswordItem item;
    item.tags = {"tag1", "tag2", "tag3"};
    item.clicks = {"10.10.2026", "11.10.2026"};

    QJsonObject json = item.toJson();
    PasswordItem deserialized = PasswordItem::fromJson(json);

    QCOMPARE(deserialized.tags, item.tags);
    QCOMPARE(deserialized.clicks, item.clicks);
}

void PasswordItemTest::testAvatarLetterDeterministic() {
    PasswordItem item1;
    item1.displayName = "Google";
    QCOMPARE(item1.avatarLetter(), QString("G"));

    PasswordItem item2;
    item2.displayName = "  apple  ";
    QCOMPARE(item2.avatarLetter(), QString("A"));

    PasswordItem item3;
    item3.displayName = "";
    item3.website = "https://github.com";
    QCOMPARE(item3.avatarLetter(), QString("H")); // 'h' from "https://github.com" or trimmed

    PasswordItem item4;
    item4.displayName = "";
    item4.website = "";
    QCOMPARE(item4.avatarLetter(), QString("?"));
}

void PasswordItemTest::testAvatarColorDeterministic() {
    PasswordItem itemA1;
    itemA1.displayName = "Dropbox";
    PasswordItem itemA2;
    itemA2.displayName = "Dropbox";
    PasswordItem itemB;
    itemB.displayName = "GitHub";

    QCOMPARE(itemA1.avatarColor(), itemA2.avatarColor());
    QVERIFY(itemA1.avatarColor() != itemB.avatarColor());
}

void PasswordItemTest::testAvatarTextColorContrast() {
    PasswordItem item;
    // Avatar color has luminance check: > 140 -> dark text (20, 20, 20), else white (255, 255, 255)
    item.displayName = "Sample Service";
    QColor textCol = item.avatarTextColor();
    QVERIFY(textCol == QColor(20, 20, 20) || textCol == QColor(255, 255, 255));
}

void PasswordItemTest::testRegisterClick() {
    PasswordItem item;
    QCOMPARE(item.clicks.size(), 0);

    item.registerClick();
    QCOMPARE(item.clicks.size(), 1);
    QString expectedDate = QDate::currentDate().toString("d.M.yyyy");
    QCOMPARE(item.clicks.first(), expectedDate);
}
