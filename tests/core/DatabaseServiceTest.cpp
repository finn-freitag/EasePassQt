#include "DatabaseServiceTest.h"
#include "DatabaseService.h"
#include "FakeDatabaseSource.h"
#include "LocalFileDatabaseSource.h"
#include "SecureBuffer.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace EasePass::Core;

namespace {

// Test-only Argon2 parameters (steps.txt 3.3). Never used outside tests.
const KdfParams kFastKdf{1, 64, 1};
const char *kPassword = "correct horse";

PasswordItem makeItem(const QString &name, const QString &password = QStringLiteral("pw")) {
    PasswordItem item;
    item.displayName = name;
    item.password = password;
    return item;
}

QByteArray makeDbBytes(const QList<PasswordItem> &items, const SecretString &password = kPassword,
                       const SecretString &secondFactor = SecretString()) {
    DatabaseFile db;
    db.masterPassword = password;
    db.secondFactor = secondFactor;
    db.useSecondFactor = !secondFactor.isEmpty();
    db.secondFactorType = secondFactor.isEmpty() ? 0 : 1;
    db.items = items;
    return db.serialize(kFastKdf);
}

QStringList decodeNames(const QByteArray &bytes, const SecretString &password = kPassword) {
    DatabaseFile db;
    if (DatabaseFile::loadFromBytes(bytes, password, db, SecretString(), kFastKdf) != LoadResult::Success) {
        return {QStringLiteral("<undecryptable>")};
    }
    QStringList names;
    for (const auto &item : db.items) {
        names << item.displayName;
    }
    return names;
}

LoadResult openAndWait(DatabaseService &svc, const std::shared_ptr<IDatabaseSource> &source,
                       const SecretString &password = kPassword, const SecretString &secondFactor = SecretString()) {
    QSignalSpy spy(&svc, &DatabaseService::openFinished);
    if (!svc.open(source, password, secondFactor)) {
        return LoadResult::Error;
    }
    if (!spy.wait(60000)) {
        return LoadResult::Error;
    }
    return spy.at(0).at(0).value<LoadResult>();
}

std::shared_ptr<FakeDatabaseSource> makeSource(const QList<PasswordItem> &items = {makeItem("A")}) {
    return std::make_shared<FakeDatabaseSource>(makeDbBytes(items));
}

} // namespace

void DatabaseServiceTest::testSerializeLoadFromBytesRoundtrip() {
    QByteArray bytes = makeDbBytes({makeItem("One"), makeItem("Two")});
    QVERIFY(!bytes.isEmpty());
    QCOMPARE(decodeNames(bytes), QStringList({"One", "Two"}));

    DatabaseFile db;
    QCOMPARE(DatabaseFile::loadFromBytes(bytes, "wrong", db, SecretString(), kFastKdf), LoadResult::WrongPassword);
    // The KDF parameters are part of the key: other parameters cannot decrypt
    QCOMPARE(DatabaseFile::loadFromBytes(bytes, kPassword, db, SecretString(), KdfParams{2, 64, 1}),
             LoadResult::WrongPassword);
    QCOMPARE(DatabaseFile::loadFromBytes(QByteArray("garbage"), kPassword, db, SecretString(), kFastKdf),
             LoadResult::WrongFormat);
    QVERIFY(KdfParams::production().isProduction());
    QVERIFY(!kFastKdf.isProduction());
}

void DatabaseServiceTest::testOpenSuccessIsAsync() {
    auto src = makeSource({makeItem("A"), makeItem("B")});
    DatabaseService svc(kFastKdf);
    QSignalSpy finished(&svc, &DatabaseService::openFinished);
    QSignalSpy opened(&svc, &DatabaseService::opened);

    QVERIFY(svc.open(src, kPassword));
    QCOMPARE(svc.state(), DatabaseService::State::Opening);
    QCOMPARE(finished.count(), 0); // result is delivered asynchronously

    QVERIFY(finished.wait(10000));
    QCOMPARE(finished.at(0).at(0).value<LoadResult>(), LoadResult::Success);
    QCOMPARE(opened.count(), 1);
    QVERIFY(svc.isOpen());
    QCOMPARE(svc.itemCount(), 2);
    QCOMPARE(svc.items().at(1).displayName, QString("B"));
    QCOMPARE(svc.source(), src.get());
    QVERIFY(!svc.isReadOnly());
    QVERIFY(!svc.isDirty());
    QCOMPARE(svc.countOccurrences("pw"), 2);
    QCOMPARE(src->calls(), QStringList({"getBytes", "login"}));
}

