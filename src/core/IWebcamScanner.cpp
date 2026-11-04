#include "IWebcamScanner.h"

#include "QrCodeScanner.h"

#include <QDateTime>

namespace EasePass::Core {

WebcamQrScanController::WebcamQrScanController(MsClock clock, QObject *parent)
    : QObject(parent), m_clock(clock ? std::move(clock) : MsClock([] { return QDateTime::currentMSecsSinceEpoch(); })) {}

bool WebcamQrScanController::submitFrame(const QImage &frame) {
    if (frame.isNull() || m_inFlight || m_detected)
        return false;
    const qint64 now = m_clock();
    if (m_lastStart >= 0 && now - m_lastStart < m_minIntervalMs)
        return false;
    m_lastStart = now;
    m_inFlight = true;
    QrCodeScanner::scanImageAllAsync(frame, this, [this](const QStringList &found) {
        m_inFlight = false;
        ++m_scanned;
        if (!m_detected && !found.isEmpty()) {
            m_detected = true;
            emit codeDetected(found.first());
        }
        emit scanFinished();
    });
    return true;
}

void WebcamQrScanController::reset() {
    m_detected = false;
    m_lastStart = -1;
}

} // namespace EasePass::Core
