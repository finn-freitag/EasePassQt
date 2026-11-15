#include "PluginCatalog.h"

#include "INetworkClient.h"
#include "PluginPackage.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QPointer>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QTimer>

namespace EasePass::Plugin {

namespace {

const QRegularExpression &hashPattern() {
    static const QRegularExpression pattern(QStringLiteral("^[0-9a-f]{64}$"));
    return pattern;
}

/// A string property. `required` = must exist; otherwise a missing value gives "".
bool readString(const QJsonObject &object, const QString &key, bool required, int minLength, int maxLength,
                QString *target, QString *error) {
    const QJsonValue value = object.value(key);
    if (value.isUndefined() && !required)
        return true;
    if (!value.isString()) {
        *error = QStringLiteral("'%1' must be a string").arg(key);
        return false;
    }
    const QString text = value.toString();
    if (text.size() < minLength || text.size() > maxLength) {
        *error = QStringLiteral("'%1' has an invalid length").arg(key);
        return false;
    }
    *target = text;
    return true;
}

bool readBool(const QJsonObject &object, const QString &key, bool *target, QString *error) {
    const QJsonValue value = object.value(key);
    if (value.isUndefined())
        return true;
    if (!value.isBool()) {
        *error = QStringLiteral("'%1' must be true or false").arg(key);
        return false;
    }
    *target = value.toBool();
    return true;
}

bool readHttpsUrl(const QJsonObject &object, const QString &key, bool required, QUrl *target, QString *error) {
    QString text;
    if (!readString(object, key, required, required ? 1 : 0, 2048, &text, error))
        return false;
    if (text.isEmpty() && !required)
        return true;
    const QUrl url(text, QUrl::StrictMode);
    if (!text.startsWith(QLatin1String("https://")) || !CatalogClient::isHttps(url)) {
        *error = QStringLiteral("'%1' must be an https URL").arg(key);
        return false;
    }
    *target = url;
    return true;
}

bool parseEntry(const QJsonObject &object, CatalogEntry *entry, QString *error) {
    static const QRegularExpression version(
        QStringLiteral("^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)(-[0-9A-Za-z.-]+)?$"));
    static const QRegularExpression plainVersion(
        QStringLiteral("^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)$"));

    if (!readString(object, QStringLiteral("id"), true, 1, 128, &entry->id, error))
        return false;
    if (!PluginManifest::isValidId(entry->id)) {
        *error = QStringLiteral("'id' must be a lower-case reverse-DNS name");
        return false;
    }
    if (!readString(object, QStringLiteral("name"), true, 1, 80, &entry->name, error) ||
        !readString(object, QStringLiteral("description"), true, 0, 2000, &entry->description, error) ||
        !readString(object, QStringLiteral("author"), true, 0, 120, &entry->author, error) ||
        !readString(object, QStringLiteral("version"), true, 1, 64, &entry->version, error))
        return false;
    if (!version.match(entry->version).hasMatch()) {
        *error = QStringLiteral("'version' must be major.minor.patch");
        return false;
    }
    const QJsonValue apiVersion = object.value(QStringLiteral("api_version"));
    if (!apiVersion.isDouble() || apiVersion.toDouble() != apiVersion.toInt() || apiVersion.toInt() < 1) {
        *error = QStringLiteral("'api_version' must be a positive integer");
        return false;
    }
    entry->apiVersion = apiVersion.toInt();

    const QJsonValue os = object.value(QStringLiteral("os"));
    if (!os.isUndefined()) {
        if (!os.isArray()) {
            *error = QStringLiteral("'os' must be a list");
            return false;
        }
        static const QStringList known{QStringLiteral("windows"), QStringLiteral("linux"), QStringLiteral("macos")};
        for (const QJsonValue &item : os.toArray()) {
            const QString name = item.toString();
            if (!item.isString() || !known.contains(name) || entry->os.contains(name)) {
                *error = QStringLiteral("'os' may only list windows, linux and macos, each once");
                return false;
            }
            entry->os << name;
        }
    }
    if (!readHttpsUrl(object, QStringLiteral("download_url"), true, &entry->downloadUrl, error) ||
        !readHttpsUrl(object, QStringLiteral("icon_url"), false, &entry->iconUrl, error))
        return false;

    QString hash;
    if (!readString(object, QStringLiteral("sha256"), true, 0, 4096, &hash, error))
        return false;
    if (!hashPattern().match(hash).hasMatch()) {
        *error = QStringLiteral("'sha256' must be 64 lower-case hex digits");
        return false;
    }
    entry->sha256 = hash.toLatin1();

    if (!readBool(object, QStringLiteral("sensitive"), &entry->sensitive, error) ||
        !readBool(object, QStringLiteral("revoked"), &entry->revoked, error) ||
        !readString(object, QStringLiteral("min_app_version"), false, 0, 64, &entry->minAppVersion, error))
        return false;
    if (!entry->minAppVersion.isEmpty() && !plainVersion.match(entry->minAppVersion).hasMatch()) {
        *error = QStringLiteral("'min_app_version' must be major.minor.patch");
        return false;
    }
    return true;
}

} // namespace

std::optional<PluginCatalogData> PluginCatalogData::parse(const QByteArray &json, QString *error) {
    const auto invalid = [&](const QString &message) -> std::optional<PluginCatalogData> {
        if (error)
            *error = QStringLiteral("plugin list: ") + message;
        return std::nullopt;
    };
    if (json.size() > MaxBytes)
        return invalid(QStringLiteral("too large"));
    const QJsonDocument document = QJsonDocument::fromJson(json);
    if (!document.isObject())
        return invalid(QStringLiteral("not a JSON object"));
    const QJsonObject object = document.object();

    const QJsonValue schema = object.value(QStringLiteral("schema"));
    if (!schema.isDouble() || schema.toDouble() != 1.0)
        return invalid(QStringLiteral("unknown format version"));

    static const QRegularExpression timePattern(QStringLiteral(
        "^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}(\\.[0-9]+)?Z$"));
    const QJsonValue updated = object.value(QStringLiteral("updated"));
    PluginCatalogData data;
    if (!updated.isString() || !timePattern.match(updated.toString()).hasMatch())
        return invalid(QStringLiteral("'updated' must be a UTC time"));
    data.updated = QDateTime::fromString(updated.toString(), Qt::ISODateWithMs);
    if (!data.updated.isValid())
        return invalid(QStringLiteral("'updated' must be a UTC time"));

    const QJsonValue plugins = object.value(QStringLiteral("plugins"));
    if (!plugins.isArray())
        return invalid(QStringLiteral("'plugins' must be a list"));
    const QJsonArray array = plugins.toArray();
    if (array.size() > MaxPlugins)
        return invalid(QStringLiteral("too many plugins"));
    for (qsizetype i = 0; i < array.size(); ++i) {
        CatalogEntry entry;
        QString entryError = QStringLiteral("not an object");
        if (!array.at(i).isObject() || !parseEntry(array.at(i).toObject(), &entry, &entryError))
            return invalid(QStringLiteral("plugin %1: %2").arg(i + 1).arg(entryError));
        data.plugins << entry;
    }
    return data;
}

// --- CatalogClient ------------------------------------------------------------------------

CatalogClient::CatalogClient(std::shared_ptr<Core::INetworkClient> network, Options options, QObject *parent)
    : QObject(parent), m_network(std::move(network)), m_options(std::move(options)) {
    readCache();
}

bool CatalogClient::isHttps(const QUrl &url) {
    return url.isValid() && url.scheme() == QLatin1String("https") && !url.host().isEmpty() &&
           url.userInfo().isEmpty();
}

QString CatalogClient::cacheFile() const {
    return m_options.cacheDirectory.isEmpty() ? QString()
                                              : QDir(m_options.cacheDirectory).filePath(QStringLiteral("PluginList.json"));
}

void CatalogClient::readCache() {
    const QString path = cacheFile();
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > PluginCatalogData::MaxBytes)
        return;
    if (const auto data = PluginCatalogData::parse(file.readAll(), nullptr)) {
        m_official = *data;
        m_state = State::Cached;
    }
}

