#pragma once

#include <QString>
#include <QImage>

namespace EasePass::Core {

class QrCodeScanner {
public:
    static QString scanImage(const QImage& image);
    static QString scanImageFile(const QString& filePath);
    static QString scanClipboard();
};

} // namespace EasePass::Core
