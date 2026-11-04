#pragma once

#include <QString>
#include <QImage>
#include <QObject>
#include <QStringList>
#include <functional>

namespace EasePass::Core {

class QrCodeScanner {
public:
    static QString scanImage(const QImage& image);
    static QString scanImageFile(const QString& filePath);
    static QString scanClipboard();

    /// All distinct QR texts found in @p image (order of detection).
    static QStringList scanImageAll(const QImage& image);

    /**
     * Runs scanImageAll() on a QThreadPool worker; @p callback is invoked on @p context's
     * thread (dropped if @p context is destroyed first).
     */
    static void scanImageAllAsync(const QImage& image, QObject* context,
                                  std::function<void(const QStringList&)> callback);
};

/// What to do with the codes found in a capture (T9): accept exactly one, else let the user pick.
struct QrScanDecision {
    enum class Kind { NoneFound, Accept, UserSelect };
    Kind kind = Kind::NoneFound;
    QString text;        ///< set for Accept
    QStringList candidates; ///< set for UserSelect

    static QrScanDecision decide(const QStringList& found);
};

} // namespace EasePass::Core
