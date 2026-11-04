#pragma once

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <memory>

#include "IClipboard.h"
#include "IClock.h"

namespace EasePass::Core {

class AppSettings;

/**
 * Copies text to the clipboard and clears it again after AppSettings::clipboardClearSeconds,
 * but only if the clipboard still holds what we copied (the user may have copied something
 * else meanwhile). 0 seconds = never clear. C#: EasePass/Helper/ClipboardHelper.cs.
 *
 * The copied text itself is not kept; only its SHA-256 is remembered for the comparison.
 * Expiry is evaluated against IClock in checkExpiry(), which an internal 1 s timer calls
 * while a clear is pending (tests call it directly with a fake clock).
 */
class ClipboardManager : public QObject {
    Q_OBJECT
public:
    explicit ClipboardManager(std::shared_ptr<IClipboard> clipboard, AppSettings *settings,
                              const IClock *clock = nullptr, QObject *parent = nullptr);
    ~ClipboardManager() override;

    /// Copies @p text (empty text is ignored). @p secret marks it for clipboard managers.
    Q_INVOKABLE void copy(const QString &text, bool secret = true);

    /// True while an automatic clear is scheduled.
    bool isClearPending() const { return m_pending; }
    /// Seconds left until the clear, or -1 if none is pending.
    int secondsUntilClear() const;

    /// Clears now if our content is still on the clipboard; cancels the pending clear.
    void clearNow();
    /// Evaluates the timeout; called by the internal timer.
    void checkExpiry();

    /// When true (default) the clipboard is cleared on destruction if it still holds our content.
    void setClearOnDestruction(bool on) { m_clearOnDestruction = on; }

signals:
    void copied();
    /// Emitted when the clipboard was cleared by us.
    void cleared();
    /// Emitted when the pending clear was dropped because the user copied something else.
    void clearSkipped();

private:
    bool clipboardStillOurs() const;
    void reset();

    std::shared_ptr<IClipboard> m_clipboard;
    QPointer<AppSettings> m_settings;
    const IClock *m_clock;
    QTimer m_timer;
    QByteArray m_hash;
    qint64 m_clearAt = 0;
    bool m_pending = false;
    bool m_clearOnDestruction = true;
};

} // namespace EasePass::Core
