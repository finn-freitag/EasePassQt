#pragma once

#include <QObject>
#include <QPointer>
#include <memory>

#include "IScreenCapture.h"
#include "QrCodeScanner.h"

namespace EasePass::Core {

/**
 * Capture -> worker-thread ZXing scan -> decision (T9). UI-independent: the UI shows the
 * capture for manual region selection when the outcome is not Accept, then calls
 * scanRegion() with the selection in global logical coordinates.
 */
class ScreenQrScanner : public QObject {
    Q_OBJECT
public:
    enum class Status { Found, NoneFound, Multiple, Cancelled, Failed, Busy };
    Q_ENUM(Status)

    struct Outcome {
        Status status = Status::Failed;
        QString text;           ///< Found
        QStringList candidates; ///< Multiple
        QString errorString;    ///< Failed
    };

    explicit ScreenQrScanner(std::shared_ptr<IScreenCapture> capture, QObject *parent = nullptr);

    bool isBusy() const { return m_busy; }
    /// Capture of the last start(); valid after a NoneFound/Multiple/Found outcome.
    const ScreenCaptureResult &lastCapture() const { return m_capture; }

    /// Captures the screen and scans the whole image. Emits finished().
    void start();
    /// Scans part of lastCapture() (global logical coordinates). Emits finished().
    void scanRegion(const QRect &selectionLogical);
    /// Drops the stored screenshot (call when the UI closes).
    void clear();

signals:
    void finished(const ScreenQrScanner::Outcome &outcome);

private:
    void scanImage(const QImage &image);
    void emitLater(const Outcome &o);

    std::shared_ptr<IScreenCapture> m_backend;
    ScreenCaptureResult m_capture;
    bool m_busy = false;
};

} // namespace EasePass::Core
