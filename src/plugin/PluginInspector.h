#pragma once

#include <QByteArray>
#include <QStringList>

namespace EasePass::Plugin {

/// What the host computed from a plugin component. Nothing here comes from the manifest.
struct PluginInspection {
    QStringList imports;     ///< as written in the component, e.g. "easepass:plugin/log@1.0.0"
    QStringList exports;
    QStringList permissions; ///< permission ids, in the order of the permission table
    QStringList types;       ///< plugin type ids, in the order of the type table
    QStringList rejected;    ///< reasons why the component must not be installed

    bool accepted() const { return rejected.isEmpty(); }
};

/**
 * Computes plugin types and permissions from the imports and exports of a WebAssembly
 * component WITHOUT instantiating it (steps.txt 4.2, spec: docs/permissions.md in
 * easepassqt-plugin-spec; same rules as its tools/check_component.py).
 */
class PluginInspector {
public:
    static constexpr int ApiVersion = 1;

    static PluginInspection inspect(const QByteArray &componentBytes);

    /// Import interface (without version) that grants `permission`, empty if unknown.
    static QString importOfPermission(const QString &permission);
};

} // namespace EasePass::Plugin
