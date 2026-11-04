#pragma once

#include "IWebcamScanner.h"

#include <QCamera>
#include <QMediaCaptureSession>
#include <QVideoSink>
#include <memory>

namespace EasePass::Multimedia {

/// IWebcamScanner on Qt Multimedia: QCamera -> QVideoSink -> WebcamQrScanController (T9).
class QtWebcamScanner : public Core::IWebcamScanner {
    Q_OBJECT
public:
    explicit QtWebcamScanner(QObject *parent = nullptr);
    ~QtWebcamScanner() override;

    QList<Core::WebcamDevice> availableDevices() const override;
    void start(const QByteArray &deviceId = {}) override;
    void stop() override;
    bool isActive() const override;

private:
    void onFrame(const QVideoFrame &frame);

    std::unique_ptr<QCamera> m_camera;
    QMediaCaptureSession m_session;
    QVideoSink m_sink;
    Core::WebcamQrScanController m_controller;
};

} // namespace EasePass::Multimedia
