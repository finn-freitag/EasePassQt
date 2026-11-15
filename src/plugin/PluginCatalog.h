#pragma once

#include "PluginManager.h"

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QUrl>
#include <functional>
#include <memory>
#include <optional>

namespace EasePass::Core {
class INetworkClient;
}

namespace EasePass::Plugin {

/// One plugin of a catalog (spec: schemas/catalog.schema.json).
struct CatalogEntry {
    QString id;
    QString name;
    QString description;
    QString author;
    QString version;
    int apiVersion = 0;
    QStringList os; ///< empty = all operating systems
    QUrl downloadUrl; ///< always https
    QUrl iconUrl;     ///< empty or https
    QByteArray sha256; ///< lower-case hex of the exact .epplugin bytes
    bool sensitive = false; ///< display hint only
    QString minAppVersion;
    bool revoked = false;
    QString feed; ///< name of the additional catalog it comes from; empty = official catalog

    bool supportsOs(const QString &os) const { return this->os.isEmpty() || this->os.contains(os); }
};

/// A parsed PluginList.json.
struct PluginCatalogData {
    static constexpr qsizetype MaxBytes = 2 * 1024 * 1024;
    static constexpr int MaxPlugins = 2000;

    QDateTime updated;
    QList<CatalogEntry> plugins;

    /// Validates like the JSON schema: ONE invalid entry makes the whole file invalid.
    /// Unknown properties are ignored; a file with another `schema` value is not read.
    static std::optional<PluginCatalogData> parse(const QByteArray &json, QString *error);
};

/// An additional catalog named by a `store-feed` plugin.
struct CatalogFeed {
    QString name;
    QUrl url;
};

/**
 * Fetches the plugin catalog (steps.txt 4.3, T28) and answers "is this package hash known?".
 *
 *  - HTTPS only, with a timeout and a size cap; the answer is validated before it is used.
 *  - A valid answer is written to `<cacheDirectory>/PluginList.json`. The cache is only
 *    used to DISPLAY the store while the catalog cannot be reached: a hash is "verified"
 *    only against a catalog fetched by the latest refresh(). The one thing taken from a
 *    cached catalog is a revocation (a revoked package stays blocked while offline).
 *  - Additional feeds are read with the same rules, but never make a package "verified".
 */
class CatalogClient : public QObject, public IPluginCatalog {
    Q_OBJECT
public:
    struct Options {
        QUrl url;               ///< official catalog
        QString cacheDirectory; ///< empty = no disk cache
        int timeoutMs = 15'000;
    };
    enum class State {
        Empty,  ///< nothing known (never fetched, no cache)
        Cached, ///< entries come from the disk cache; the last refresh failed or never ran
        Live,   ///< entries come from the latest refresh()
    };
    using DoneCallback = std::function<void(bool ok, const QString &error)>;
    using DownloadCallback = std::function<void(const QByteArray &bytes, const QString &error)>;

    CatalogClient(std::shared_ptr<Core::INetworkClient> network, Options options, QObject *parent = nullptr);

    State state() const { return m_state; }
    bool isRefreshing() const { return m_refreshing; }
    QString lastError() const { return m_lastError; }
    QDateTime updated() const { return m_official.updated; }

    /// Official entries followed by the entries of the additional feeds.
    QList<CatalogEntry> entries() const;
    std::optional<CatalogEntry> officialEntryByHash(const QByteArray &sha256Hex) const;

    void setFeeds(const QList<CatalogFeed> &feeds);

    /// Fetches the official catalog and then the feeds. A second call while one is running
    /// waits for the running one.
    void refresh(DoneCallback done = {});

    PluginVerification verify(const QByteArray &sha256Hex) const override;

    /// Downloads the package of `entry`. The caller must still check the hash (PluginManager
    /// does, with entry.sha256 as expected hash).
    void download(const CatalogEntry &entry, DownloadCallback done);

    static bool isHttps(const QUrl &url);

signals:
    void changed();

private:
    using FetchCallback = std::function<void(const QByteArray &data, const QString &error)>;
    void fetch(const QUrl &url, qsizetype maxBytes, FetchCallback done);
    void fetchNextFeed(int index);
    void finishRefresh(bool ok, const QString &error);
    void readCache();
    QString cacheFile() const;

    std::shared_ptr<Core::INetworkClient> m_network;
    Options m_options;
    State m_state = State::Empty;
    bool m_refreshing = false;
    QString m_lastError;
    PluginCatalogData m_official;
    QList<CatalogFeed> m_feeds;
    QList<CatalogEntry> m_feedEntries;
    QList<CatalogEntry> m_pendingFeedEntries;
    QList<DoneCallback> m_waiting;
};

} // namespace EasePass::Plugin
