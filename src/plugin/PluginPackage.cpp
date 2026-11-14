#include "PluginPackage.h"

#include "PluginNetwork.h"

#include <QCryptographicHash>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSysInfo>
#include <QtEndian>
#include <zlib.h>

namespace EasePass::Plugin {

namespace {

// --- minimal ZIP reader (stored + deflate, no ZIP64, no encryption) -----------

constexpr quint32 EndOfCentralDirectory = 0x06054b50;
constexpr quint32 CentralFileHeader = 0x02014b50;
constexpr quint32 LocalFileHeader = 0x04034b50;
constexpr int MaxEntries = 64;

struct ZipEntry {
    quint16 flags = 0;
    quint16 method = 0;
    quint32 crc = 0;
    quint32 compressedSize = 0;
    quint32 size = 0;
    quint32 localOffset = 0;
};

quint16 u16(const QByteArray &data, qsizetype offset) {
    return qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(data.constData()) + offset);
}
quint32 u32(const QByteArray &data, qsizetype offset) {
    return qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(data.constData()) + offset);
}

bool fail(QString *error, const QString &message) {
    if (error)
        *error = message;
    return false;
}

/// Reads the central directory. Duplicate names are an error.
bool readDirectory(const QByteArray &zip, QHash<QByteArray, ZipEntry> *entries, QString *error) {
    const QString invalid = QStringLiteral("the package is not a valid ZIP archive");
    if (zip.size() < 22)
        return fail(error, invalid);
    qsizetype end = -1;
    for (qsizetype i = zip.size() - 22; i >= qMax<qsizetype>(0, zip.size() - 22 - 65535); --i) {
        if (u32(zip, i) == EndOfCentralDirectory && i + 22 + u16(zip, i + 20) == zip.size()) {
            end = i;
            break;
        }
    }
    if (end < 0)
        return fail(error, invalid);
    const quint16 disk = u16(zip, end + 4), directoryDisk = u16(zip, end + 6);
    const quint16 entriesOnDisk = u16(zip, end + 8), entryCount = u16(zip, end + 10);
    const quint32 directorySize = u32(zip, end + 12), directoryOffset = u32(zip, end + 16);
    if (disk != 0 || directoryDisk != 0 || entriesOnDisk != entryCount || entryCount == 0xFFFF ||
        directorySize == 0xFFFFFFFF || directoryOffset == 0xFFFFFFFF)
        return fail(error, QStringLiteral("multi-part and ZIP64 archives are not supported"));
    if (entryCount > MaxEntries)
        return fail(error, QStringLiteral("the package contains too many files"));
    if (qint64(directoryOffset) + directorySize > end)
        return fail(error, invalid);

    qsizetype position = directoryOffset;
    for (int i = 0; i < entryCount; ++i) {
        if (position + 46 > end || u32(zip, position) != CentralFileHeader)
            return fail(error, invalid);
        ZipEntry entry;
        entry.flags = u16(zip, position + 8);
        entry.method = u16(zip, position + 10);
        entry.crc = u32(zip, position + 16);
        entry.compressedSize = u32(zip, position + 20);
        entry.size = u32(zip, position + 24);
        entry.localOffset = u32(zip, position + 42);
        const qsizetype nameLength = u16(zip, position + 28);
        const qsizetype extraLength = u16(zip, position + 30), commentLength = u16(zip, position + 32);
        if (position + 46 + nameLength + extraLength + commentLength > end)
            return fail(error, invalid);
        const QByteArray name = zip.mid(position + 46, nameLength);
        if (entries->contains(name))
            return fail(error, QStringLiteral("the package contains '%1' twice").arg(QString::fromUtf8(name)));
        entries->insert(name, entry);
        position += 46 + nameLength + extraLength + commentLength;
    }
    return true;
}

