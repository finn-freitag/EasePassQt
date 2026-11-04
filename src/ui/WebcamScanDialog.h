#pragma once

#include <QDialog>

class QComboBox;
class QLabel;

namespace EasePass::Core {
class IWebcamScanner;
}

namespace EasePass::UI {

/// Webcam QR scanner window (C#: AppWindows/WebcamScannerWindow.xaml.cs). Only built with Qt Multimedia.
class WebcamScanDialog : public QDialog {
    Q_OBJECT
public:
    explicit WebcamScanDialog(QWidget *parent = nullptr);
    ~WebcamScanDialog() override;

    QString scannedResult() const { return m_result; }

private:
    void startSelected();

    Core::IWebcamScanner *m_scanner = nullptr;
    QComboBox *m_devices = nullptr;
    QLabel *m_preview = nullptr;
    QLabel *m_status = nullptr;
    QString m_result;
};

} // namespace EasePass::UI