void DatabaseServiceTest::testOpenWrongPasswordAsync() {
    auto src = makeSource();
    DatabaseService svc(kFastKdf);
    QSignalSpy finished(&svc, &DatabaseService::openFinished);
    QVERIFY(svc.open(src, "wrong password"));
    QCOMPARE(finished.count(), 0);
    QVERIFY(finished.wait(10000));
    QCOMPARE(finished.at(0).at(0).value<LoadResult>(), LoadResult::WrongPassword);
    QCOMPARE(svc.state(), DatabaseService::State::Closed);
    QVERIFY(svc.database() == nullptr);
    QCOMPARE(svc.itemCount(), 0);
    QVERIFY(!src->calls().contains("login"));

    // Retrying with the right password works
    QCOMPARE(openAndWait(svc, src), LoadResult::Success);
}

void DatabaseServiceTest::testOpenNeedsSecondFactor() {
    auto src = std::make_shared<FakeDatabaseSource>(makeDbBytes({makeItem("A")}, kPassword, "123456"));
    DatabaseService svc(kFastKdf);
    QCOMPARE(openAndWait(svc, src), LoadResult::NeedsSecondFactor);
    QVERIFY(!svc.isOpen());
    QCOMPARE(openAndWait(svc, src, kPassword, "654321"), LoadResult::WrongPassword);
    QCOMPARE(openAndWait(svc, src, kPassword, "123456"), LoadResult::Success);
    QVERIFY(svc.database()->useSecondFactor);
    QCOMPARE(svc.itemCount(), 1);
}

void DatabaseServiceTest::testOpenAvailabilityHandling() {
    DatabaseService svc(kFastKdf);

    auto unavailable = makeSource();
    unavailable->setAvailability(IDatabaseSource::Availability::Unavailable);
    QCOMPARE(openAndWait(svc, unavailable), LoadResult::SourceUnavailable);
    QVERIFY(unavailable->calls().isEmpty()); // never touched

    auto locked = makeSource();
    locked->setAvailability(IDatabaseSource::Availability::LockedByOtherUser);
    QCOMPARE(openAndWait(svc, locked), LoadResult::SourceLocked);

    auto unknown = makeSource();
    unknown->setAvailability(IDatabaseSource::Availability::UnknownState);
    QCOMPARE(openAndWait(svc, unknown), LoadResult::Success); // unknown: getBytes decides
    svc.close();

    auto unreadable = makeSource();
    unreadable->setFailReads(true);
    QCOMPARE(openAndWait(svc, unreadable), LoadResult::DatabaseNotFound);
    QVERIFY(!svc.isOpen());
}

void DatabaseServiceTest::testOpenWhileOpeningRejected() {
    auto src = makeSource();
    DatabaseService svc(kFastKdf);
    QSignalSpy finished(&svc, &DatabaseService::openFinished);
    QVERIFY(svc.open(src, kPassword));
    QVERIFY(!svc.open(src, kPassword));
    QVERIFY(finished.wait(10000));
    QTest::qWait(20);
    QCOMPARE(finished.count(), 1);
    QVERIFY(svc.isOpen());

    // Opening while open closes the current database first
    QSignalSpy closed(&svc, &DatabaseService::closed);
    auto other = makeSource({makeItem("X"), makeItem("Y"), makeItem("Z")});
    QCOMPARE(openAndWait(svc, other), LoadResult::Success);
    QCOMPARE(closed.count(), 1);
    QCOMPARE(svc.itemCount(), 3);
}

