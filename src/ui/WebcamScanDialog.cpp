#include "WebcamScanDialog.h"

#include "QtWebcamScanner.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>

namespace EasePass::UI {

WebcamScanDialog::WebcamScanDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("Scan QR code with webcam"));
    m_scanner = new Multimedia::QtWebcamScanner(this);

    auto *layout = new QVBoxLayout(this);
    m_devices = new QComboBox(this);
    m_preview = new QLabel(this);
    m_preview->setMinimumSize(480, 360);
    m_preview->setAlignment(Qt::AlignCenter);
    m_status = new QLabel(tr("Hold the QR code in front of the camera."), this);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    layout->addWidget(m_devices);
    layout->addWidget(m_preview, 1);
    layout->addWidget(m_status);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    int defaultIndex = 0;
    const auto devices = m_scanner->availableDevices();
    for (const auto &d : devices) {
        if (d.isDefault)
            defaultIndex = m_devices->count();
        m_devices->addItem(d.name, d.id);
    }
    if (devices.isEmpty()) {
        m_devices->setEnabled(false);
        m_status->setText(tr("No camera found."));
    }

    connect(m_scanner, &Core::IWebcamScanner::previewFrame, this, [this](const QImage &img) {
        m_preview->setPixmap(QPixmap::fromImage(img).scaled(m_preview->size(), Qt::KeepAspectRatio));
    });
    connect(m_scanner, &Core::IWebcamScanner::errorOccurred, m_status, &QLabel::setText);
    connect(m_scanner, &Core::IWebcamScanner::codeDetected, this, [this](const QString &text) {
        m_result = text;
        accept();
    });

    if (!devices.isEmpty()) {
        m_devices->setCurrentIndex(defaultIndex);
        connect(m_devices, &QComboBox::currentIndexChanged, this, &WebcamScanDialog::startSelected);
        startSelected();
    }
}

WebcamScanDialog::~WebcamScanDialog() {
    m_scanner->stop();
}

void WebcamScanDialog::startSelected() {
    m_scanner->start(m_devices->currentData().toByteArray());
}

} // namespace EasePass::UI
