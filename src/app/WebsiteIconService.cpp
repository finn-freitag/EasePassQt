#include "WebsiteIconService.h"

#include "AppSettings.h"
#include "WebsiteIconManager.h"

#include <QPointer>
#include <QUrl>

namespace EasePass::App {

namespace {
class FaviconProvider : public QQuickImageProvider {
public:
    explicit FaviconProvider(Core::WebsiteIconManager *manager)
        : QQuickImageProvider(QQuickImageProvider::Pixmap), m_manager(manager) {}

    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override {
        // id = <domain>?r=<revision>
        const QString domain = QUrl::fromPercentEncoding(id.section(u'?', 0, 0).toUtf8());
        const int px = requestedSize.isValid() ? std::max(requestedSize.width(), requestedSize.height()) : 32;
        QPixmap p = m_manager ? m_manager->getIcon(domain, px) : QPixmap();
        if (size)
            *size = p.size();
        return p;
    }

private:
    QPointer<Core::WebsiteIconManager> m_manager;
};
} // namespace

WebsiteIconService::WebsiteIconService(Core::WebsiteIconManager *manager, Core::AppSettings *settings, QObject *parent)
    : QObject(parent), m_manager(manager), m_settings(settings) {
    connect(m_manager, &Core::WebsiteIconManager::iconLoaded, this, [this] {
        ++m_revision;
        emit revisionChanged();
    });
    if (m_settings) {
        auto bump = [this] {
            ++m_revision;
            emit revisionChanged();
        };
        connect(m_settings, &Core::AppSettings::showIconsChanged, this, bump);
        connect(m_settings, &Core::AppSettings::fetchFaviconsChanged, this, bump);
    }
}

bool WebsiteIconService::enabled() const { return !m_settings || m_settings->showIcons(); }

QString WebsiteIconService::iconUrl(const QString &website) {
    if (!enabled())
        return {};
    const QString domain = Core::WebsiteIconManager::extractDomain(website);
    if (domain.isEmpty())
        return {};
    // getIcon() answers from the caches or starts a download (only when allowed).
    if (m_manager->getIcon(website, 16).isNull())
        return {};
    return QStringLiteral("image://%1/%2?r=%3")
        .arg(ProviderId, QString::fromUtf8(QUrl::toPercentEncoding(domain)))
        .arg(m_revision);
}

QQuickImageProvider *WebsiteIconService::createImageProvider() const { return new FaviconProvider(m_manager); }

} // namespace EasePass::App
