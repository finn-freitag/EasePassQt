#pragma once

#include <QObject>

// T7: ClipboardManager, InactivityManager, favicon fetch policy.
class ClipboardInactivityTest : public QObject {
    Q_OBJECT

private slots:
    void testClipboardClearedAfterTimeout();
    void testClipboardNotClearedIfUserCopiedOther();
    void testClipboardRecopyRestartsTimer();
    void testClipboardDisabledWhenZero();
    void testClipboardSecretFlagAndEmptyText();
    void testClipboardClearOnDestruction();
    void testInactivityLogoutAfterTimeout();
    void testInactivityActivityResetsTimer();
    void testInactivityWarningThenLogout();
    void testInactivityWarningCancelledByActivity();
    void testInactivityDisabled();
    void testInactivityFollowsSettingChange();
    void testFaviconFetchDisabledBySetting();
    void testFaviconFetchDefaultOffWithoutSettings();
};
