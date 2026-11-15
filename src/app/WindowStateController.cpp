#include "WindowStateController.h"

#include "AppSettings.h"

#include <QEvent>
#include <QGuiApplication>
#include <QScreen>
#include <algorithm>

namespace EasePass::App {

using Core::AppSettings;

WindowStateController::WindowStateController(AppSettings *settings, QObject *parent)
    : QObject(parent), m_settings(settings) {}

QRect WindowStateController::fitToScreens(const QRect &saved, const QList<QRect> &screens) {
    if (screens.isEmpty() || !saved.isValid())
        return saved;
    // The title area must stay reachable so the user can move the window.
    const QRect titleArea(saved.left(), saved.top(), saved.width(), std::min(50, saved.height()));
    for (const QRect &screen : screens) {
        const QRect visible = screen.intersected(titleArea);
        if (visible.width() >= std::min(100, saved.width()) && visible.height() >= std::min(50, saved.height()))
            return saved;
    }
    const QRect &primary = screens.first();
    QRect fitted(QPoint(0, 0), saved.size().boundedTo(primary.size()));
    fitted.moveCenter(primary.center());
    return fitted;
}

bool WindowStateController::isMaximized(Qt::WindowStates state) {
    return !(state & Qt::WindowMinimized) && (state & (Qt::WindowMaximized | Qt::WindowFullScreen));
}

bool WindowStateController::isNormal(Qt::WindowStates state) {
    return !(state & (Qt::WindowMinimized | Qt::WindowMaximized | Qt::WindowFullScreen));
}

void WindowStateController::attach(QWindow *window) {
    if (m_window)
        m_window->removeEventFilter(this);
    m_window = window;
    if (!m_window || !m_settings)
        return;
    const QRect stored = m_settings->windowGeometry();
    if (!stored.isValid()) {
        m_window->resize(AppSettings::defaultWindowSize()); // the window manager places it
    } else {
        QList<QRect> screens;
        if (QScreen *primary = QGuiApplication::primaryScreen())
            screens << primary->availableGeometry();
        for (QScreen *s : QGuiApplication::screens())
            if (s != QGuiApplication::primaryScreen())
                screens << s->availableGeometry();
        m_window->setGeometry(fitToScreens(stored, screens));
    }
    m_normalGeometry = m_window->geometry();
    m_window->setWindowStates(m_settings->windowMaximized() ? Qt::WindowMaximized : Qt::WindowNoState);
    m_window->installEventFilter(this);
    connect(m_window, &QWindow::windowStateChanged, this, [this] { track(); });
    // Geometry signals as well: some platforms report changes without Move/Resize events.
    for (auto signal : {&QWindow::xChanged, &QWindow::yChanged, &QWindow::widthChanged, &QWindow::heightChanged})
        connect(m_window, signal, this, [this] { track(); });
}

void WindowStateController::track() {
    if (!m_window)
        return;
    // Only the geometry of the normal window is remembered.
    if (isNormal(m_window->windowStates()) && m_window->isVisible())
        m_normalGeometry = m_window->geometry();
}

bool WindowStateController::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_window) {
        switch (event->type()) {
        case QEvent::Move:
        case QEvent::Resize:
            track();
            break;
        case QEvent::Close:
            save();
            break;
        default:
            break;
        }
    }
    return QObject::eventFilter(watched, event);
}

void WindowStateController::save() {
    if (!m_window || !m_settings)
        return;
    track();
    if (m_normalGeometry.isValid())
        m_settings->setWindowGeometry(m_normalGeometry);
    // A minimized window keeps what was stored: it says nothing about maximized or not.
    if (!(m_window->windowStates() & Qt::WindowMinimized))
        m_settings->setWindowMaximized(isMaximized(m_window->windowStates()));
}

} // namespace EasePass::App
