#include "WebsiteIconManager.h"

#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QCryptographicHash>
#include <QImageReader>
#include <QBuffer>

namespace EasePass::Core {

WebsiteIconManager& WebsiteIconManager::instance() {
    static WebsiteIconManager inst;
    return inst;
}

WebsiteIconManager::WebsiteIconManager() {
    QString cacheBase = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cacheBase.isEmpty()) {
        cacheBase = QDir::homePath() + "/.cache/EasePass";
    }
    m_cacheDir = cacheBase + "/icons";
    QDir().mkpath(m_cacheDir);
}

QString WebsiteIconManager::extractDomain(const QString& website) {
    QString s = website.trimmed();
    if (s.isEmpty()) return QString();

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

QString WebsiteIconManager::getCachePath(const QString& domain) const {
    QByteArray hash = QCryptographicHash::hash(domain.toUtf8(), QCryptographicHash::Md5).toHex();
    return m_cacheDir + "/" + QString::fromLatin1(hash) + ".png";
}

QPixmap WebsiteIconManager::getIcon(const QString& website, int size) {
    QString domain = extractDomain(website);
    if (domain.isEmpty()) return QPixmap();

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

void WebsiteIconManager::requestIcon(const QString& website) {
    QString domain = extractDomain(website);
    if (domain.isEmpty() || m_pendingDownloads.contains(domain)) {
        return;
    }

    m_pendingDownloads.insert(domain);
    // Attempt 1: direct favicon
    QUrl url("https://" + domain + "/favicon.ico");
    startDownload(domain, url, false);
}

void WebsiteIconManager::startDownload(const QString& domain, const QUrl& url, bool isFallback) {
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0 (X11; Linux x86_64) EasePass/1.4");

    QNetworkReply* reply = m_netManager.get(request);
    reply->setProperty("domain", domain);
    reply->setProperty("isFallback", isFallback);

    connect(reply, &QNetworkReply::finished, this, &WebsiteIconManager::onReplyFinished);
}

void WebsiteIconManager::onReplyFinished() {
    auto* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();

    QString domain = reply->property("domain").toString();
    bool isFallback = reply->property("isFallback").toBool();

    if (reply->error() == QNetworkReply::NoError && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
        QByteArray data = reply->readAll();
        if (data.size() >= 10) {
            QPixmap pix;
            if (pix.loadFromData(data)) {
                m_memoryCache[domain] = pix;
                pix.save(getCachePath(domain), "PNG");
                m_pendingDownloads.remove(domain);
                emit iconLoaded(domain);
                return;
            }
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
