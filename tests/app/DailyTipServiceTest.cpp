#include "AppTestSupport.h"
#include "AppTests.h"

#include "DailyTipService.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

using namespace EasePass::App;
using namespace AppTest;

namespace {
void writeFile(const QString &path, const QByteArray &data) {
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}
qint64 localNoon(int y, int m, int d) { return QDateTime(QDate(y, m, d), QTime(12, 0)).toSecsSinceEpoch(); }
} // namespace

void DailyTipServiceTest::testParseTips() {
    QCOMPARE(DailyTipService::parseTips("a\r\nb\n\n  c  \r\n\r\n"), (QStringList{"a", "b", "c"}));
    QVERIFY(DailyTipService::parseTips("\n\r\n").isEmpty());
    QCOMPARE(DailyTipService::tipIndexForDate(QDate(2026, 1, 1), 0), -1);
    for (int n : {1, 2, 22})
        for (int d = 1; d <= 28; ++d) {
            const int i = DailyTipService::tipIndexForDate(QDate(2026, 2, d), n);
            QVERIFY(i >= 0 && i < n);
        }
}

void DailyTipServiceTest::testTipStableForADayAndChanges() {
    QTemporaryDir dir;
    QByteArray body;
    for (int i = 0; i < 22; ++i)
        body += "Tip " + QByteArray::number(i) + "\n";
    writeFile(QDir(dir.path()).filePath("DailyTips_en-US.txt"), body);

    FakeClock clock;
    clock.now = localNoon(2026, 11, 5);
    DailyTipService tips(&clock);
    tips.setTipDirectory(dir.path());
    QVERIFY(tips.hasTip());
    const QString today = tips.tip();
    QCOMPARE(today, QStringLiteral("Tip %1").arg(DailyTipService::tipIndexForDate(QDate(2026, 11, 5), 22)));

    clock.now += 6 * 3600; // later the same day
    tips.reload();
    QCOMPARE(tips.tip(), today);

    // Over a month the tip changes at least once.
    QSet<QString> seen;
    for (int d = 1; d <= 30; ++d) {
        clock.now = localNoon(2026, 6, d);
        tips.reload();
        seen.insert(tips.tip());
    }
    QVERIFY(seen.size() > 1);
}

void DailyTipServiceTest::testLanguageFallbackAndMissingFiles() {
    QTemporaryDir dir;
    writeFile(QDir(dir.path()).filePath("DailyTips_en-US.txt"), "English only\n");
    writeFile(QDir(dir.path()).filePath("DailyTips_de-DE.txt"), "Nur Deutsch\n");
    FakeClock clock;
    DailyTipService tips(&clock);
    tips.setTipDirectory(dir.path());
    QSignalSpy changed(&tips, &DailyTipService::tipChanged);
    tips.setLanguage("de-DE");
    QCOMPARE(tips.tip(), QStringLiteral("Nur Deutsch"));
    tips.setLanguage("fr-FR"); // no file -> English
    QCOMPARE(tips.tip(), QStringLiteral("English only"));
    QCOMPARE(changed.count(), 2);
    tips.setLanguage(QString());
    QCOMPARE(tips.language(), DailyTipService::FallbackLanguage);

    QTemporaryDir empty;
    tips.setTipDirectory(empty.path()); // nothing at all -> no tip, box stays hidden
    QVERIFY(!tips.hasTip());
    QVERIFY(tips.tip().isEmpty());
}

void DailyTipServiceTest::testBundledTipFiles() {
    FakeClock clock;
    DailyTipService tips(&clock); // default: :/dailytips from easepass_app
    for (const char *lang : {"en-US", "de-DE"}) {
        QFile f(QStringLiteral(":/dailytips/DailyTips_%1.txt").arg(QLatin1String(lang)));
        QVERIFY2(f.open(QIODevice::ReadOnly), lang);
        QVERIFY(DailyTipService::parseTips(QString::fromUtf8(f.readAll())).size() >= 10);
    }
    QVERIFY(tips.hasTip());
    tips.setLanguage("de-DE");
    QVERIFY(tips.hasTip());
}
