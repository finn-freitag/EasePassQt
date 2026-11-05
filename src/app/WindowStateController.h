#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QRect>
#include <QWindow>

namespace EasePass::Core { class AppSettings; }

namespace EasePass::App {

/**
 * Restores and remembers the main window's size, position and state (steps.txt T19;
 * C#: Core/WindowStateManager.cs + RestoreWindowManager).
 *
 * Like C#, only the geometry of the normal (restored) window is remembered, so a maximized
 * or minimized window still stores the size it returns to. The state is stored with the C#
 * values (OverlappedPresenterState: 0 maximized, 1 minimized, 2 restored); a window that was
 * minimized starts restored. A saved position that is no longer on any screen (monitor
 * unplugged) is moved onto the primary screen; the default geometry (never saved) keeps the
 * window manager's placement and only sets the size.
 */
class WindowStateController : public QObject {
    Q_OBJECT
public:
    // C#: Microsoft.UI.Windowing.OverlappedPresenterState
    enum StoredState { Maximized = 0, Minimized = 1, Restored = 2 };

    explicit WindowStateController(Core::AppSettings *settings, QObject *parent = nullptr);

    /// Applies the stored geometry/state to @p window (call before showing it) and tracks it.
    void attach(QWindow *window);
    /// Writes the current normal geometry and state to the settings (also done on close).
    void save();

    // --- Pure helpers (tested directly) ---------------------------------------------------
    /// Keeps @p saved if at least a 100x50 part of its title area is on one of @p screens;
    /// otherwise centers it on the first screen (and shrinks it to fit). Empty list: unchanged.
    static QRect fitToScreens(const QRect &saved, const QList<QRect> &screens);
    static Qt::WindowState toWindowState(int stored);
    static int fromWindowState(Qt::WindowStates state);
    /// True when @p geometry equals the AppSettings defaults (never saved).
    static bool isDefaultGeometry(const QRect &geometry);

private:
    void track();
    bool eventFilter(QObject *watched, QEvent *event) override;

    Core::AppSettings *m_settings;
    QPointer<QWindow> m_window;
    QRect m_normalGeometry;
};

} // namespace EasePass::App
