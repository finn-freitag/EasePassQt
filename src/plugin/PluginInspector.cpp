#include "PluginInspector.h"

#include "PluginEngine.h"

#include <QRegularExpression>
#include <algorithm>
#include <array>
#include <wasmtime.h>

namespace EasePass::Plugin {

namespace {

struct Mapping {
    const char *interface;
    const char *id;
};

// Mirror of spec/permissions.json (api_version 1) in easepassqt-plugin-spec.
constexpr std::array NeutralImports = {"easepass:plugin/types", "easepass:plugin/log", "wasi:random/random",
                                       "wasi:clocks/wall-clock"};
constexpr std::array<Mapping, 4> Permissions = {{{"easepass:plugin/storage", "storage"},
                                                 {"easepass:plugin/file-picker", "pick-file"},
                                                 {"easepass:plugin/network", "network"},
                                                 {"easepass:plugin/leak-check", "leak-check"}}};
constexpr std::array<Mapping, 6> Types = {{{"easepass:plugin/about", "about"},
                                           {"easepass:plugin/initializer", "initializer"},
                                           {"easepass:plugin/importer", "importer"},
                                           {"easepass:plugin/database-source", "database-source"},
                                           {"easepass:plugin/password-generator", "password-generator"},
                                           {"easepass:plugin/store-feed", "store-feed"}}};

QString errorText(wasmtime_error_t *error) {
    wasm_name_t message;
    wasmtime_error_message(error, &message);
    const QString text = QString::fromUtf8(message.data, qsizetype(message.size));
    wasm_byte_vec_delete(&message);
    wasmtime_error_delete(error);
    return text;
}

/// Returns the interface name without version, or an empty string and a rejection reason.
QString baseName(const QString &item, bool isInstance, const char *kind, QStringList &rejected) {
    static const QRegularExpression pattern(
        QStringLiteral("^([a-z0-9-]+:[a-z0-9-]+/[a-z0-9-]+)@(\\d+)\\.(\\d+)\\.(\\d+)$"));
    const QRegularExpressionMatch match = pattern.match(item);
    if (!isInstance || !match.hasMatch()) {
        rejected << QStringLiteral("%1 '%2' is not a versioned interface").arg(QLatin1String(kind), item);
        return {};
    }
    const QString name = match.captured(1);
    const int major = match.captured(2).toInt();
    const int minor = match.captured(3).toInt();
    const bool compatible =
        name.startsWith(QLatin1String("wasi:")) ? (major == 0 && minor == 2) : major == PluginInspector::ApiVersion;
    if (!compatible) {
        rejected << QStringLiteral("%1 '%2' has an unsupported version").arg(QLatin1String(kind), item);
        return {};
    }
    return name;
}

template <typename Table> QStringList inTableOrder(const Table &table, const QStringList &found) {
    QStringList ordered;
    for (const Mapping &entry : table) {
        if (found.contains(QLatin1String(entry.id)))
            ordered << QLatin1String(entry.id);
    }
    return ordered;
}

} // namespace

QString PluginInspector::importOfPermission(const QString &permission) {
    for (const Mapping &entry : Permissions) {
        if (permission == QLatin1String(entry.id))
            return QLatin1String(entry.interface);
    }
    return {};
}

PluginInspection PluginInspector::inspect(const QByteArray &componentBytes) {
    PluginInspection result;
    wasm_engine_t *engine = PluginEngine::instance().engine();

    // Compiles the component but does not instantiate it: no plugin code runs here.
    wasmtime_component_t *component = nullptr;
    if (wasmtime_error_t *error =
            wasmtime_component_new(engine, reinterpret_cast<const uint8_t *>(componentBytes.constData()),
                                   size_t(componentBytes.size()), &component)) {
        result.rejected << QStringLiteral("not a valid WebAssembly component: %1").arg(errorText(error));
        return result;
    }
    wasmtime_component_type_t *type = wasmtime_component_type(component);

    QStringList permissions, types;
    const size_t importCount = wasmtime_component_type_import_count(type, engine);
    for (size_t i = 0; i < importCount; ++i) {
        const char *name = nullptr;
        size_t nameLength = 0;
        wasmtime_component_item_t item;
        if (!wasmtime_component_type_import_nth(type, engine, i, &name, &nameLength, &item))
            continue;
        const QString import = QString::fromUtf8(name, qsizetype(nameLength));
        const bool isInstance = item.kind == WASMTIME_COMPONENT_ITEM_COMPONENT_INSTANCE;
        wasmtime_component_item_delete(&item);
        result.imports << import;

        const QString base = baseName(import, isInstance, "import", result.rejected);
        if (base.isEmpty())
            continue;
        const auto permission = std::find_if(Permissions.begin(), Permissions.end(),
                                             [&](const Mapping &m) { return base == QLatin1String(m.interface); });
        if (permission != Permissions.end())
            permissions << QLatin1String(permission->id);
        else if (std::none_of(NeutralImports.begin(), NeutralImports.end(),
                              [&](const char *neutral) { return base == QLatin1String(neutral); }))
            result.rejected << QStringLiteral("import '%1' is not on the allow-list").arg(import);
    }

    const size_t exportCount = wasmtime_component_type_export_count(type, engine);
    for (size_t i = 0; i < exportCount; ++i) {
        const char *name = nullptr;
        size_t nameLength = 0;
        wasmtime_component_item_t item;
        if (!wasmtime_component_type_export_nth(type, engine, i, &name, &nameLength, &item))
            continue;
        const QString exported = QString::fromUtf8(name, qsizetype(nameLength));
        const bool isInstance = item.kind == WASMTIME_COMPONENT_ITEM_COMPONENT_INSTANCE;
        wasmtime_component_item_delete(&item);
        result.exports << exported;

        const QString base = baseName(exported, isInstance, "export", result.rejected);
        if (base.isEmpty())
            continue;
        const auto pluginType = std::find_if(Types.begin(), Types.end(),
                                             [&](const Mapping &m) { return base == QLatin1String(m.interface); });
        if (pluginType != Types.end())
            types << QLatin1String(pluginType->id);
        else
            result.rejected << QStringLiteral("export '%1' is not a known plugin interface").arg(exported);
    }
    if (types.isEmpty() && result.rejected.isEmpty())
        result.rejected << QStringLiteral("the component exports no plugin interface");

    result.permissions = inTableOrder(Permissions, permissions);
    result.types = inTableOrder(Types, types);

    wasmtime_component_type_delete(type);
    wasmtime_component_delete(component);
    return result;
}

} // namespace EasePass::Plugin
