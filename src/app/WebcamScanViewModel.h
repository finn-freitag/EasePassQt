#pragma once

#include <QImage>
#include <QMutex>
#include <QObject>
#include <QStringList>
#include <memory>

class QQuickImageProvider;

namespace EasePass::Core {
class IWebcamScanner;
}

namespace EasePass::App {

/**
 * Webcam QR scanning for the 2FA dialog (steps.txt T15, T9; C#: AppWindows/WebcamScannerWindow).
 * Wraps an IWebcamScanner (nullptr = no Qt Multimedia -> available false). The preview is
 * served by an image provider: image://webcam/<frameRevision>.
 */
class WebcamScanViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(QStringList deviceNames READ deviceNames NOTIFY devicesChanged)
    Q_PROPERTY(int deviceIndex READ deviceIndex WRITE setDeviceIndex NOTIFY devicesChanged)
    Q_PROPERTY(int frameRevision READ frameRevision NOTIFY frameChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)
public:
    static inline const QString ProviderId = QStringLiteral("webcam");

    explicit WebcamScanViewModel(std::shared_ptr<Core::IWebcamScanner> scanner, QObject *parent = nullptr);
    ~WebcamScanViewModel() override;

    bool available() const { return m_scanner != nullptr; }
    bool active() const;
    QStringList deviceNames() const { return m_deviceNames; }
    int deviceIndex() const { return m_deviceIndex; }
    void setDeviceIndex(int index);
    int frameRevision() const { return m_frameRevision; }
    QString errorString() const { return m_errorString; }
    QImage currentFrame() const;

    /// Refreshes the device list and starts the selected (or default) camera.
    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();

    /// Provider serving the latest frame; owned by the QQmlEngine after addImageProvider.
    QQuickImageProvider *createImageProvider();

signals:
    void activeChanged();
    void devicesChanged();
    void frameChanged();
    void errorChanged();
    void codeDetected(const QString &text);

private:
    void setError(const QString &message);
    void onFrame(const QImage &frame);

    std::shared_ptr<Core::IWebcamScanner> m_scanner;
    QList<QByteArray> m_deviceIds;
    QStringList m_deviceNames;
    int m_deviceIndex = -1;
    int m_frameRevision = 0;
    QString m_errorString;
    mutable QMutex m_frameMutex;
    QImage m_frame;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};

} // namespace EasePass::App
