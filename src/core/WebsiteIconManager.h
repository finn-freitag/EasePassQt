#pragma once

#include <QObject>
#include <QPixmap>
#include <QMap>
#include <QSet>
#include <QPointer>
#include <memory>
#include "INetworkClient.h"

namespace EasePass::Core {

class AppSettings;

/**
 * Favicon fetching with memory/disk cache. Network access only happens when
 * AppSettings::fetchFavicons is on (default off, P8); without settings the default applies.
 * Already cached icons are still returned when fetching is off (no network involved).
 */
class WebsiteIconManager : public QObject {
    Q_OBJECT
public:
    explicit WebsiteIconManager(std::shared_ptr<INetworkClient> client = nullptr);
    ~WebsiteIconManager() override = default;

    QPixmap getIcon(const QString &website, int size = 28);
    void requestIcon(const QString &website);
    static QString extractDomain(const QString &website);

    void setNetworkClient(std::shared_ptr<INetworkClient> client);
    void setCacheDir(const QString &dir);
    void clearCache();

    void setSettings(AppSettings *settings);
    bool fetchingAllowed() const;

signals:
    void iconLoaded(const QString &domain);

private:
    QString getCachePath(const QString &domain) const;
    void startDownload(const QString &domain, const QUrl &url, bool isFallback = false);
    void handleResponse(const QString &domain, const QUrl &url, bool isFallback, const NetworkResponse &resp);

    std::shared_ptr<INetworkClient> m_netClient;
    QMap<QString, QPixmap> m_memoryCache;
    QSet<QString> m_pendingDownloads;
    QString m_cacheDir;
    QPointer<AppSettings> m_settings;
};

} // namespace EasePass::Core
