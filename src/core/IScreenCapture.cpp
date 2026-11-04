#include "IScreenCapture.h"

#include "PortalScreenCapture.h"
#include "ScreenGeometry.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QPixmap>
#include <QScreen>
#include <QTimer>

namespace EasePass::Core {

bool QtScreenCapture::isAvailable() const {
    return qobject_cast<QGuiApplication *>(QCoreApplication::instance()) && !QGuiApplication::screens().isEmpty();
}

void QtScreenCapture::capture(Callback callback) {
    ScreenCaptureResult result;
    if (!isAvailable()) {
        result.errorString = QCoreApplication::translate("ScreenCapture", "No screen available for capturing.");
    } else {
        QList<ScreenGeometry::ScreenImage> parts;
        for (QScreen *screen : QGuiApplication::screens()) {
            ScreenGeometry::ScreenImage part;
            part.screen = {screen->geometry(), screen->devicePixelRatio()};
            part.image = screen->grabWindow(0).toImage();
            parts.append(part);
        }
        QList<ScreenGeometry::ScreenInfo> infos;
        for (const auto &p : parts)
            infos.append(p.screen);
        result.logicalGeometry = ScreenGeometry::unionGeometry(infos);
        result.image = ScreenGeometry::composeScreens(parts);
        if (result.image.isNull())
            result.errorString = QCoreApplication::translate("ScreenCapture", "Capturing the screen failed.");
    }
    // Keep the contract "callback is asynchronous" for all backends.
    QTimer::singleShot(0, QCoreApplication::instance(), [callback, result]() { callback(result); });
}

ScreenCaptureBackendKind chooseScreenCaptureBackend(const QString &platformName, bool portalAvailable) {
    const bool wayland = platformName.startsWith(QLatin1String("wayland"), Qt::CaseInsensitive);
    if (wayland)
        return portalAvailable ? ScreenCaptureBackendKind::Portal : ScreenCaptureBackendKind::None;
    if (platformName == QLatin1String("offscreen") || platformName == QLatin1String("minimal"))
        return ScreenCaptureBackendKind::None;
    return ScreenCaptureBackendKind::QtScreen;
}

std::unique_ptr<IScreenCapture> createScreenCapture() {
    const QString platform = QGuiApplication::platformName();
    bool portal = false;
#ifdef EASEPASS_HAVE_PORTAL
    portal = PortalScreenCapture::serviceAvailable();
#endif
    switch (chooseScreenCaptureBackend(platform, portal)) {
    case ScreenCaptureBackendKind::QtScreen:
        return std::make_unique<QtScreenCapture>();
    case ScreenCaptureBackendKind::Portal:
#ifdef EASEPASS_HAVE_PORTAL
        return std::make_unique<PortalScreenCapture>();
#else
        return nullptr;
#endif
    case ScreenCaptureBackendKind::None:
        break;
    }
    return nullptr;
}

} // namespace EasePass::Core
