#include "QrCodeScanner.h"

#include <QGuiApplication>
#include <QClipboard>
#include <QPointer>
#include <QThreadPool>
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

QStringList QrCodeScanner::scanImageAll(const QImage& image) {
    QStringList out;
    if (image.isNull()) return out;

    QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
    ZXing::ImageView iv(gray.constBits(), gray.width(), gray.height(),
                        ZXing::ImageFormat::Lum, gray.bytesPerLine());

    ZXing::ReaderOptions options;
    options.setFormats(ZXing::BarcodeFormat::QRCode);
    options.setTryHarder(true);
    options.setTryRotate(true);
    options.setTryInvert(true);

    for (const auto& result : ZXing::ReadBarcodes(iv, options)) {
        if (!result.isValid()) continue;
        const QString text = QString::fromStdString(result.text());
        if (!text.isEmpty() && !out.contains(text)) out.append(text);
    }
    return out;
}

void QrCodeScanner::scanImageAllAsync(const QImage& image, QObject* context,
                                      std::function<void(const QStringList&)> callback) {
    QPointer<QObject> ctx(context);
    QThreadPool::globalInstance()->start([image, ctx, callback = std::move(callback)]() {
        const QStringList found = scanImageAll(image);
        if (!ctx) return;
        QMetaObject::invokeMethod(ctx, [ctx, callback, found]() {
            if (ctx && callback) callback(found);
        }, Qt::QueuedConnection);
    });
}

QrScanDecision QrScanDecision::decide(const QStringList& found) {
    QStringList distinct;
    for (const QString& s : found)
        if (!s.isEmpty() && !distinct.contains(s)) distinct.append(s);
    QrScanDecision d;
    if (distinct.size() == 1) {
        d.kind = Kind::Accept;
        d.text = distinct.first();
    } else if (distinct.size() > 1) {
        d.kind = Kind::UserSelect;
        d.candidates = distinct;
    }
    return d;
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