bool readEntry(const QByteArray &zip, const QByteArray &name, const ZipEntry &entry, qsizetype maxSize,
               QByteArray *out, QString *error) {
    const QString label = QString::fromUtf8(name);
    const QString invalid = QStringLiteral("'%1' in the package is damaged").arg(label);
    if (entry.flags & 0x0001)
        return fail(error, QStringLiteral("'%1' is encrypted").arg(label));
    if (entry.method != 0 && entry.method != 8)
        return fail(error, QStringLiteral("'%1' uses an unsupported compression method").arg(label));
    if (entry.size == 0xFFFFFFFF || entry.compressedSize == 0xFFFFFFFF || entry.localOffset == 0xFFFFFFFF)
        return fail(error, QStringLiteral("ZIP64 archives are not supported"));
    if (qsizetype(entry.size) > maxSize)
        return fail(error, QStringLiteral("'%1' is too large").arg(label));

    const qsizetype header = entry.localOffset;
    if (header + 30 > zip.size() || u32(zip, header) != LocalFileHeader)
        return fail(error, invalid);
    const qsizetype nameLength = u16(zip, header + 26), extraLength = u16(zip, header + 28);
    const qsizetype dataStart = header + 30 + nameLength + extraLength;
    if (zip.mid(header + 30, nameLength) != name || dataStart + qsizetype(entry.compressedSize) > zip.size())
        return fail(error, invalid);
    const QByteArray compressed = zip.mid(dataStart, entry.compressedSize);

    QByteArray data;
    if (entry.method == 0) {
        if (entry.compressedSize != entry.size)
            return fail(error, invalid);
        data = compressed;
    } else {
        data.resize(entry.size);
        z_stream stream{};
        if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) // raw deflate, as stored in ZIP files
            return fail(error, invalid);
        stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(compressed.constData()));
        stream.avail_in = uInt(compressed.size());
        stream.next_out = reinterpret_cast<Bytef *>(data.data());
        stream.avail_out = uInt(data.size());
        const int result = inflate(&stream, Z_FINISH);
        const bool complete = result == Z_STREAM_END && stream.avail_out == 0 && stream.avail_in == 0;
        inflateEnd(&stream);
        if (!complete) // too short, too long (a "zip bomb" lies about its size) or corrupt
            return fail(error, invalid);
    }
    const uLong crc = crc32(crc32(0L, Z_NULL, 0), reinterpret_cast<const Bytef *>(data.constData()), uInt(data.size()));
    if (quint32(crc) != entry.crc)
        return fail(error, invalid);
    *out = data;
    return true;
}

// --- manifest ------------------------------------------------------------------

const QRegularExpression &versionPattern(bool allowPreRelease) {
    static const QRegularExpression release(QStringLiteral("^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)$"));
    static const QRegularExpression any(
        QStringLiteral("^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)(-[0-9A-Za-z.-]+)?$"));
    return allowPreRelease ? any : release;
}

/// Reads an optional string array whose items must be in `allowed` (empty = any string).
bool readStringArray(const QJsonObject &object, const QString &key, const QStringList &allowed, QStringList *out,
                     bool *present, QString *error) {
    *present = object.contains(key);
    if (!*present)
        return true;
    if (!object.value(key).isArray())
        return fail(error, QStringLiteral("manifest: '%1' must be a list").arg(key));
    for (const QJsonValue &item : object.value(key).toArray()) {
        const QString text = item.toString();
        if (!item.isString() || (!allowed.isEmpty() && !allowed.contains(text)) || out->contains(text))
            return fail(error, QStringLiteral("manifest: invalid entry in '%1'").arg(key));
        *out << text;
    }
    return true;
}

} // namespace

bool PluginManifest::isValidId(const QString &id) {
    static const QRegularExpression pattern(
        QStringLiteral("^[a-z][a-z0-9]*(-[a-z0-9]+)*(\\.[a-z][a-z0-9]*(-[a-z0-9]+)*)+$"));
    return id.size() <= 128 && pattern.match(id).hasMatch();
}

QString PluginManifest::currentOs() {
#if defined(Q_OS_WIN)
    return QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("macos");
#elif defined(Q_OS_LINUX)
    return QStringLiteral("linux");
#else
    return QString();
#endif
}

bool PluginManifest::supportsCurrentOs() const { return os.isEmpty() || os.contains(currentOs()); }

int PluginManifest::compareVersions(const QString &a, const QString &b) {
    const QStringList left = a.section(QLatin1Char('-'), 0, 0).split(QLatin1Char('.'));
    const QStringList right = b.section(QLatin1Char('-'), 0, 0).split(QLatin1Char('.'));
    for (int i = 0; i < 3; ++i) {
        const int l = left.value(i).toInt(), r = right.value(i).toInt();
        if (l != r)
            return l < r ? -1 : 1;
    }
    return 0;
}

