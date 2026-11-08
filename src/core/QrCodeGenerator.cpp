#include "QrCodeGenerator.h"

// zxing-cpp 3 has <ZXing/Version.h> (ZXVersion.h is deprecated there); 2.x only ZXVersion.h.
#if __has_include(<ZXing/Version.h>)
#include <ZXing/Version.h>
#else
#include <ZXing/ZXVersion.h>
#endif

// zxing-cpp 3 has two writer backends, chosen at its build time (ZXING_WRITERS=OLD/NEW/BOTH):
// the new one (CreateBarcode/WriteBarcode, zint) defines ZXING_USE_ZINT in Version.h. Nix
// builds BOTH, the vcpkg port (nu-book-zxing-cpp) only OLD, and 2.x only has the old writer.
// The new API's headers exist in every 3.x build, but without zint its functions are not
// compiled in, so the macro (not the major version) decides. Define
// EASEPASS_ZXING_FORCE_OLD_WRITER to test the old path on a BOTH build (e.g. Nix).
#if defined(ZXING_USE_ZINT) && !defined(EASEPASS_ZXING_FORCE_OLD_WRITER)
#define EASEPASS_ZXING_NEW_WRITER 1
#include <ZXing/CreateBarcode.h>
#include <ZXing/WriteBarcode.h>
#else
#include <ZXing/BitMatrix.h>
#include <ZXing/MultiFormatWriter.h>
#endif

#include <exception>
#include <vector>

namespace EasePass::Core {

namespace {
constexpr int QuietZone = 4;

// Module matrix (true = dark), row-major; empty on failure.
std::vector<bool> encode(const QString &text, int *size) {
    *size = 0;
    if (text.isEmpty())
        return {};
    const std::string utf8 = text.toStdString();
    try {
#ifdef EASEPASS_ZXING_NEW_WRITER
        // zxing-cpp 3 (zint backend): render at scale 1 without quiet zone -> one pixel per module.
        const auto barcode = ZXing::CreateBarcodeFromText(utf8, ZXing::CreatorOptions(ZXing::BarcodeFormat::QRCode));
        if (!barcode.isValid())
            return {};
        const ZXing::Image img = ZXing::WriteBarcodeToImage(barcode, ZXing::WriterOptions().scale(1).addQuietZones(false));
        const int w = img.width(), h = img.height();
        if (w <= 0 || w != h)
            return {};
        std::vector<bool> out(size_t(w) * size_t(h));
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                out[size_t(y) * size_t(w) + size_t(x)] = img.data(x, y)[0] < 128;
        *size = w;
        return out;
#else
        // Deprecated in zxing-cpp 3, but the only writer of builds without zint (vcpkg).
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
        ZXing::MultiFormatWriter writer(ZXing::BarcodeFormat::QRCode);
        writer.setMargin(0);
        writer.setEncoding(ZXing::CharacterSet::UTF8);
        const ZXing::BitMatrix m = writer.encode(utf8, 0, 0);
#if defined(_MSC_VER)
#pragma warning(pop)
#else
#pragma GCC diagnostic pop
#endif
        const int w = m.width();
        if (w <= 0 || w != m.height())
            return {};
        std::vector<bool> out(size_t(w) * size_t(w));
        for (int y = 0; y < w; ++y)
            for (int x = 0; x < w; ++x)
                out[size_t(y) * size_t(w) + size_t(x)] = m.get(x, y);
        *size = w;
        return out;
#endif
    } catch (const std::exception &) {
        return {}; // too long for a QR code etc.
    }
}
} // namespace

int QrCodeGenerator::moduleCount(const QString &text) {
    int size = 0;
    encode(text, &size);
    return size;
}

QImage QrCodeGenerator::generate(const QString &text, int moduleSize) {
    int size = 0;
    const std::vector<bool> modules = encode(text, &size);
    if (size <= 0)
        return {};
    moduleSize = std::max(1, moduleSize);
    const int side = (size + 2 * QuietZone) * moduleSize;
    QImage img(side, side, QImage::Format_Grayscale8);
    img.fill(255);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            if (!modules[size_t(y) * size_t(size) + size_t(x)])
                continue;
            for (int dy = 0; dy < moduleSize; ++dy) {
                uchar *line = img.scanLine((y + QuietZone) * moduleSize + dy);
                std::fill_n(line + (x + QuietZone) * moduleSize, moduleSize, uchar(0));
            }
        }
    }
    return img;
}

} // namespace EasePass::Core
