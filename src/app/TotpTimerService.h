#pragma once

#include <QObject>
#include <QTimer>

namespace EasePass::Core { class IClock; }

namespace EasePass::App {

/**
 * The ONE shared 1 s timer for every TOTP display (steps.txt T12). Consumers acquire() while
 * they show a token and release() afterwards; the timer only runs while someone holds it.
 * tick() carries the clock's current time so all codes are computed for the same second.
 * Tests call tickNow() with a fake clock instead of waiting.
 */
class TotpTimerService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
public:
    explicit TotpTimerService(const Core::IClock *clock = nullptr, QObject *parent = nullptr);

    const Core::IClock *clock() const { return m_clock; }
    qint64 now() const;
    bool isRunning() const { return m_timer.isActive(); }
    int subscribers() const { return m_subscribers; }

    void acquire();
    void release();
    Q_INVOKABLE void tickNow();

signals:
    void tick(qint64 nowSecs);
    void runningChanged();

private:
    const Core::IClock *m_clock;
    QTimer m_timer;
    int m_subscribers = 0;
};

} // namespace EasePass::App
