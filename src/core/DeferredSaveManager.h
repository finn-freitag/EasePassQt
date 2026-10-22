#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <functional>

namespace EasePass::Core {

/**
 * @brief Debounces and coalesces database saves.
 *
 * C#: Manager/DeferredSaveManager.cs (DeferredSaveHelper). Runs on the thread that owns it
 * (normally the UI thread) and never blocks: the save function only STARTS an asynchronous
 * save and the owner reports its end through saveFinished().
 *
 *  - requestSave(): (re)starts the delay timer. Many requests inside the delay => one save.
 *  - When the timer fires while a save is still running, exactly one more save is queued
 *    and started as soon as the running one finishes (coalescing).
 *  - flush(): starts a scheduled save immediately (or right after the running one).
 *  - saveNow(): like requestSave() + flush().
 *  - cancelPending(): drops a scheduled/queued save; a running save is not interrupted.
 *  - Results are reported through saveCompleted(); idle() is emitted when nothing is
 *    scheduled or running any more.
 */
class DeferredSaveManager : public QObject {
    Q_OBJECT
public:
    /// C#: Settings/DefaultSettingsValues.cs databaseDeferredSaveTime_Sec = 5
    static constexpr int DefaultDelayMs = 5000;

    /// Starts an asynchronous save. Must lead to exactly one saveFinished() call later
    /// (it may also be called synchronously from inside the function).
    using SaveFunction = std::function<void()>;

    explicit DeferredSaveManager(SaveFunction saveFunction, int delayMs = DefaultDelayMs, QObject *parent = nullptr);

    int delayMs() const;
    void setDelayMs(int delayMs);

    void requestSave();
    void flush();
    void saveNow();
    void cancelPending();

    /// Must be called by the owner when the save started by the save function has ended.
    void saveFinished(bool ok, const QString &error = QString());

    bool isScheduled() const; ///< timer running or a follow-up save queued
    bool isSaving() const;
    bool isIdle() const { return !isScheduled() && !isSaving(); }

signals:
    void saveStarted();
    void saveCompleted(bool ok, const QString &error);
    void idle();

private:
    void trigger();

    SaveFunction m_saveFunction;
    QTimer m_timer;
    bool m_saving = false;
    bool m_rerunQueued = false;
};

} // namespace EasePass::Core
