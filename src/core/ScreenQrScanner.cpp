#include "ScreenQrScanner.h"

#include "ScreenGeometry.h"

#include <QCoreApplication>
#include <QTimer>

namespace EasePass::Core {

ScreenQrScanner::ScreenQrScanner(std::shared_ptr<IScreenCapture> capture, QObject *parent)
    : QObject(parent), m_backend(std::move(capture)) {}

void ScreenQrScanner::emitLater(const Outcome &o) {
    QPointer<ScreenQrScanner> self(this);
    QTimer::singleShot(0, this, [self, o]() {
        if (self)
            emit self->finished(o);
    });
}

void ScreenQrScanner::start() {
    if (m_busy) {
        emitLater({Status::Busy, {}, {}, {}});
        return;
    }
    if (!m_backend || !m_backend->isAvailable()) {
        emitLater({Status::Failed, {}, {}, tr("Screen capture is not available on this system. Scan a file or the clipboard instead.")});
        return;
    }
    m_busy = true;
    QPointer<ScreenQrScanner> self(this);
    m_backend->capture([self](const ScreenCaptureResult &result) {
        if (!self)
            return;
        self->m_capture = result;
        if (result.cancelled) {
            self->m_busy = false;
            emit self->finished({Status::Cancelled, {}, {}, {}});
            return;
        }
        if (!result.ok()) {
            self->m_busy = false;
            emit self->finished({Status::Failed, {}, {}, result.errorString});
            return;
        }
        self->scanImage(result.image);
    });
}

void ScreenQrScanner::scanRegion(const QRect &selectionLogical) {
    if (m_busy) {
        emitLater({Status::Busy, {}, {}, {}});
        return;
    }
    const QRect px =
        ScreenGeometry::mapSelectionToImage(selectionLogical, m_capture.logicalGeometry, m_capture.image.size());
    if (px.isEmpty()) {
        emitLater({Status::NoneFound, {}, {}, {}});
        return;
    }
    m_busy = true;
    scanImage(m_capture.image.copy(px));
}

void ScreenQrScanner::scanImage(const QImage &image) {
    QrCodeScanner::scanImageAllAsync(image, this, [this](const QStringList &found) {
        m_busy = false;
        const QrScanDecision d = QrScanDecision::decide(found);
        Outcome o;
        switch (d.kind) {
        case QrScanDecision::Kind::Accept:
            o.status = Status::Found;
            o.text = d.text;
            break;
        case QrScanDecision::Kind::UserSelect:
            o.status = Status::Multiple;
            o.candidates = d.candidates;
            break;
        case QrScanDecision::Kind::NoneFound:
            o.status = Status::NoneFound;
            break;
        }
        emit finished(o);
    });
}

void ScreenQrScanner::clear() {
    m_capture = ScreenCaptureResult{};
}

} // namespace EasePass::Core
