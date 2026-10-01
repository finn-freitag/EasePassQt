#pragma once

#include <QObject>
#include <QPixmap>
#include <QMap>
#include <QSet>
#include <QNetworkAccessManager>

namespace EasePass::Core {

class WebsiteIconManager : public QObject {
    Q_OBJECT
public:
    static WebsiteIconManager& instance();

    QPixmap getIcon(const QString& website, int size = 28);
    void requestIcon(const QString& website);
    static QString extractDomain(const QString& website);

signals:
    void iconLoaded(const QString& domain);

private slots:
    void onReplyFinished();

private:
    WebsiteIconManager();
    ~WebsiteIconManager() override = default;

    QString getCachePath(const QString& domain) const;
    void startDownload(const QString& domain, const QUrl& url, bool isFallback = false);

    QNetworkAccessManager m_netManager;
    QMap<QString, QPixmap> m_memoryCache;
    QSet<QString> m_pendingDownloads;
    QString m_cacheDir;
};

} // namespace EasePass::Core
