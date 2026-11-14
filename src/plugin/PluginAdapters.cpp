#include "PluginAdapters.h"

#include <QTimeZone>

namespace EasePass::Plugin {

namespace {

const QString AboutInterface = QStringLiteral("easepass:plugin/about");
const QString ImporterInterface = QStringLiteral("easepass:plugin/importer");
const QString GeneratorInterface = QStringLiteral("easepass:plugin/password-generator");
const QString StoreFeedInterface = QStringLiteral("easepass:plugin/store-feed");
const QString DatabaseInterface = QStringLiteral("easepass:plugin/database-source");

PluginError unexpected(const QString &function) {
    return {PluginError::Kind::Host, {}, QStringLiteral("%1 returned an unexpected value").arg(function)};
}

/// Calls a function and converts its value with `convert(value, &out)` (false = wrong shape).
/// `isResult`: the function returns result<T, error> that has to be unwrapped first.
template <typename T, typename Convert>
void request(const std::shared_ptr<PluginWorker> &worker, const QString &interface, const QString &function,
             const QList<Value> &arguments, bool isResult, int timeoutMs, QObject *context,
             PluginCallback<T> callback, Convert convert) {
    worker->call(interface, function, arguments, timeoutMs, context,
                 [=](const Value &raw, const PluginError &callError) {
                     PluginError error = callError;
                     T out{};
                     if (error.ok()) {
                         const Value value = isResult ? PluginError::unwrap(raw, &error) : raw;
                         if (error.ok() && !convert(value, &out)) {
                             error = unexpected(function);
                             out = T{};
                         }
                     }
                     callback(out, error);
                 });
}

bool toText(const Value &value, QString *out) {
    if (value.kind() != Value::Kind::String)
        return false;
    *out = value.toString();
    return true;
}

bool hasStrings(const Value &record, std::initializer_list<const char *> names) {
    if (record.kind() != Value::Kind::Record)
        return false;
    for (const char *name : names) {
        if (record.field(QLatin1String(name)).kind() != Value::Kind::String)
            return false;
    }
    return true;
}

QUrl webUrl(const QString &text, bool httpsOnly) {
    const QUrl url(text, QUrl::StrictMode);
    const bool schemeOk = url.scheme() == QLatin1String("https") || (!httpsOnly && url.scheme() == QLatin1String("http"));
    return url.isValid() && schemeOk && !url.host().isEmpty() ? url : QUrl();
}

} // namespace

// --- about -----------------------------------------------------------------------

void PluginAbout::info(QObject *context, PluginCallback<PluginAboutInfo> callback) {
    request<PluginAboutInfo>(m_worker, AboutInterface, QStringLiteral("info"), {}, false, 0, context, callback,
                             [](const Value &value, PluginAboutInfo *info) {
                                 if (!hasStrings(value, {"name", "description", "author"}))
                                     return false;
                                 info->name = value.field(QStringLiteral("name")).toString();
                                 info->description = value.field(QStringLiteral("description")).toString();
                                 info->author = value.field(QStringLiteral("author")).toString();
                                 const Value &url = value.field(QStringLiteral("author-url"));
                                 if (url.isSome())
                                     info->authorUrl = webUrl(url.payload().toString(), false);
                                 return true;
                             });
}

// --- importer --------------------------------------------------------------------

bool PluginImporter::toPasswordItem(const Value &record, Core::PasswordItem *item) {
    if (!hasStrings(record, {"display-name", "user-name", "email", "website", "password", "notes"}))
        return false;
    const Value &tags = record.field(QStringLiteral("tags"));
    const Value &totp = record.field(QStringLiteral("totp"));
    if (tags.kind() != Value::Kind::List || totp.kind() != Value::Kind::Option)
        return false;

    Core::PasswordItem result;
    result.displayName = record.field(QStringLiteral("display-name")).toString();
    result.username = record.field(QStringLiteral("user-name")).toString();
    result.email = record.field(QStringLiteral("email")).toString();
    result.website = record.field(QStringLiteral("website")).toString();
    result.password = record.field(QStringLiteral("password")).toString();
    result.notes = record.field(QStringLiteral("notes")).toString();
    for (const Value &tag : tags.items()) {
        if (tag.kind() != Value::Kind::String)
            return false;
        if (!tag.toString().trimmed().isEmpty())
            result.tags << tag.toString();
    }
    if (totp.isSome()) {
        const Value &value = totp.payload();
        if (!hasStrings(value, {"secret"}) || value.field(QStringLiteral("algorithm")).kind() != Value::Kind::Enum)
            return false;
        const QString algorithm = value.field(QStringLiteral("algorithm")).toString().toUpper();
        if (algorithm != QLatin1String("SHA1") && algorithm != QLatin1String("SHA256") &&
            algorithm != QLatin1String("SHA512"))
            return false;
        result.secret = value.field(QStringLiteral("secret")).toString();
        result.digits = QString::number(value.field(QStringLiteral("digits")).toU64());
        result.interval = QString::number(value.field(QStringLiteral("interval")).toU64());
        result.algorithm = algorithm;
    }
    *item = result;
    return true;
}

void PluginImporter::sources(QObject *context, PluginCallback<QList<PluginImportSource>> callback) {
    request<QList<PluginImportSource>>(m_worker, ImporterInterface, QStringLiteral("sources"), {}, false, 0, context,
                                       callback, [](const Value &value, QList<PluginImportSource> *sources) {
                                           if (value.kind() != Value::Kind::List)
                                               return false;
                                           for (const Value &source : value.items()) {
                                               if (!hasStrings(source, {"id", "name", "description"}))
                                                   return false;
                                               *sources << PluginImportSource{
                                                   source.field(QStringLiteral("id")).toString(),
                                                   source.field(QStringLiteral("name")).toString(),
                                                   source.field(QStringLiteral("description")).toString()};
                                           }
                                           return true;
                                       });
}

void PluginImporter::passwordsAvailable(const QString &sourceId, QObject *context, PluginCallback<bool> callback) {
    request<bool>(m_worker, ImporterInterface, QStringLiteral("passwords-available"), {Value::string(sourceId)}, true,
                  0, context, callback, [](const Value &value, bool *available) {
                      if (value.kind() != Value::Kind::Bool)
                          return false;
                      *available = value.toBool();
                      return true;
                  });
}

void PluginImporter::importPasswords(const QString &sourceId, QObject *context,
                                     PluginCallback<QList<Core::PasswordItem>> callback) {
    request<QList<Core::PasswordItem>>(m_worker, ImporterInterface, QStringLiteral("import-passwords"),
                                       {Value::string(sourceId)}, true, 0, context, callback,
                                       [](const Value &value, QList<Core::PasswordItem> *items) {
                                           if (value.kind() != Value::Kind::List)
                                               return false;
                                           for (const Value &record : value.items()) {
                                               Core::PasswordItem item;
                                               if (!toPasswordItem(record, &item))
                                                   return false;
                                               *items << item;
                                           }
                                           return true;
                                       });
}

// --- password generator ------------------------------------------------------------

void PluginPasswordGenerator::name(QObject *context, PluginCallback<QString> callback) {
    request<QString>(m_worker, GeneratorInterface, QStringLiteral("name"), {}, false, 0, context, callback, toText);
}

void PluginPasswordGenerator::generate(int length, const QString &characters, bool verifyNotLeaked, QObject *context,
                                       PluginCallback<QString> callback) {
    const Value options = Value::record({{QStringLiteral("length"), Value::u32(quint32(qMax(0, length)))},
                                         {QStringLiteral("characters"), Value::string(characters)},
                                         {QStringLiteral("verify-not-leaked"), Value::boolean(verifyNotLeaked)}});
    request<QString>(m_worker, GeneratorInterface, QStringLiteral("generate"), {options}, true, 0, context, callback,
                     [](const Value &value, QString *password) {
                         // The output becomes a real password: refuse nonsense instead of storing it.
                         return toText(value, password) && !password->isEmpty() && password->size() <= MaxLength;
                     });
}

// --- store feed ----------------------------------------------------------------------

void PluginStoreFeed::feedSources(QObject *context, PluginCallback<QList<PluginFeedSource>> callback) {
    request<QList<PluginFeedSource>>(m_worker, StoreFeedInterface, QStringLiteral("feed-sources"), {}, true, 0, context,
                                     callback, [](const Value &value, QList<PluginFeedSource> *sources) {
                                         if (value.kind() != Value::Kind::List)
                                             return false;
                                         for (const Value &source : value.items()) {
                                             if (!hasStrings(source, {"name", "url"}))
                                                 return false;
                                             const QUrl url = webUrl(source.field(QStringLiteral("url")).toString(), true);
                                             if (url.isValid())
                                                 *sources << PluginFeedSource{
                                                     source.field(QStringLiteral("name")).toString(), url};
                                         }
                                         return true;
                                     });
}

// --- database source -------------------------------------------------------------------

bool PluginDatabaseSource::toInfo(const Value &record, Info *info) {
    if (!hasStrings(record, {"id", "name", "description"}) ||
        record.field(QStringLiteral("read-only")).kind() != Value::Kind::Bool ||
        record.field(QStringLiteral("availability")).kind() != Value::Kind::Enum ||
        record.field(QStringLiteral("last-modified")).kind() != Value::Kind::Option)
        return false;
    info->id = record.field(QStringLiteral("id")).toString();
    info->name = record.field(QStringLiteral("name")).toString();
    info->description = record.field(QStringLiteral("description")).toString();
    info->readOnly = record.field(QStringLiteral("read-only")).toBool();
    const QString availability = record.field(QStringLiteral("availability")).toString();
    info->availability = availability == QLatin1String("available")              ? Availability::Available
                         : availability == QLatin1String("locked-by-other-user") ? Availability::LockedByOtherUser
                         : availability == QLatin1String("unavailable")          ? Availability::Unavailable
                                                                                 : Availability::UnknownState;
    const Value &modified = record.field(QStringLiteral("last-modified"));
    info->lastModified = modified.isSome()
                             ? QDateTime::fromSecsSinceEpoch(qint64(modified.payload().toU64()), QTimeZone::UTC)
                             : QDateTime();
    return true;
}

PluginDatabaseSource::PluginDatabaseSource(std::shared_ptr<PluginWorker> worker, Info info, QObject *parent)
    : Core::IDatabaseSource(parent), m_worker(std::move(worker)), m_info(std::move(info)) {}

QString PluginDatabaseSource::databaseId() const {
    QMutexLocker lock(&m_mutex);
    return m_info.id;
}
QString PluginDatabaseSource::databaseName() const {
    QMutexLocker lock(&m_mutex);
    return m_info.name;
}
QString PluginDatabaseSource::sourceDescription() const {
    QMutexLocker lock(&m_mutex);
    return m_info.description;
}
bool PluginDatabaseSource::isReadOnly() const {
    QMutexLocker lock(&m_mutex);
    return m_info.readOnly;
}
Core::IDatabaseSource::Availability PluginDatabaseSource::availability() const {
    QMutexLocker lock(&m_mutex);
    return m_info.availability;
}
QDateTime PluginDatabaseSource::lastModified() const {
    QMutexLocker lock(&m_mutex);
    return m_info.lastModified;
}
PluginError PluginDatabaseSource::lastError() const {
    QMutexLocker lock(&m_mutex);
    return m_lastError;
}
void PluginDatabaseSource::setLastError(const PluginError &error) {
    QMutexLocker lock(&m_mutex);
    m_lastError = error;
}

void PluginDatabaseSource::apply(const Info &info) {
    bool differs = false;
    {
        QMutexLocker lock(&m_mutex);
        if (info.id != m_info.id)
            return; // a plugin must not swap the database behind an existing source
        differs = info.name != m_info.name || info.description != m_info.description ||
                  info.readOnly != m_info.readOnly || info.availability != m_info.availability ||
                  info.lastModified != m_info.lastModified;
        m_info = info;
    }
    if (differs)
        emit changed();
}

std::optional<QByteArray> PluginDatabaseSource::getBytes() {
    PluginError error;
    const Value raw = m_worker->callBlocking(DatabaseInterface, QStringLiteral("read-database"),
                                             {Value::string(databaseId())}, TransferTimeoutMs, &error);
    const Value value = error.ok() ? PluginError::unwrap(raw, &error) : Value();
    if (error.ok() && value.kind() != Value::Kind::Bytes && value.kind() != Value::Kind::List)
        error = unexpected(QStringLiteral("read-database"));
    setLastError(error);
    if (!error.ok())
        return std::nullopt;
    return value.toBytes();
}

bool PluginDatabaseSource::saveBytes(const QByteArray &bytes) {
    PluginError error;
    if (isReadOnly()) {
        error = {PluginError::Kind::Host, {}, QStringLiteral("the database is read-only")};
    } else if (bytes.size() > PluginLimits::MaxByteListSize) {
        error = {PluginError::Kind::Host, {}, QStringLiteral("the database is too large for a plugin (8 MiB)")};
    } else {
        const Value raw = m_worker->callBlocking(DatabaseInterface, QStringLiteral("write-database"),
                                                 {Value::string(databaseId()), Value::bytes(bytes)},
                                                 TransferTimeoutMs, &error);
        if (error.ok())
            PluginError::unwrap(raw, &error);
    }
    setLastError(error);
    if (error.ok())
        refreshBlocking(); // new modification time
    return error.ok();
}

void PluginDatabaseSource::login() {
    PluginError error;
    const Value raw = m_worker->callBlocking(DatabaseInterface, QStringLiteral("login"),
                                             {Value::string(databaseId())}, 0, &error);
    if (error.ok())
        PluginError::unwrap(raw, &error);
    setLastError(error);
    refreshBlocking();
}

void PluginDatabaseSource::logout() {
    PluginError error;
    const Value raw = m_worker->callBlocking(DatabaseInterface, QStringLiteral("logout"),
                                             {Value::string(databaseId())}, 0, &error);
    if (error.ok())
        PluginError::unwrap(raw, &error);
    setLastError(error);
    refreshBlocking();
}

void PluginDatabaseSource::refreshBlocking() {
    PluginError error;
    const Value raw = m_worker->callBlocking(DatabaseInterface, QStringLiteral("get-database"),
                                             {Value::string(databaseId())}, 0, &error);
    const Value value = error.ok() ? PluginError::unwrap(raw, &error) : Value();
    Info info;
    if (error.ok() && toInfo(value, &info))
        apply(info);
}

void PluginDatabaseSource::refresh() {
    m_worker->call(DatabaseInterface, QStringLiteral("get-database"), {Value::string(databaseId())}, 0, this,
                   [this](const Value &raw, const PluginError &callError) {
                       PluginError error = callError;
                       const Value value = error.ok() ? PluginError::unwrap(raw, &error) : Value();
                       Info info;
                       if (error.ok() && toInfo(value, &info))
                           apply(info);
                   });
}

void PluginDatabaseProvider::providerName(QObject *context, PluginCallback<QString> callback) {
    request<QString>(m_worker, DatabaseInterface, QStringLiteral("provider-name"), {}, false, 0, context, callback,
                     toText);
}

void PluginDatabaseProvider::configSchema(QObject *context, PluginCallback<QString> callback) {
    request<QString>(m_worker, DatabaseInterface, QStringLiteral("config-schema"), {}, false, 0, context, callback,
                     toText);
}

void PluginDatabaseProvider::config(QObject *context, PluginCallback<QString> callback) {
    request<QString>(m_worker, DatabaseInterface, QStringLiteral("get-config"), {}, true, 0, context, callback, toText);
}

void PluginDatabaseProvider::setConfig(const QString &configJson, QObject *context, PluginDoneCallback callback) {
    m_worker->call(DatabaseInterface, QStringLiteral("set-config"), {Value::string(configJson)}, 0, context,
                   [callback](const Value &raw, const PluginError &callError) {
                       PluginError error = callError;
                       if (error.ok())
                           PluginError::unwrap(raw, &error);
                       callback(error);
                   });
}

void PluginDatabaseProvider::listDatabases(QObject *context,
                                           PluginCallback<QList<std::shared_ptr<PluginDatabaseSource>>> callback) {
    const std::shared_ptr<PluginWorker> worker = m_worker;
    request<QList<std::shared_ptr<PluginDatabaseSource>>>(
        m_worker, DatabaseInterface, QStringLiteral("list-databases"), {}, true, 0, context, callback,
        [worker](const Value &value, QList<std::shared_ptr<PluginDatabaseSource>> *sources) {
            if (value.kind() != Value::Kind::List)
                return false;
            QStringList ids;
            for (const Value &record : value.items()) {
                PluginDatabaseSource::Info info;
                if (!PluginDatabaseSource::toInfo(record, &info) || info.id.isEmpty() || ids.contains(info.id))
                    return false;
                ids << info.id;
                *sources << std::make_shared<PluginDatabaseSource>(worker, info);
            }
            return true;
        });
}

} // namespace EasePass::Plugin
