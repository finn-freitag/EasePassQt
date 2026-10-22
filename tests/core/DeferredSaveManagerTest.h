#pragma once

#include <QObject>

class DeferredSaveManagerTest : public QObject {
    Q_OBJECT

private slots:
    void testDebounceCoalescesRequests();
    void testFlushRunsImmediately();
    void testFlushWithoutPendingIsNoOp();
    void testRequestsDuringSaveRunOnceAfterwards();
    void testCancelPending();
    void testErrorReportedBySignal();
    void testSaveNowAndIdleSignal();
};
