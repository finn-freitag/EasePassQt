#include "AppTestSupport.h"
#include "AppTests.h"

#include "PasswordFilterModel.h"
#include "PasswordListModel.h"
#include "SortingHelper.h"

#include <QAbstractItemModelTester>
#include <QTest>

using EasePass::Core::DatabaseService; using EasePass::Core::PasswordItem; using EasePass::Core::SortingHelper; using EasePass::Core::SortOrder;
using namespace EasePass::App;
using namespace AppTest;
using SortMode = PasswordFilterModel::SortMode;
namespace Core = EasePass::Core;

namespace {
QStringList names(const QAbstractItemModel &m) {
    QStringList out;
    for (int r = 0; r < m.rowCount(); ++r)
        out << m.index(r, 0).data(PasswordListModel::DisplayNameRole).toString();
    return out;
}

QList<PasswordItem> sampleItems() {
    PasswordItem a = makeItem("Banking", "Xy9!aaaaaaaaaaaaaaa", {"money", "work"});
    a.username = "zed";
    a.website = "bank.example";
    PasswordItem b = makeItem("amazon", "pw", {"shop"});
    b.username = "alice";
    b.email = "alice@example.com";
    b.secret = "JBSWY3DPEHPK3PXP";
    PasswordItem c = makeItem("Cloud", "pw2", {"work"});
    c.username = "bob";
    c.notes = "backup storage";
    return {a, b, c};
}
} // namespace

void PasswordModelsTest::testListModelRolesAndNoSecrets() {
    DatabaseService svc(kFastKdf);
    PasswordListModel list(&svc);
    QAbstractItemModelTester tester(&list, QAbstractItemModelTester::FailureReportingMode::QtTest);
    QCOMPARE(list.rowCount(), 0);
    QVERIFY(openItems(svc, sampleItems()));
    QCOMPARE(list.count(), 3);

    const QModelIndex i = list.index(1, 0);
    QCOMPARE(i.data(PasswordListModel::DisplayNameRole).toString(), QStringLiteral("amazon"));
    QCOMPARE(i.data(Qt::DisplayRole).toString(), QStringLiteral("amazon"));
    QCOMPARE(i.data(PasswordListModel::UsernameRole).toString(), QStringLiteral("alice"));
    QCOMPARE(i.data(PasswordListModel::EmailRole).toString(), QStringLiteral("alice@example.com"));
    QCOMPARE(i.data(PasswordListModel::TagsRole).toStringList(), QStringList{"shop"});
    QCOMPARE(i.data(PasswordListModel::Has2FARole).toBool(), true);
    QCOMPARE(i.data(PasswordListModel::AvatarLetterRole).toString(), svc.items().at(1).avatarLetter());
    QCOMPARE(i.data(PasswordListModel::AvatarColorRole).value<QColor>(), svc.items().at(1).avatarColor());
    QCOMPARE(i.data(PasswordListModel::SourceIndexRole).toInt(), 1);
    QCOMPARE(list.index(0, 0).data(PasswordListModel::WebsiteRole).toString(), QStringLiteral("bank.example"));
    QCOMPARE(list.index(2, 0).data(PasswordListModel::NotesRole).toString(), QStringLiteral("backup storage"));
    QVERIFY(!list.index(5, 0).data(PasswordListModel::DisplayNameRole).isValid());

    // Secrets are never exposed as roles.
    const auto roles = list.roleNames();
    QVERIFY(!roles.values().contains("password"));
    QVERIFY(!roles.values().contains("secret"));
    QCOMPARE(roles.value(PasswordListModel::DisplayNameRole), QByteArray("displayName"));
    svc.shutdown();
}

void PasswordModelsTest::testListModelResetsOnChanges() {
    DatabaseService svc(kFastKdf);
    PasswordListModel list(&svc);
    QAbstractItemModelTester tester(&list, QAbstractItemModelTester::FailureReportingMode::QtTest);
    QSignalSpy count(&list, &PasswordListModel::countChanged);
    QVERIFY(openItems(svc, sampleItems()));
    QVERIFY(svc.addItem(makeItem("Delta")));
    QCOMPARE(list.count(), 4);
    QVERIFY(svc.deleteItem(0));
    QCOMPARE(list.count(), 3);
    svc.lock();
    QCOMPARE(list.count(), 0);
    QVERIFY(count.count() >= 4);
    QCOMPARE(list.itemAt(0), nullptr);
    svc.shutdown();
}

void PasswordModelsTest::testFilterFreeTextAndTags() {
    DatabaseService svc(kFastKdf);
    PasswordListModel list(&svc);
    PasswordFilterModel filter(&list);
    QAbstractItemModelTester tester(&filter, QAbstractItemModelTester::FailureReportingMode::QtTest);
    QVERIFY(openItems(svc, sampleItems()));
    QCOMPARE(filter.count(), 3);
    QCOMPARE(filter.infoLabel(), QStringLiteral("3"));

    QSignalSpy count(&filter, &PasswordFilterModel::countChanged);
    filter.setFilterText("CLOUD"); // case-insensitive
    QCOMPARE(names(filter), QStringList{"Cloud"});
    QCOMPARE(filter.infoLabel(), QStringLiteral("1"));
    QVERIFY(count.count() >= 1);
    filter.setFilterText("alice"); // username
    QCOMPARE(names(filter), QStringList{"amazon"});
    filter.setFilterText("/work"); // tag search (C#: "/" prefix)
    QVERIFY(filter.isTagSearch());
    QCOMPARE(names(filter), (QStringList{"Banking", "Cloud"}));
    filter.setFilterText("nothing-matches-ä");
    QCOMPARE(filter.count(), 0);
    filter.setFilterText(QString());
    QCOMPARE(filter.count(), 3);

    // New items appear while a filter is active only if they match.
    filter.setFilterText("cl");
    QVERIFY(svc.addItem(makeItem("Clock")));
    QCOMPARE(names(filter), (QStringList{"Cloud", "Clock"}));
    svc.shutdown();
}

