#pragma once

#include <QObject>

namespace EasePass::Core { class ClipboardManager; }

namespace EasePass::App {

/**
 * QML-facing clipboard (steps.txt T12): forwards to Core::ClipboardManager, which handles
 * secret marking, auto-clear and "only clear if still ours". Copy controls emit
 * copyRequested(text) and QML passes that to ClipboardService.copy().
 */
class ClipboardService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool clearPending READ clearPending NOTIFY stateChanged)
    Q_PROPERTY(int secondsUntilClear READ secondsUntilClear NOTIFY stateChanged)
public:
    explicit ClipboardService(Core::ClipboardManager *manager, QObject *parent = nullptr);

    bool clearPending() const;
    int secondsUntilClear() const;

    Q_INVOKABLE void copy(const QString &text, bool secret = true);
    Q_INVOKABLE void clearNow();

signals:
    void copied();
    void stateChanged();

private:
    Core::ClipboardManager *m_manager;
};

} // namespace EasePass::App
