#include "WebsiteIconManager.h"
#include "AppSettings.h"

#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QUrl>
#include <QNetworkRequest>
#include <QCryptographicHash>

namespace EasePass::Core {

WebsiteIconManager::WebsiteIconManager(std::shared_ptr<INetworkClient> client) : m_netClient(std::move(client)) {
    if (!m_netClient) {
        m_netClient = std::make_shared<QtNetworkClient>(this);
    }
    QString cacheBase = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cacheBase.isEmpty()) {
        cacheBase = QDir::homePath() + "/.cache/EasePass";
    }
    m_cacheDir = cacheBase + "/icons";
    QDir().mkpath(m_cacheDir);
}

void WebsiteIconManager::setNetworkClient(std::shared_ptr<INetworkClient> client) {
    m_netClient = std::move(client);
}

void WebsiteIconManager::setCacheDir(const QString &dir) {
    m_cacheDir = dir;
    QDir().mkpath(m_cacheDir);
}

void WebsiteIconManager::setSettings(AppSettings *settings) {
    m_settings = settings;
}

bool WebsiteIconManager::fetchingAllowed() const {
    return m_settings ? m_settings->fetchFavicons() : AppSettings::Defaults::FetchFavicons;
}

void WebsiteIconManager::clearCache() {
    m_memoryCache.clear();
    m_pendingDownloads.clear();
}

QString WebsiteIconManager::extractDomain(const QString &website) {
    QString s = website.trimmed();
    if (s.isEmpty())
        return QString();

    if (!s.startsWith("http://", Qt::CaseInsensitive) && !s.startsWith("https://", Qt::CaseInsensitive)) {
        s = "https://" + s;
    }

    QUrl url = QUrl::fromUserInput(s);
    QString host = url.host();
    if (host.startsWith("www.", Qt::CaseInsensitive)) {
        host = host.mid(4);
    }
    return host.toLower();
}

QString WebsiteIconManager::getCachePath(const QString &domain) const {
    QByteArray hash = QCryptographicHash::hash(domain.toUtf8(), QCryptographicHash::Md5).toHex();
    return m_cacheDir + "/" + QString::fromLatin1(hash) + ".png";
}

QPixmap WebsiteIconManager::getIcon(const QString &website, int size) {
    QString domain = extractDomain(website);
    if (domain.isEmpty())
        return QPixmap();

    if (m_memoryCache.contains(domain)) {
        QPixmap p = m_memoryCache.value(domain);
        if (!p.isNull()) {
            return p.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }

    QString diskPath = getCachePath(domain);
    if (QFile::exists(diskPath)) {
        QPixmap p(diskPath);
        if (!p.isNull()) {
            m_memoryCache[domain] = p;
            return p.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }

    requestIcon(website);
    return QPixmap();
}

void WebsiteIconManager::requestIcon(const QString &website) {
    QString domain = extractDomain(website);
    // Privacy (P8): never contact the site or the fallback service unless the user opted in.
    if (domain.isEmpty() || m_pendingDownloads.contains(domain) || !fetchingAllowed()) {
        return;
    }

    m_pendingDownloads.insert(domain);
    // Attempt 1: direct favicon
    QUrl url("https://" + domain + "/favicon.ico");
    startDownload(domain, url, false);
}

void WebsiteIconManager::startDownload(const QString &domain, const QUrl &url, bool isFallback) {
    if (!m_netClient || !fetchingAllowed()) {
        m_pendingDownloads.remove(domain);
        return;
    }

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0 (X11; Linux x86_64) EasePass/1.4");

    m_netClient->get(request, [this, domain, url, isFallback](const NetworkResponse &resp) {
        handleResponse(domain, url, isFallback, resp);
    });
}

void WebsiteIconManager::handleResponse(const QString &domain, const QUrl & /*url*/, bool isFallback,
                                        const NetworkResponse &resp) {
    if (resp.statusCode == 200 && resp.data.size() >= 10) {
        QPixmap pix;
        if (pix.loadFromData(resp.data)) {
            m_memoryCache[domain] = pix;
            pix.save(getCachePath(domain), "PNG");
            m_pendingDownloads.remove(domain);
            emit iconLoaded(domain);
            return;
        }
    }

    if (!isFallback) {
        // Fallback to Google Favicon service
        QUrl fallbackUrl("https://www.google.com/s2/favicons?domain=" + domain + "&sz=64");
        startDownload(domain, fallbackUrl, true);
    } else {
        m_pendingDownloads.remove(domain);
    }
}

} // namespace EasePass::Core