void DatabaseServiceTest::testOpenNullSourceRejected() {
    DatabaseService svc(kFastKdf);
    QVERIFY(!svc.open(nullptr, kPassword));
    QCOMPARE(svc.state(), DatabaseService::State::Closed);
}

void DatabaseServiceTest::testMutationSchedulesDeferredSave() {
    auto src = makeSource();
    DatabaseService svc(kFastKdf);
    svc.setSaveDelayMs(50);
    QCOMPARE(openAndWait(svc, src), LoadResult::Success);

    QSignalSpy itemsChanged(&svc, &DatabaseService::itemsChanged);
    QSignalSpy saveFinished(&svc, &DatabaseService::saveFinished);
    QVERIFY(svc.addItem(makeItem("B")));
    QCOMPARE(itemsChanged.count(), 1);
    QVERIFY(svc.isDirty());
    QCOMPARE(src->saveCount(), 0); // deferred, not immediate

    QVERIFY(saveFinished.wait(10000));
    QCOMPARE(saveFinished.at(0).at(0).toBool(), true);
    QCOMPARE(src->saveCount(), 1);
    QVERIFY(!svc.isDirty());
    QCOMPARE(decodeNames(src->bytes()), QStringList({"A", "B"}));

    QVERIFY(svc.updateItem(0, makeItem("A2")));
    QVERIFY(svc.deleteItem(1));
    QVERIFY(!svc.updateItem(5, makeItem("out of range")));
    QVERIFY(!svc.deleteItem(-1));
    QTRY_COMPARE(src->saveCount(), 2);
    svc.waitForBackgroundWork();
    QCOMPARE(decodeNames(src->bytes()), QStringList({"A2"}));
}

void DatabaseServiceTest::testConcurrentEditsCoalesceIntoOneSave() {
    auto src = makeSource({});
    DatabaseService svc(kFastKdf);
    svc.setSaveDelayMs(100);
    QCOMPARE(openAndWait(svc, src), LoadResult::Success);

    for (int i = 0; i < 20; ++i) {
        QVERIFY(svc.addItem(makeItem(QString::number(i))));
    }
    QTRY_COMPARE(src->saveCount(), 1);
    QTest::qWait(250);
    QCOMPARE(src->saveCount(), 1);
    QCOMPARE(decodeNames(src->bytes()).size(), 20);
}

void DatabaseServiceTest::testEditsDuringRunningSaveAreSavedAfterwards() {
    auto src = makeSource({});
    src->setSaveDelayMs(300);
    DatabaseService svc(kFastKdf);
    svc.setSaveDelayMs(20);
    QCOMPARE(openAndWait(svc, src), LoadResult::Success);

    QVERIFY(svc.addItem(makeItem("A")));
    QTRY_VERIFY(svc.isSaving());
    QVERIFY(svc.addItem(makeItem("B")));
    QVERIFY(svc.addItem(makeItem("C")));
    QVERIFY(svc.isDirty());

    QTRY_COMPARE_WITH_TIMEOUT(src->saveCount(), 2, 10000);
    QTRY_VERIFY(!svc.isSaving());
    QTest::qWait(100);
    QCOMPARE(src->saveCount(), 2); // B and C coalesced into one follow-up save
    QVERIFY(!svc.isDirty());
    QCOMPARE(decodeNames(src->bytes()), QStringList({"A", "B", "C"}));
}

void DatabaseServiceTest::testFlushOnCloseThenLogout() {
    auto src = makeSource();
    DatabaseService svc(kFastKdf);
    svc.setSaveDelayMs(60000);
    QCOMPARE(openAndWait(svc, src), LoadResult::Success);
    QVERIFY(svc.addItem(makeItem("B")));

    QSignalSpy closed(&svc, &DatabaseService::closed);
    svc.close(); // does not block: the final save runs in the background
    QCOMPARE(closed.count(), 1);
    QVERIFY(!svc.isOpen());
    QCOMPARE(svc.itemCount(), 0);
    QVERIFY(!svc.addItem(makeItem("after close")));

    svc.waitForBackgroundWork();
    QCOMPARE(src->saveCount(), 1);
    QCOMPARE(decodeNames(src->bytes()), QStringList({"A", "B"}));
    QCOMPARE(src->calls(), QStringList({"getBytes", "login", "saveBytes", "logout"}));
}

