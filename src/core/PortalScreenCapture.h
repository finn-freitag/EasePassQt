#pragma once

#include <QString>
#include <QUrl>
#include <QVariantMap>

#include "IScreenCapture.h"

namespace EasePass::Core {

/// Pure helpers for the org.freedesktop.portal.Screenshot protocol (testable without D-Bus).
namespace PortalScreenshot {

/// Request object path for a sender unique name (":1.42") and a handle token.
QString requestObjectPath(const QString &senderUniqueName, const QString &token);

struct Response {
    enum class Status { Success, Cancelled, Failed };
    Status status = Status::Failed;
    QString localFile; ///< set on Success
    QString errorString;
};

/// Interprets the Response(u response, a{sv} results) signal arguments.
Response parseResponse(uint response, const QVariantMap &results);

} // namespace PortalScreenshot

#ifdef EASEPASS_HAVE_PORTAL
/**
 * Wayland screen capture through xdg-desktop-portal Screenshot (QtDBus, Linux only, D4).
 * The portal writes a PNG file; it is loaded and then deleted immediately because it may
 * contain a 2FA secret. The image covers the whole desktop; logicalGeometry is the union of
 * the Qt screens.
 */
class PortalScreenCapture : public IScreenCapture {
public:
    static bool serviceAvailable();

    QString name() const override { return QStringLiteral("xdg-desktop-portal"); }
    bool isAvailable() const override { return serviceAvailable(); }
    void capture(Callback callback) override;

    /// Request timeout (the user may need to confirm a portal dialog).
    static constexpr int TimeoutMs = 120000;
};
#endif

} // namespace EasePass::Core
