#include "ActivityEventFilter.h"

#include <QEvent>

namespace EasePass::App {

ActivityEventFilter::ActivityEventFilter(std::function<void()> onActivity, QObject *parent)
    : QObject(parent), m_onActivity(std::move(onActivity)) {}

bool ActivityEventFilter::isUserInput(const QEvent *event, bool requireSpontaneous) {
    switch (event->type()) {
    case QEvent::KeyPress:
    case QEvent::MouseButtonPress:
    case QEvent::MouseMove:
    case QEvent::Wheel:
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TabletPress:
    case QEvent::TabletMove:
        return !requireSpontaneous || event->spontaneous();
    default:
        return false;
    }
}

bool ActivityEventFilter::eventFilter(QObject *watched, QEvent *event) {
    if (m_onActivity && isUserInput(event, m_requireSpontaneous))
        m_onActivity();
    return QObject::eventFilter(watched, event);
}

} // namespace EasePass::App