std::optional<PluginManifest> PluginManifest::parse(const QByteArray &json, QString *error) {
    const auto invalid = [&](const QString &message) -> std::optional<PluginManifest> {
        if (error)
            *error = QStringLiteral("manifest: ") + message;
        return std::nullopt;
    };
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (!document.isObject())
        return invalid(QStringLiteral("not a JSON object"));
    const QJsonObject object = document.object();

    // Required/optional strings with their maximum length; wrong types are errors.
    PluginManifest manifest;
    struct StringField {
        const char *key;
        QString *target;
        int maxLength;
        bool required;
    };
    const StringField strings[] = {{"id", &manifest.id, 128, true},
                                   {"name", &manifest.name, 80, true},
                                   {"version", &manifest.version, 64, true},
                                   {"description", &manifest.description, 2000, true},
                                   {"author", &manifest.author, 120, false},
                                   {"homepage", &manifest.homepage, 2048, false},
                                   {"license", &manifest.license, 120, false},
                                   {"min_app_version", &manifest.minAppVersion, 64, false}};
    for (const StringField &field : strings) {
        const QString key = QLatin1String(field.key);
        if (!object.contains(key)) {
            if (field.required)
                return invalid(QStringLiteral("'%1' is missing").arg(key));
            continue;
        }
        const QJsonValue value = object.value(key);
        if (!value.isString() || value.toString().size() > field.maxLength)
            return invalid(QStringLiteral("'%1' is invalid").arg(key));
        *field.target = value.toString();
    }
    if (!isValidId(manifest.id))
        return invalid(QStringLiteral("'id' must be a lower-case reverse-DNS name"));
    if (manifest.name.isEmpty())
        return invalid(QStringLiteral("'name' is empty"));
    if (!versionPattern(true).match(manifest.version).hasMatch())
        return invalid(QStringLiteral("'version' must be major.minor.patch"));
    if (!manifest.minAppVersion.isEmpty() && !versionPattern(false).match(manifest.minAppVersion).hasMatch())
        return invalid(QStringLiteral("'min_app_version' must be major.minor.patch"));
    if (!manifest.homepage.isEmpty() && !manifest.homepage.startsWith(QLatin1String("http://")) &&
        !manifest.homepage.startsWith(QLatin1String("https://")))
        return invalid(QStringLiteral("'homepage' must be an http(s) URL"));

    const QJsonValue apiVersion = object.value(QStringLiteral("api_version"));
    if (!apiVersion.isDouble() || apiVersion.toDouble() != apiVersion.toInt() || apiVersion.toInt() < 1)
        return invalid(QStringLiteral("'api_version' must be a positive integer"));
    manifest.apiVersion = apiVersion.toInt();

    QString arrayError;
    bool present = false;
    if (!readStringArray(object, QStringLiteral("os"),
                         {QStringLiteral("windows"), QStringLiteral("linux"), QStringLiteral("macos")}, &manifest.os,
                         &present, &arrayError))
        return invalid(arrayError.mid(10));
    if (!readStringArray(object, QStringLiteral("network_hosts"), {}, &manifest.networkHosts, &present, &arrayError))
        return invalid(arrayError.mid(10));
    if (manifest.networkHosts.size() > 64)
        return invalid(QStringLiteral("too many entries in 'network_hosts'"));
    for (const QString &host : std::as_const(manifest.networkHosts)) {
        if (!NetworkAllowList::isValidEntry(host))
            return invalid(QStringLiteral("'%1' in 'network_hosts' is not host:port").arg(host));
    }
    QStringList permissions;
    if (!readStringArray(object, QStringLiteral("permissions"),
                         {QStringLiteral("storage"), QStringLiteral("pick-file"), QStringLiteral("network"),
                          QStringLiteral("leak-check")},
                         &permissions, &present, &arrayError))
        return invalid(arrayError.mid(10));
    if (present)
        manifest.permissions = permissions;
    return manifest;
}

QByteArray PluginPackage::hash(const QByteArray &packageBytes) {
    return QCryptographicHash::hash(packageBytes, QCryptographicHash::Sha256).toHex();
}

std::optional<PluginPackage> PluginPackage::parse(const QByteArray &packageBytes, QString *error) {
    QString ignored;
    if (!error)
        error = &ignored;
    if (packageBytes.size() > MaxPackageBytes) {
        *error = QStringLiteral("the package is larger than 16 MiB");
        return std::nullopt;
    }
    PluginPackage package;
    package.sha256 = hash(packageBytes); // before any parsing

    QHash<QByteArray, ZipEntry> entries;
    if (!readDirectory(packageBytes, &entries, error))
        return std::nullopt;
    for (const char *required : {"manifest.json", "plugin.wasm"}) {
        if (!entries.contains(required)) {
            *error = QStringLiteral("the package has no %1").arg(QLatin1String(required));
            return std::nullopt;
        }
    }
    QByteArray manifestJson;
    if (!readEntry(packageBytes, "manifest.json", entries.value("manifest.json"), MaxManifestBytes, &manifestJson,
                   error) ||
        !readEntry(packageBytes, "plugin.wasm", entries.value("plugin.wasm"), MaxWasmBytes, &package.wasm, error))
        return std::nullopt;
    if (entries.contains("icon.png") &&
        !readEntry(packageBytes, "icon.png", entries.value("icon.png"), MaxIconBytes, &package.icon, error))
        return std::nullopt;

    const std::optional<PluginManifest> manifest = PluginManifest::parse(manifestJson, error);
    if (!manifest)
        return std::nullopt;
    package.manifest = *manifest;
    return package;
}

} // namespace EasePass::Plugin