void DatabaseServiceTest::testReadOnlyRejectsMutation() {
    auto src = makeSource();
    src->setReadOnly(true);
    DatabaseService svc(kFastKdf);
    svc.setSaveDelayMs(10);
    QCOMPARE(openAndWait(svc, src), LoadResult::Success);
    QVERIFY(svc.isReadOnly());

    QSignalSpy itemsChanged(&svc, &DatabaseService::itemsChanged);
    QVERIFY(!svc.addItem(makeItem("B")));
    QVERIFY(!svc.updateItem(0, makeItem("A2")));
    QVERIFY(!svc.deleteItem(0));
    QVERIFY(!svc.setItems({}));
    QVERIFY(!svc.registerClick(0));
    svc.requestSave();
    svc.saveNow();
    QCOMPARE(itemsChanged.count(), 0);
    QCOMPARE(svc.items().at(0).displayName, QString("A"));
    QVERIFY(!svc.isDirty());

    svc.close();
    svc.waitForBackgroundWork();
    QCOMPARE(src->saveCount(), 0);
    QVERIFY(src->calls().contains("logout"));
}

void DatabaseServiceTest::testSourceBecomingReadOnly() {
    auto src = makeSource();
    DatabaseService svc(kFastKdf);
    QCOMPARE(openAndWait(svc, src), LoadResult::Success);
    QSignalSpy roSpy(&svc, &DatabaseService::readOnlyChanged);
    QSignalSpy srcSpy(&svc, &DatabaseService::sourceChanged);

    src->setReadOnly(true);
    QCOMPARE(roSpy.count(), 1);
    QCOMPARE(roSpy.at(0).at(0).toBool(), true);
    QCOMPARE(srcSpy.count(), 1);
    QVERIFY(svc.isReadOnly());
    QVERIFY(!svc.addItem(makeItem("B")));

    src->setReadOnly(false);
    QCOMPARE(roSpy.count(), 2);
    QVERIFY(svc.addItem(makeItem("B")));
}

void DatabaseServiceTest::testSaveFailureReportedAndRetried() {
    auto src = makeSource();
    src->setFailSaves(true);
    DatabaseService svc(kFastKdf);
    svc.setSaveDelayMs(10);
    QCOMPARE(openAndWait(svc, src), LoadResult::Success);

    QSignalSpy failed(&svc, &DatabaseService::saveFailed);
    QVERIFY(svc.addItem(makeItem("B")));
    QVERIFY(failed.wait(10000));
    QVERIFY(failed.at(0).at(0).toString().contains("memory://fake"));
    QVERIFY(svc.isDirty()); // unsaved changes are kept for the next attempt

    src->setFailSaves(false);
    QSignalSpy finished(&svc, &DatabaseService::saveFinished);
    svc.saveNow();
    QVERIFY(finished.wait(10000));
    QCOMPARE(finished.last().at(0).toBool(), true);
    QVERIFY(!svc.isDirty());
    QCOMPARE(decodeNames(src->bytes()), QStringList({"A", "B"}));
}

void DatabaseServiceTest::testLockWipesSecrets() {
    auto src = makeSource();
    DatabaseService svc(kFastKdf);
    QCOMPARE(openAndWait(svc, src), LoadResult::Success);
    svc.waitForBackgroundWork();
    QCOMPARE(svc.database()->masterPassword.toQString(), QString(kPassword));

    int wipes = 0;
    bool allZero = true;
    SecureBuffer::setWipeHook([&](const void *ptr, size_t size) {
        ++wipes;
        const auto *bytes = static_cast<const unsigned char *>(ptr);
        for (size_t i = 0; i < size; ++i) {
            if (bytes[i] != 0) {
                allZero = false;
            }
        }
    });
    svc.lock();
    SecureBuffer::setWipeHook(nullptr);

    QVERIFY(wipes >= 1); // at least the master password buffer
    QVERIFY(allZero);
    QVERIFY(svc.database() == nullptr);
    QCOMPARE(svc.itemCount(), 0);
    QCOMPARE(svc.state(), DatabaseService::State::Closed);
    svc.waitForBackgroundWork();
}

