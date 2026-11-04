#include "ClipboardManager.h"

#include "AppSettings.h"

#include <QCryptographicHash>

namespace EasePass::Core {

namespace {
QByteArray hashText(const QString &text) {
    return QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha256);
}
} // namespace

ClipboardManager::ClipboardManager(std::shared_ptr<IClipboard> clipboard, AppSettings *settings, const IClock *clock,
                                   QObject *parent)
    : QObject(parent), m_clipboard(std::move(clipboard)), m_settings(settings),
      m_clock(clock ? clock : &SystemClock::instance()) {
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &ClipboardManager::checkExpiry);
}

ClipboardManager::~ClipboardManager() {
    if (m_pending && m_clearOnDestruction && clipboardStillOurs())
        m_clipboard->clear();
}

void ClipboardManager::copy(const QString &text, bool secret) {
    if (!m_clipboard || text.isEmpty())
        return;

    m_clipboard->setText(text, secret);
    emit copied();

    const int seconds = m_settings ? m_settings->clipboardClearSeconds() : AppSettings::Defaults::ClipboardClearSeconds;
    if (seconds <= 0) { // C#: 0 disables clearing
        reset();
        return;
    }
    m_hash = hashText(text);
    m_clearAt = m_clock->currentSecsSinceEpoch() + seconds;
    m_pending = true;
    m_timer.start();
}

int ClipboardManager::secondsUntilClear() const {
    if (!m_pending)
        return -1;
    return int(std::max<qint64>(0, m_clearAt - m_clock->currentSecsSinceEpoch()));
}

bool ClipboardManager::clipboardStillOurs() const {
    return m_clipboard && !m_hash.isEmpty() && hashText(m_clipboard->text()) == m_hash;
}

void ClipboardManager::reset() {
    m_timer.stop();
    m_pending = false;
    m_hash.fill('\0');
    m_hash.clear();
    m_clearAt = 0;
}

void ClipboardManager::clearNow() {
    if (!m_pending)
        return;
    const bool ours = clipboardStillOurs();
    reset();
    if (ours) {
        m_clipboard->clear();
        emit cleared();
    } else {
        emit clearSkipped();
    }
}

void ClipboardManager::checkExpiry() {
    if (!m_pending)
        return;
    if (m_clock->currentSecsSinceEpoch() >= m_clearAt)
        clearNow();
}

} // namespace EasePass::Core
