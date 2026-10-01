#include "QrCodeScanner.h"

#include <QGuiApplication>
#include <QClipboard>
#include <ZXing/ReadBarcode.h>
#include <ZXing/ImageView.h>

namespace EasePass::Core {

QString QrCodeScanner::scanImage(const QImage& image) {
    if (image.isNull()) return QString();

    QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
    ZXing::ImageView iv(gray.constBits(), gray.width(), gray.height(),
                        ZXing::ImageFormat::Lum, gray.bytesPerLine());

    ZXing::ReaderOptions options;
    options.setFormats(ZXing::BarcodeFormat::QRCode);
    options.setTryHarder(true);
    options.setTryRotate(true);
    options.setTryInvert(true);

    auto result = ZXing::ReadBarcode(iv, options);
    if (result.isValid()) {
        return QString::fromStdString(result.text());
    }

    return QString();
}

QString QrCodeScanner::scanImageFile(const QString& filePath) {
    QImage img(filePath);
    return scanImage(img);
}

QString QrCodeScanner::scanClipboard() {
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (!clipboard) return QString();

    QString text = clipboard->text().trimmed();
    if (text.startsWith("otpauth://", Qt::CaseInsensitive)) {
        return text;
    }

    QImage img = clipboard->image();
    if (!img.isNull()) {
        return scanImage(img);
    }

    return QString();
}

} // namespace EasePass::Core