QList<CatalogEntry> CatalogClient::entries() const { return m_official.plugins + m_feedEntries; }

std::optional<CatalogEntry> CatalogClient::officialEntryByHash(const QByteArray &sha256Hex) const {
    for (const CatalogEntry &entry : m_official.plugins) {
        if (entry.sha256 == sha256Hex)
            return entry;
    }
    return std::nullopt;
}

void CatalogClient::setFeeds(const QList<CatalogFeed> &feeds) {
    m_feeds.clear();
    QSet<QUrl> seen{m_options.url};
    for (const CatalogFeed &feed : feeds) {
        if (!isHttps(feed.url) || seen.contains(feed.url))
            continue;
        seen.insert(feed.url);
        m_feeds << feed;
    }
}

PluginVerification CatalogClient::verify(const QByteArray &sha256Hex) const {
    const std::optional<CatalogEntry> entry = officialEntryByHash(sha256Hex);
    if (entry && entry->revoked)
        return PluginVerification::Revoked; // also from the cache: a revocation never expires
    if (m_state != State::Live)
        return PluginVerification::Offline;
    return entry ? PluginVerification::Verified : PluginVerification::Unverified;
}

void CatalogClient::fetch(const QUrl &url, qsizetype maxBytes, FetchCallback done) {
    if (!isHttps(url)) {
        done({}, QStringLiteral("not an https URL"));
        return;
    }
    QNetworkRequest request(url);
    request.setTransferTimeout(m_options.timeoutMs);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setAttribute(Core::INetworkClient::MaxResponseBytesAttribute, qint64(maxBytes));

    // The network client may never answer (or a fake may not honour the transfer timeout):
    // the first of "answer" and "timer" wins, the other one is ignored.
    auto finished = std::make_shared<bool>(false);
    auto callback = std::make_shared<FetchCallback>(std::move(done));
    QTimer::singleShot(m_options.timeoutMs, this, [finished, callback] {
        if (*finished)
            return;
        *finished = true;
        (*callback)({}, QStringLiteral("timeout"));
    });
    QPointer<CatalogClient> self(this);
    m_network->get(request, [self, finished, callback, maxBytes](const Core::NetworkResponse &response) {
        if (!self || *finished)
            return;
        *finished = true;
        if (!response.isSuccess())
            (*callback)({}, response.errorString.isEmpty() ? QStringLiteral("HTTP status %1").arg(response.statusCode)
                                                           : response.errorString);
        else if (response.data.size() > maxBytes)
            (*callback)({}, QStringLiteral("too large"));
        else
            (*callback)(response.data, {});
    });
}

