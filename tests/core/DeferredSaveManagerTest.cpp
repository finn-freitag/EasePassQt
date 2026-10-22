#include "DeferredSaveManagerTest.h"
#include "DeferredSaveManager.h"

#include <QSignalSpy>
#include <QTest>

using namespace EasePass::Core;

// C#: tests SaveDatabaseScheduling (debounce, coalesce, force save)

void DeferredSaveManagerTest::testDebounceCoalescesRequests() {
    int saves = 0;
    DeferredSaveManager *mgrPtr = nullptr;
    DeferredSaveManager mgr(
        [&]() {
            ++saves;
            mgrPtr->saveFinished(true);
        },
        50);
    mgrPtr = &mgr;

    for (int i = 0; i < 10; ++i) {
        mgr.requestSave();
        QTest::qWait(5);
    }
    QCOMPARE(saves, 0); // still inside the debounce window
    QVERIFY(mgr.isScheduled());
    QTRY_COMPARE(saves, 1);
    QTest::qWait(100);
    QCOMPARE(saves, 1);
    QVERIFY(mgr.isIdle());
}

void DeferredSaveManagerTest::testFlushRunsImmediately() {
    int saves = 0;
    DeferredSaveManager *mgrPtr = nullptr;
    DeferredSaveManager mgr(
        [&]() {
            ++saves;
            mgrPtr->saveFinished(true);
        },
        60000);
    mgrPtr = &mgr;

    mgr.requestSave();
    QCOMPARE(saves, 0);
    mgr.flush();
    QCOMPARE(saves, 1);
    QVERIFY(mgr.isIdle());
}

void DeferredSaveManagerTest::testFlushWithoutPendingIsNoOp() {
    int saves = 0;
    DeferredSaveManager mgr([&]() { ++saves; }, 10);
    mgr.flush();
    QCOMPARE(saves, 0);
    QVERIFY(mgr.isIdle());
}

void DeferredSaveManagerTest::testRequestsDuringSaveRunOnceAfterwards() {
    int saves = 0;
    DeferredSaveManager mgr([&]() { ++saves; }, 10); // finished manually below
    QSignalSpy started(&mgr, &DeferredSaveManager::saveStarted);

    mgr.saveNow();
    QCOMPARE(saves, 1);
    QVERIFY(mgr.isSaving());

    // Many requests while the first save runs => exactly one follow-up save
    for (int i = 0; i < 5; ++i) {
        mgr.requestSave();
    }
    QTest::qWait(50); // timer fires while saving -> queued
    QCOMPARE(saves, 1);
    QVERIFY(mgr.isScheduled());

    mgr.saveFinished(true);
    QCOMPARE(saves, 2);
    QVERIFY(mgr.isSaving());
    mgr.saveFinished(true);
    QCOMPARE(saves, 2);
    QVERIFY(mgr.isIdle());
    QCOMPARE(started.count(), 2);
}

void DeferredSaveManagerTest::testCancelPending() {
    int saves = 0;
    DeferredSaveManager mgr([&]() { ++saves; }, 20);
    QSignalSpy idleSpy(&mgr, &DeferredSaveManager::idle);
    mgr.requestSave();
    mgr.cancelPending();
    QVERIFY(mgr.isIdle());
    QCOMPARE(idleSpy.count(), 1);
    QTest::qWait(60);
    QCOMPARE(saves, 0);
}

void DeferredSaveManagerTest::testErrorReportedBySignal() {
    DeferredSaveManager *mgrPtr = nullptr;
    DeferredSaveManager mgr([&]() { mgrPtr->saveFinished(false, QStringLiteral("disk full")); }, 10);
    mgrPtr = &mgr;
    QSignalSpy completed(&mgr, &DeferredSaveManager::saveCompleted);

    mgr.saveNow();
    QCOMPARE(completed.count(), 1);
    QCOMPARE(completed.at(0).at(0).toBool(), false);
    QCOMPARE(completed.at(0).at(1).toString(), QString("disk full"));
    QVERIFY(mgr.isIdle());
}

void DeferredSaveManagerTest::testSaveNowAndIdleSignal() {
    int saves = 0;
    DeferredSaveManager mgr([&]() { ++saves; }, 60000);
    QSignalSpy idleSpy(&mgr, &DeferredSaveManager::idle);
    QCOMPARE(mgr.delayMs(), 60000);
    mgr.setDelayMs(-5);
    QCOMPARE(mgr.delayMs(), 0);

    mgr.saveNow();
    QCOMPARE(saves, 1);
    QCOMPARE(idleSpy.count(), 0);
    mgr.saveFinished(true);
    QCOMPARE(idleSpy.count(), 1);
    mgr.saveFinished(true); // spurious call is ignored
    QCOMPARE(idleSpy.count(), 1);
    QCOMPARE(DeferredSaveManager::DefaultDelayMs, 5000);
}
