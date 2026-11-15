#pragma once

#include "IDatabaseSource.h"
#include "PasswordItem.h"
#include "PluginWorker.h"

#include <QMutex>
#include <QUrl>
#include <functional>
#include <memory>

// Typed front ends for the plugin types of easepass:plugin@1 (spec: wit/plugin.wit). They
// turn plugin values into application types and treat everything a plugin returns as
// untrusted input. Callbacks run on the PluginWorker's thread (normally the UI thread)
// while `context` exists.

namespace EasePass::Plugin {

template <typename T> using PluginCallback = std::function<void(const T &value, const PluginError &error)>;
using PluginDoneCallback = std::function<void(const PluginError &error)>;

/// Type `importer`: the items go to the user for selection and then to
/// Core::DatabaseService::importItems().
struct PluginImportSource {
    QString id;
    QString name;
    QString description;
};

class PluginImporter {
public:
    explicit PluginImporter(std::shared_ptr<PluginWorker> worker) : m_worker(std::move(worker)) {}

    void sources(QObject *context, PluginCallback<QList<PluginImportSource>> callback);
    void importPasswords(const QString &sourceId, QObject *context,
                         PluginCallback<QList<Core::PasswordItem>> callback);

    /// record password-item -> PasswordItem. Returns false if the value has the wrong shape.
    static bool toPasswordItem(const Value &record, Core::PasswordItem *item);

private:
    std::shared_ptr<PluginWorker> m_worker;
};

/// Type `password-generator`.
class PluginPasswordGenerator {
public:
    static constexpr int MaxLength = 4096;

    explicit PluginPasswordGenerator(std::shared_ptr<PluginWorker> worker) : m_worker(std::move(worker)) {}

    /// Fails (Kind::Host) if the plugin returns an empty or absurdly long password.
    void generate(int length, const QString &characters, bool verifyNotLeaked, QObject *context,
                  PluginCallback<QString> callback);

private:
    std::shared_ptr<PluginWorker> m_worker;
};

/// Type `store-feed`: additional plugin catalogs.
struct PluginFeedSource {
    QString name;
    QUrl url; ///< always https
};

class PluginStoreFeed {
public:
    explicit PluginStoreFeed(std::shared_ptr<PluginWorker> worker) : m_worker(std::move(worker)) {}
    /// Sources whose URL is not a valid https URL are dropped.
    void feedSources(QObject *context, PluginCallback<QList<PluginFeedSource>> callback);

private:
    std::shared_ptr<PluginWorker> m_worker;
};

/**
 * One database of a `database-source` plugin as Core::IDatabaseSource. The plugin only
 * ever receives the ENCRYPTED database bytes.
 *
 * Property getters return the state last reported by the plugin; refresh() polls it
 * (the plugin interface has no change callback) and emits changed().
 */
class PluginDatabaseSource : public Core::IDatabaseSource {
    Q_OBJECT
public:
    static constexpr int TransferTimeoutMs = 120'000;

    struct Info {
        QString id;
        QString name;
        QString description;
        bool readOnly = false;
        Availability availability = Availability::UnknownState;
        QDateTime lastModified;
    };

    PluginDatabaseSource(std::shared_ptr<PluginWorker> worker, Info info, QObject *parent = nullptr);

    QString databaseId() const;
    QString databaseName() const override;
    QString sourceDescription() const override;
    bool isReadOnly() const override;
    Availability availability() const override;
    QDateTime lastModified() const override;

    std::optional<QByteArray> getBytes() override;
    bool saveBytes(const QByteArray &bytes) override;
    void login() override;
    void logout() override;

    /// Error of the last getBytes/saveBytes/login/logout (ok() after a success).
    PluginError lastError() const;
    /// Asks the plugin for the current state; emits changed() if something differs.
    void refresh();

    static bool toInfo(const Value &record, Info *info);

private:
    void refreshBlocking();
    void apply(const Info &info);
    void setLastError(const PluginError &error);

    std::shared_ptr<PluginWorker> m_worker;
    mutable QMutex m_mutex;
    Info m_info;
    PluginError m_lastError;
};

/// Type `database-source`, provider level: configuration and the list of databases. The
/// provider is shown under the plugin's name (manifest).
class PluginDatabaseProvider {
public:
    explicit PluginDatabaseProvider(std::shared_ptr<PluginWorker> worker) : m_worker(std::move(worker)) {}

    /// JSON Schema of the configuration (the host renders a form from it).
    void configSchema(QObject *context, PluginCallback<QString> callback);
    void config(QObject *context, PluginCallback<QString> callback);
    void setConfig(const QString &configJson, QObject *context, PluginDoneCallback callback);
    void listDatabases(QObject *context, PluginCallback<QList<std::shared_ptr<PluginDatabaseSource>>> callback);

private:
    std::shared_ptr<PluginWorker> m_worker;
};

} // namespace EasePass::Plugin
