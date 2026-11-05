#pragma once

#include <QObject>
#include <functional>

namespace EasePass::App {

/**
 * Application-wide event filter that reports user input (keys, mouse, wheel, touch, tablet) to a
 * callback, e.g. AutoLogoutService::reportActivity (steps.txt T7/T19). Install it on qApp.
 * Synthesized and non-input events are ignored, so timers or repaints never count as activity.
 */
class ActivityEventFilter : public QObject {
    Q_OBJECT
public:
    explicit ActivityEventFilter(std::function<void()> onActivity, QObject *parent = nullptr);
    /// Tests: also count events sent by QTest (they are not spontaneous).
    void setRequireSpontaneous(bool require) { m_requireSpontaneous = require; }

    static bool isUserInput(const QEvent *event, bool requireSpontaneous = true);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    std::function<void()> m_onActivity;
    bool m_requireSpontaneous = true;
};

} // namespace EasePass::App
