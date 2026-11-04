#include "PortalScreenCapture.h"

#include "ScreenGeometry.h"

#include <QCoreApplication>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QRandomGenerator>
#include <QScreen>
#include <QTimer>

#ifdef EASEPASS_HAVE_PORTAL
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusObjectPath>
#endif

namespace EasePass::Core {

namespace PortalScreenshot {

QString requestObjectPath(const QString &senderUniqueName, const QString &token) {
    QString sender = senderUniqueName;
    if (sender.startsWith(QLatin1Char(':')))
        sender.remove(0, 1);
    sender.replace(QLatin1Char('.'), QLatin1Char('_'));
    return QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token);
}

Response parseResponse(uint response, const QVariantMap &results) {
    Response r;
    if (response == 1) { // user cancelled
        r.status = Response::Status::Cancelled;
        return r;
    }
    if (response != 0) {
        r.errorString = QCoreApplication::translate("ScreenCapture", "The screenshot portal reported an error.");
        return r;
    }
    const QUrl uri(results.value(QStringLiteral("uri")).toString());
    if (!uri.isValid() || !uri.isLocalFile() || uri.toLocalFile().isEmpty()) {
        r.errorString = QCoreApplication::translate("ScreenCapture", "The screenshot portal returned no file.");
        return r;
    }
    r.status = Response::Status::Success;
    r.localFile = uri.toLocalFile();
    return r;
}

} // namespace PortalScreenshot

#ifdef EASEPASS_HAVE_PORTAL

namespace {
const QString kService = QStringLiteral("org.freedesktop.portal.Desktop");
const QString kPath = QStringLiteral("/org/freedesktop/portal/desktop");
const QString kRequestIface = QStringLiteral("org.freedesktop.portal.Request");

/// Receives the Response signal of one portal request; deletes itself after finishing.
class PortalRequest : public QObject {
    Q_OBJECT
public:
    PortalRequest(QString path, IScreenCapture::Callback cb) : m_path(std::move(path)), m_callback(std::move(cb)) {
        m_timeout.setSingleShot(true);
        connect(&m_timeout, &QTimer::timeout, this, [this]() {
            ScreenCaptureResult r;
            r.errorString = QCoreApplication::translate("ScreenCapture", "The screenshot portal did not respond.");
            finish(r);
        });
        m_timeout.start(PortalScreenCapture::TimeoutMs);
    }

    bool subscribe() {
        return QDBusConnection::sessionBus().connect(kService, m_path, kRequestIface, QStringLiteral("Response"), this,
                                                      SLOT(onResponse(uint, QVariantMap)));
    }

    void finish(const ScreenCaptureResult &result) {
        if (m_done)
            return;
        m_done = true;
        QDBusConnection::sessionBus().disconnect(kService, m_path, kRequestIface, QStringLiteral("Response"), this,
                                                 SLOT(onResponse(uint, QVariantMap)));
        m_callback(result);
        deleteLater();
    }

public slots:
    void onResponse(uint response, const QVariantMap &results) {
        const auto parsed = PortalScreenshot::parseResponse(response, results);
        ScreenCaptureResult r;
        if (parsed.status == PortalScreenshot::Response::Status::Cancelled) {
            r.cancelled = true;
        } else if (parsed.status == PortalScreenshot::Response::Status::Failed) {
            r.errorString = parsed.errorString;
        } else {
            r.image = QImage(parsed.localFile);
            // The screenshot may show a 2FA secret: never leave it on disk.
            QFile::remove(parsed.localFile);
            QList<ScreenGeometry::ScreenInfo> infos;
            for (QScreen *s : QGuiApplication::screens())
                infos.append({s->geometry(), s->devicePixelRatio()});
            r.logicalGeometry = ScreenGeometry::unionGeometry(infos);
            if (r.logicalGeometry.isEmpty() && !r.image.isNull())
                r.logicalGeometry = QRect(QPoint(0, 0), r.image.size());
            if (r.image.isNull())
                r.errorString = QCoreApplication::translate("ScreenCapture", "The screenshot file could not be read.");
        }
        finish(r);
    }

private:
    QString m_path;
    IScreenCapture::Callback m_callback;
    QTimer m_timeout;
    bool m_done = false;
};
} // namespace

bool PortalScreenCapture::serviceAvailable() {
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || !bus.interface())
        return false;
    // Activatable services count as available, too.
    if (bus.interface()->isServiceRegistered(kService))
        return true;
    const QStringList activatable = bus.interface()->activatableServiceNames().value();
    return activatable.contains(kService);
}

void PortalScreenCapture::capture(Callback callback) {
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        ScreenCaptureResult r;
        r.errorString = QCoreApplication::translate("ScreenCapture", "No D-Bus session bus.");
        QTimer::singleShot(0, QCoreApplication::instance(), [callback, r]() { callback(r); });
        return;
    }

    const QString token = QStringLiteral("easepass%1").arg(QRandomGenerator::global()->generate());
    const QString path = PortalScreenshot::requestObjectPath(bus.baseService(), token);
    auto *request = new PortalRequest(path, std::move(callback));
    // Subscribe before calling to avoid missing a fast Response.
    request->subscribe();

    QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath, QStringLiteral("org.freedesktop.portal.Screenshot"),
                                                      QStringLiteral("Screenshot"));
    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), token);
    options.insert(QStringLiteral("interactive"), false);
    options.insert(QStringLiteral("modal"), true);
    msg << QString() << options;

    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(msg), request);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, request, [request](QDBusPendingCallWatcher *w) {
        QDBusPendingReply<QDBusObjectPath> reply = *w;
        w->deleteLater();
        if (reply.isError()) {
            ScreenCaptureResult r;
            r.errorString = reply.error().message();
            request->finish(r);
        }
        // Older portals may return a different handle path; we keep listening on ours, which
        // all current implementations honour (handle_token).
    });
}

#endif // EASEPASS_HAVE_PORTAL

} // namespace EasePass::Core

#ifdef EASEPASS_HAVE_PORTAL
#include "PortalScreenCapture.moc"
#endif
