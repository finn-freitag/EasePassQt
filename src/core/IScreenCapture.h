#pragma once

#include <QImage>
#include <QRect>
#include <QString>
#include <functional>
#include <memory>

namespace EasePass::Core {

struct ScreenCaptureResult {
    QImage image;          ///< device pixels; devicePixelRatio set when known
    QRect logicalGeometry; ///< virtual-desktop area (logical coords) the image shows
    bool cancelled = false;
    QString errorString;
    bool ok() const { return !image.isNull(); }
};

/**
 * Screen capture backend (D4). capture() is asynchronous; the callback runs on the thread
 * that called capture() (usually the UI thread) and is invoked exactly once.
 */
class IScreenCapture {
public:
    using Callback = std::function<void(const ScreenCaptureResult &)>;
    virtual ~IScreenCapture() = default;
    virtual QString name() const = 0;
    virtual bool isAvailable() const = 0;
    virtual void capture(Callback callback) = 0;
};

/// X11 / Windows / macOS: QScreen::grabWindow(0) for every screen, composed into one image.
class QtScreenCapture : public IScreenCapture {
public:
    QString name() const override { return QStringLiteral("QScreen"); }
    bool isAvailable() const override;
    void capture(Callback callback) override;
};

enum class ScreenCaptureBackendKind { QtScreen, Portal, None };

/**
 * Backend choice as a pure function: Wayland needs the xdg-desktop-portal (grabWindow is black
 * there, P1); everything else uses QScreen. Wayland without portal -> None (feature degrades to
 * "scan file / clipboard").
 */
ScreenCaptureBackendKind chooseScreenCaptureBackend(const QString &platformName, bool portalAvailable);

/// Creates the backend for the running platform (may return nullptr if none is usable).
std::unique_ptr<IScreenCapture> createScreenCapture();

} // namespace EasePass::Core