void CatalogClient::refresh(DoneCallback done) {
    if (done)
        m_waiting << std::move(done);
    if (m_refreshing)
        return;
    m_refreshing = true;
    emit changed();
    fetch(m_options.url, PluginCatalogData::MaxBytes, [this](const QByteArray &data, const QString &error) {
        if (!error.isEmpty()) {
            finishRefresh(false, error);
            return;
        }
        QString parseError;
        const auto parsed = PluginCatalogData::parse(data, &parseError);
        if (!parsed) {
            finishRefresh(false, parseError);
            return;
        }
        m_official = *parsed;
        const QString path = cacheFile();
        if (!path.isEmpty() && QDir().mkpath(m_options.cacheDirectory)) {
            QSaveFile file(path);
            if (file.open(QIODevice::WriteOnly) && file.write(data) == data.size())
                file.commit();
        }
        m_pendingFeedEntries.clear();
        fetchNextFeed(0);
    });
}

void CatalogClient::fetchNextFeed(int index) {
    if (index >= m_feeds.size()) {
        m_feedEntries = m_pendingFeedEntries;
        m_pendingFeedEntries.clear();
        finishRefresh(true, {});
        return;
    }
    const CatalogFeed feed = m_feeds.at(index);
    fetch(feed.url, PluginCatalogData::MaxBytes, [this, feed, index](const QByteArray &data, const QString &error) {
        // A broken feed is skipped; it must not hide the official catalog.
        if (error.isEmpty()) {
            if (auto parsed = PluginCatalogData::parse(data, nullptr)) {
                for (CatalogEntry entry : std::as_const(parsed->plugins)) {
                    entry.feed = feed.name.isEmpty() ? feed.url.host() : feed.name;
                    m_pendingFeedEntries << entry;
                }
            }
        }
        fetchNextFeed(index + 1);
    });
}

void CatalogClient::finishRefresh(bool ok, const QString &error) {
    m_refreshing = false;
    m_lastError = error;
    if (ok) {
        m_state = State::Live;
    } else {
        // Keep showing what we have, but it no longer counts as verified.
        m_state = m_official.plugins.isEmpty() && !m_official.updated.isValid() ? State::Empty : State::Cached;
        m_feedEntries.clear();
    }
    const QList<DoneCallback> waiting = std::move(m_waiting);
    m_waiting.clear();
    emit changed();
    for (const DoneCallback &callback : waiting)
        callback(ok, error);
}

void CatalogClient::download(const CatalogEntry &entry, DownloadCallback done) {
    fetch(entry.downloadUrl, PluginPackage::MaxPackageBytes, std::move(done));
}

} // namespace EasePass::Plugin