void PasswordModelsTest::testTagSuggestions() {
    DatabaseService svc(kFastKdf);
    PasswordListModel list(&svc);
    PasswordFilterModel filter(&list);
    QVERIFY(openItems(svc, sampleItems()));
    QSignalSpy sugg(&filter, &PasswordFilterModel::suggestionsChanged);
    filter.setFilterText("/w");
    QCOMPARE(filter.suggestions(), QStringList{"/work"});
    QCOMPARE(sugg.count(), 1);
    filter.setFilterText("/");
    QCOMPARE(filter.suggestions().size(), 3); // money, shop, work
    // Programmatic changes (suggestion chosen) do not reopen suggestions.
    filter.setFilterTextProgrammatically("/money");
    QVERIFY(filter.suggestions().isEmpty());
    QCOMPARE(names(filter), QStringList{"Banking"});
    filter.setFilterText("plain");
    QVERIFY(filter.suggestions().isEmpty());
    svc.shutdown();
}

void PasswordModelsTest::testSortModesAndOrder() {
    DatabaseService svc(kFastKdf);
    PasswordListModel list(&svc);
    FakeClock clock;
    PasswordFilterModel filter(&list, &clock);
    QAbstractItemModelTester tester(&filter, QAbstractItemModelTester::FailureReportingMode::QtTest);
    QVERIFY(openItems(svc, sampleItems()));
    QCOMPARE(names(filter), (QStringList{"Banking", "amazon", "Cloud"})); // None = database order

    QSignalSpy sort(&filter, &PasswordFilterModel::sortChanged);
    filter.setSortMode(SortMode::Name);
    QCOMPARE(names(filter), (QStringList{"amazon", "Banking", "Cloud"}));
    filter.setDescending(true);
    QCOMPARE(names(filter), (QStringList{"Cloud", "Banking", "amazon"}));
    filter.setDescending(false);
    filter.setSortMode(SortMode::Username);
    QCOMPARE(names(filter), (QStringList{"amazon", "Cloud", "Banking"})); // alice, bob, zed
    filter.setSortMode(SortMode::PasswordStrength);
    {
        QStringList expected; // same order as the Core sort (T5)
        for (const auto &i : SortingHelper::sorted(sampleItems(), Core::SortMode::PasswordStrength, SortOrder::Ascending, &clock))
            expected << i.displayName;
        QCOMPARE(names(filter), expected);
    }
    filter.setSortMode(SortMode::None);
    QCOMPARE(names(filter), (QStringList{"Banking", "amazon", "Cloud"}));
    QCOMPARE(sort.count(), 6);

    // Sorting stays applied to new items.
    filter.setSortMode(SortMode::Name);
    QVERIFY(svc.addItem(makeItem("Aardvark")));
    QCOMPARE(names(filter).first(), QStringLiteral("Aardvark"));
    svc.shutdown();
}

void PasswordModelsTest::testSourceIndexMapping() {
    DatabaseService svc(kFastKdf);
    PasswordListModel list(&svc);
    PasswordFilterModel filter(&list);
    QVERIFY(openItems(svc, sampleItems()));
    filter.setSortMode(SortMode::Name);
    QCOMPARE(filter.sourceIndex(0), 1); // amazon
    QCOMPARE(filter.proxyRow(1), 0);
    filter.setFilterText("cloud");
    QCOMPARE(filter.proxyRow(1), -1);
    QCOMPARE(filter.sourceIndex(0), 2);
    QCOMPARE(filter.sourceIndex(7), -1);
    svc.shutdown();
}

void PasswordModelsTest::testInPlaceEditKeepsRows() {
    DatabaseService svc(kFastKdf);
    PasswordListModel list(&svc);
    PasswordFilterModel filter(&list);
    QAbstractItemModelTester tester(&filter, QAbstractItemModelTester::FailureReportingMode::QtTest);
    QVERIFY(openItems(svc, sampleItems()));
    QSignalSpy reset(&list, &QAbstractItemModel::modelReset);
    QSignalSpy changed(&list, &QAbstractItemModel::dataChanged);
    QVERIFY(svc.registerClick(1));          // clicks emit no itemsChanged at all (Core design)
    PasswordItem edited = svc.items().at(0);
    edited.displayName = "Zzz";
    QVERIFY(svc.updateItem(0, edited));
    QCOMPARE(reset.count(), 0);             // same count -> in place, views keep their selection
    QCOMPARE(changed.count(), 1);
    QCOMPARE(list.index(0, 0).data(PasswordListModel::DisplayNameRole).toString(), QStringLiteral("Zzz"));
    QCOMPARE(list.index(1, 0).data(PasswordListModel::HasPasswordRole).toBool(), true);
    // Proxy re-sorts on dataChanged.
    filter.setSortMode(SortMode::Name);
    edited.displayName = "000 first";
    QVERIFY(svc.updateItem(0, edited));
    QCOMPARE(names(filter).first(), QStringLiteral("000 first"));
    QVERIFY(svc.addItem(makeItem("New")));  // count changes -> reset
    QCOMPARE(reset.count(), 1);
    svc.shutdown();
}
