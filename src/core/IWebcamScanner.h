#pragma once

#include <QImage>
#include <QList>
#include <QObject>
#include <QString>
#include <functional>
#include <algorithm>

namespace EasePass::Core {

struct WebcamDevice {
    QByteArray id;
    QString name;
    bool isDefault = false;
};

/**
 * Webcam QR scanning (T9; C#: AppWindows/WebcamScannerWindow.xaml.cs). The Qt Multimedia
 * implementation lives in src/multimedia (QtWebcamScanner) and is only built when Qt
 * Multimedia is available.
 */
class IWebcamScanner : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~IWebcamScanner() override = default;

    virtual QList<WebcamDevice> availableDevices() const = 0;
    /// Starts the camera (empty id = default device). Errors are reported via errorOccurred().
    virtual void start(const QByteArray &deviceId = {}) = 0;
    virtual void stop() = 0;
    virtual bool isActive() const = 0;

signals:
    void activeChanged(bool active);
    /// Latest preview frame (scaled down) for UI display.
    void previewFrame(const QImage &frame);
    /// A QR code was decoded; the scanner stops itself afterwards.
    void codeDetected(const QString &text);
    void errorOccurred(const QString &message);
};

/**
 * Feeds camera frames to ZXing without blocking the UI: at most one scan in flight and at most
 * one scan per minIntervalMs; frames arriving meanwhile are dropped. Emits codeDetected once
 * per reset(). Independent of Qt Multimedia, so it is unit-testable with QImages.
 */
class WebcamQrScanController : public QObject {
    Q_OBJECT
public:
    using MsClock = std::function<qint64()>;
    explicit WebcamQrScanController(MsClock clock = {}, QObject *parent = nullptr);

    void setMinIntervalMs(int ms) { m_minIntervalMs = std::max(0, ms); }
    /// Returns true if the frame was accepted for scanning.
    bool submitFrame(const QImage &frame);
    void reset();
    bool isScanning() const { return m_inFlight; }
    bool hasDetected() const { return m_detected; }
    int framesScanned() const { return m_scanned; }

signals:
    void codeDetected(const QString &text);
    void scanFinished();

private:
    MsClock m_clock;
    int m_minIntervalMs = 250;
    qint64 m_lastStart = -1;
    bool m_inFlight = false;
    bool m_detected = false;
    int m_scanned = 0;
};

} // namespace EasePass::Core
