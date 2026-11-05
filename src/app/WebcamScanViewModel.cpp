#include "WebcamScanViewModel.h"

#include "IWebcamScanner.h"

#include <QQuickImageProvider>

namespace EasePass::App {

using namespace EasePass::Core;

namespace {
// Holds a weak reference: the provider may outlive the view model inside the engine.
class WebcamFrameProvider : public QQuickImageProvider {
public:
    WebcamFrameProvider(WebcamScanViewModel *vm, std::weak_ptr<bool> alive)
        : QQuickImageProvider(QQuickImageProvider::Image), m_vm(vm), m_alive(std::move(alive)) {}
    QImage requestImage(const QString &, QSize *size, const QSize &requestedSize) override {
        QImage img;
        if (auto alive = m_alive.lock())
            img = m_vm->currentFrame();
        if (img.isNull()) {
            img = QImage(16, 9, QImage::Format_RGB32);
            img.fill(Qt::black);
        }
        if (size)
            *size = img.size();
        if (requestedSize.isValid() && !requestedSize.isEmpty())
            img = img.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        return img;
    }

private:
    WebcamScanViewModel *m_vm;
    std::weak_ptr<bool> m_alive;
};
} // namespace

WebcamScanViewModel::WebcamScanViewModel(std::shared_ptr<IWebcamScanner> scanner, QObject *parent)
    : QObject(parent), m_scanner(std::move(scanner)) {
    if (!m_scanner)
        return;
    connect(m_scanner.get(), &IWebcamScanner::activeChanged, this, &WebcamScanViewModel::activeChanged);
    connect(m_scanner.get(), &IWebcamScanner::previewFrame, this, &WebcamScanViewModel::onFrame);
    connect(m_scanner.get(), &IWebcamScanner::errorOccurred, this, &WebcamScanViewModel::setError);
    connect(m_scanner.get(), &IWebcamScanner::codeDetected, this, [this](const QString &text) {
        stop(); // C#: the scanner window closes on the first code
        emit codeDetected(text);
    });
}

WebcamScanViewModel::~WebcamScanViewModel() {
    *m_alive = false;
    if (m_scanner) {
        disconnect(m_scanner.get(), nullptr, this, nullptr);
        m_scanner->stop();
    }
}

bool WebcamScanViewModel::active() const { return m_scanner && m_scanner->isActive(); }

void WebcamScanViewModel::setDeviceIndex(int index) {
    if (index == m_deviceIndex || index < 0 || index >= m_deviceIds.size())
        return;
    m_deviceIndex = index;
    emit devicesChanged();
    if (active()) {
        m_scanner->stop();
        m_scanner->start(m_deviceIds.at(index));
    }
}

QImage WebcamScanViewModel::currentFrame() const {
    QMutexLocker lock(&m_frameMutex);
    return m_frame;
}

void WebcamScanViewModel::start() {
    if (!m_scanner)
        return;
    setError(QString());
    const QByteArray previous = m_deviceIndex >= 0 && m_deviceIndex < m_deviceIds.size() ? m_deviceIds.at(m_deviceIndex) : QByteArray();
    m_deviceIds.clear();
    m_deviceNames.clear();
    m_deviceIndex = -1;
    const auto devices = m_scanner->availableDevices();
    for (const auto &d : devices) {
        if ((m_deviceIndex < 0 && d.isDefault) || (!previous.isEmpty() && d.id == previous))
            m_deviceIndex = int(m_deviceIds.size());
        m_deviceIds.append(d.id);
        m_deviceNames.append(d.name);
    }
    if (m_deviceIndex < 0 && !m_deviceIds.isEmpty())
        m_deviceIndex = 0;
    emit devicesChanged();
    if (m_deviceIds.isEmpty()) {
        setError(tr("No camera found."));
        return;
    }
    m_scanner->start(m_deviceIds.at(m_deviceIndex));
}

void WebcamScanViewModel::stop() {
    if (m_scanner)
        m_scanner->stop();
    {
        QMutexLocker lock(&m_frameMutex);
        m_frame = QImage(); // the last frame may show a 2FA secret
    }
    ++m_frameRevision;
    emit frameChanged();
}

void WebcamScanViewModel::setError(const QString &message) {
    if (message == m_errorString)
        return;
    m_errorString = message;
    emit errorChanged();
}

void WebcamScanViewModel::onFrame(const QImage &frame) {
    {
        QMutexLocker lock(&m_frameMutex);
        m_frame = frame;
    }
    ++m_frameRevision;
    emit frameChanged();
}

QQuickImageProvider *WebcamScanViewModel::createImageProvider() { return new WebcamFrameProvider(this, m_alive); }

} // namespace EasePass::App