void DatabaseServiceTest::testCloseWhileOpeningDiscardsResult() {
    auto src = makeSource();
    DatabaseService svc(kFastKdf);
    QSignalSpy finished(&svc, &DatabaseService::openFinished);
    QVERIFY(svc.open(src, kPassword));
    svc.close();
    QCOMPARE(svc.state(), DatabaseService::State::Closed);
    svc.waitForBackgroundWork();
    QCOMPARE(finished.count(), 0);
    QVERIFY(!svc.isOpen());
    QCOMPARE(src->calls(), QStringList({"getBytes", "login", "logout"}));
}

void DatabaseServiceTest::testDestructorFlushesPendingSave() {
    auto src = makeSource();
    {
        DatabaseService svc(kFastKdf);
        svc.setSaveDelayMs(60000);
        QCOMPARE(openAndWait(svc, src), LoadResult::Success);
        QVERIFY(svc.addItem(makeItem("B")));
    }
    QCOMPARE(src->saveCount(), 1);
    QCOMPARE(decodeNames(src->bytes()), QStringList({"A", "B"}));
    QCOMPARE(src->calls().last(), QString("logout"));
}

void DatabaseServiceTest::testRegisterClickPersistedOnClose() {
    auto src = makeSource();
    DatabaseService svc(kFastKdf);
    svc.setSaveDelayMs(10);
    QCOMPARE(openAndWait(svc, src), LoadResult::Success);
    QVERIFY(svc.registerClick(0));
    QVERIFY(!svc.registerClick(3));
    QVERIFY(svc.isDirty());
    QTest::qWait(50);
    QCOMPARE(src->saveCount(), 0); // a click alone schedules no save

    svc.close();
    svc.waitForBackgroundWork();
    QCOMPARE(src->saveCount(), 1);
    DatabaseFile db;
    QCOMPARE(DatabaseFile::loadFromBytes(src->bytes(), kPassword, db, SecretString(), kFastKdf), LoadResult::Success);
    QCOMPARE(db.items.at(0).clicks.size(), 1);
}

void DatabaseServiceTest::testLocalFileSourceEndToEnd() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("db.epdb");
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(makeDbBytes({makeItem("A")}));
    }

    auto src = std::make_shared<LocalFileDatabaseSource>(path);
    DatabaseService svc(kFastKdf);
    QCOMPARE(openAndWait(svc, src), LoadResult::Success);
    QVERIFY(svc.addItem(makeItem("B")));
    QSignalSpy finished(&svc, &DatabaseService::saveFinished);
    svc.saveNow();
    QVERIFY(finished.wait(10000));
    QCOMPARE(finished.at(0).at(0).toBool(), true);

    auto current = src->getBytes();
    QVERIFY(current.has_value());
    QCOMPARE(decodeNames(*current), QStringList({"A", "B"}));

    QFile bak(LocalFileDatabaseSource::backupPathFor(path));
    QVERIFY(bak.open(QIODevice::ReadOnly));
    QCOMPARE(decodeNames(bak.readAll()), QStringList({"A"}));
}

void DatabaseServiceTest::testOpenCSharpFixtureThroughService() {
    // Copy so the committed fixture can never be modified (steps.txt "INFO FOR TESTING").
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("test.epdb");
    QVERIFY(QFile::copy(QStringLiteral(EASEPASS_TEST_FIXTURES_DIR "/test.epdb"), path));

    auto src = std::make_shared<LocalFileDatabaseSource>(path, true);
    DatabaseService svc; // production Argon2 parameters
    QCOMPARE(openAndWait(svc, src, "123456"), LoadResult::Success);
    QCOMPARE(svc.itemCount(), 4);
    QCOMPARE(svc.items().at(0).displayName, QString("Test123"));
    QVERIFY(svc.isReadOnly());
    QVERIFY(!svc.addItem(makeItem("nope")));
}
