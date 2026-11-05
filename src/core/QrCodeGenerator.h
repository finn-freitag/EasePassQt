#pragma once

#include <QImage>
#include <QString>

namespace EasePass::Core {

/**
 * Encodes text as a QR code image (steps.txt T15; C#: Helper/QRCodeScanner.GenerateQRCode,
 * used by "Export 2FA"). The image is black on white, @p moduleSize pixels per module, with
 * a 4-module quiet zone. Returns a null image for empty or too long input.
 */
class QrCodeGenerator {
public:
    static QImage generate(const QString &text, int moduleSize = 8);
    /// Number of modules per side (without quiet zone), 0 on failure. For tests / layout.
    static int moduleCount(const QString &text);
};

} // namespace EasePass::Core
