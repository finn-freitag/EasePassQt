#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QRect>
#include <QWindow>

namespace EasePass::Core { class AppSettings; }

namespace EasePass::App {

/**
 * Restores and remembers the main window's size, position and maximized state.
 *
 * Only the geometry of the normal window is remembered, so a maximized window still stores
 * the size it returns to; a minimized window starts normal. A saved position that is no
 * longer on any screen (monitor unplugged) is moved onto the primary screen. Before the first
 * save the window gets the default size and the window manager places it.
 */
class WindowStateController : public QObject {
    Q_OBJECT
public:
    explicit WindowStateController(Core::AppSettings *settings, QObject *parent = nullptr);

    /// Applies the stored geometry/state to @p window (call before showing it) and tracks it.
    void attach(QWindow *window);
    /// Writes the current normal geometry and state to the settings (also done on close).
    void save();

    // --- Pure helpers (tested directly) ---------------------------------------------------
    /// Keeps @p saved if at least a 100x50 part of its title area is on one of @p screens;
    /// otherwise centers it on the first screen (and shrinks it to fit). Empty list: unchanged.
    static QRect fitToScreens(const QRect &saved, const QList<QRect> &screens);
    /// Maximized or full screen (and not minimized): the state worth restoring.
    static bool isMaximized(Qt::WindowStates state);
    static bool isNormal(Qt::WindowStates state);

private:
    void track();
    bool eventFilter(QObject *watched, QEvent *event) override;

    Core::AppSettings *m_settings;
    QPointer<QWindow> m_window;
    QRect m_normalGeometry;
};

} // namespace EasePass::App
