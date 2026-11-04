#include "QtWebcamScanner.h"

#include <QCameraDevice>
#include <QMediaDevices>
#include <QVideoFrame>

namespace EasePass::Multimedia {

QtWebcamScanner::QtWebcamScanner(QObject *parent) : Core::IWebcamScanner(parent) {
    m_session.setVideoSink(&m_sink);
    connect(&m_sink, &QVideoSink::videoFrameChanged, this, &QtWebcamScanner::onFrame);
    connect(&m_controller, &Core::WebcamQrScanController::codeDetected, this, [this](const QString &text) {
        stop();
        emit codeDetected(text);
    });
}

QtWebcamScanner::~QtWebcamScanner() {
    stop();
}

QList<Core::WebcamDevice> QtWebcamScanner::availableDevices() const {
    QList<Core::WebcamDevice> out;
    const QCameraDevice def = QMediaDevices::defaultVideoInput();
    for (const QCameraDevice &d : QMediaDevices::videoInputs())
        out.append({d.id(), d.description(), d.id() == def.id()});
    return out;
}

void QtWebcamScanner::start(const QByteArray &deviceId) {
    stop();
    QCameraDevice device = QMediaDevices::defaultVideoInput();
    if (!deviceId.isEmpty()) {
        for (const QCameraDevice &d : QMediaDevices::videoInputs())
            if (d.id() == deviceId)
                device = d;
    }
    if (device.isNull()) {
        emit errorOccurred(tr("No camera found."));
        return;
    }
    m_camera = std::make_unique<QCamera>(device);
    connect(m_camera.get(), &QCamera::errorOccurred, this,
            [this](QCamera::Error, const QString &msg) { emit errorOccurred(msg); });
    connect(m_camera.get(), &QCamera::activeChanged, this, &QtWebcamScanner::activeChanged);
    m_session.setCamera(m_camera.get());
    m_controller.reset();
    m_camera->start();
}

void QtWebcamScanner::stop() {
    if (!m_camera)
        return;
    m_camera->stop();
    m_session.setCamera(nullptr);
    m_camera.reset();
    emit activeChanged(false);
}

bool QtWebcamScanner::isActive() const {
    return m_camera && m_camera->isActive();
}

void QtWebcamScanner::onFrame(const QVideoFrame &frame) {
    if (!frame.isValid())
        return;
    const QImage image = frame.toImage();
    if (image.isNull())
        return;
    emit previewFrame(image.width() > 640 ? image.scaledToWidth(640, Qt::FastTransformation) : image);
    m_controller.submitFrame(image);
}

} // namespace EasePass::Multimedia
