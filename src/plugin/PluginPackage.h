#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <optional>

namespace EasePass::Plugin {

/**
 * manifest.json of a plugin package (spec: schemas/manifest.schema.json). Informational:
 * types and permissions are computed from the component, never taken from here. The only
 * value that affects what a plugin can do is `networkHosts`, which narrows the `network`
 * permission and is shown to the user.
 */
struct PluginManifest {
    QString id;
    QString name;
    QString version;
    int apiVersion = 0;
    QString description;
    QString author;
    QString homepage;
    QString license;
    QStringList os; ///< empty = all
    QString minAppVersion;
    QStringList networkHosts;
    std::optional<QStringList> permissions; ///< if present it must equal the computed set

    /// Validates like the JSON schema; unknown properties are ignored.
    static std::optional<PluginManifest> parse(const QByteArray &json, QString *error);

    static bool isValidId(const QString &id);
    /// "windows", "linux" or "macos" (empty on other systems).
    static QString currentOs();
    bool supportsCurrentOs() const;
    /// <0, 0, >0 like strcmp for "major.minor.patch" versions (pre-release parts are ignored).
    static int compareVersions(const QString &a, const QString &b);
};

/**
 * A parsed .epplugin file: ZIP with manifest.json, plugin.wasm and optionally icon.png
 * (spec: docs/package-format.md). Only these three names are read; nothing is ever
 * extracted to a path taken from the archive.
 */
struct PluginPackage {
    static constexpr qsizetype MaxPackageBytes = 16 * 1024 * 1024;
    static constexpr qsizetype MaxManifestBytes = 64 * 1024;
    static constexpr qsizetype MaxWasmBytes = 16 * 1024 * 1024;
    static constexpr qsizetype MaxIconBytes = 1024 * 1024;

    QByteArray sha256; ///< lower-case hex of the package bytes
    PluginManifest manifest;
    QByteArray wasm;
    QByteArray icon; ///< empty if the package has none

    /// Lower-case hex SHA-256. Computed over the raw bytes BEFORE anything is parsed.
    static QByteArray hash(const QByteArray &packageBytes);

    static std::optional<PluginPackage> parse(const QByteArray &packageBytes, QString *error);
};

} // namespace EasePass::Plugin
