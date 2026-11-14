#pragma once

#include "PluginInstance.h"
#include "PluginNetwork.h"
#include "PluginStorage.h"

#include <QString>
#include <QStringList>
#include <functional>
#include <memory>
#include <optional>

namespace EasePass::Core {
class IClock;
}

namespace EasePass::Plugin {

struct PickedFile {
    QString name; ///< file name without directory
    QByteArray data;
};

struct LeakCheckOutcome {
    QString status = QStringLiteral("unknown"); ///< "not-leaked", "leaked" or "unknown"
    quint64 count = 0;
};

/**
 * What the application provides to ONE plugin. Every callback runs on the plugin's worker
 * thread and may block; callbacks that need the UI thread have to hop there themselves.
 */
struct HostServices {
    QString pluginId;
    /// Receives sanitized log lines. Level is "debug", "info", "warning" or "error".
    std::function<void(const QString &level, const QString &message)> log;
    std::shared_ptr<PluginStorage> storage;
    /// Shows the host's file dialog. nullopt = the user cancelled.
    std::function<std::optional<PickedFile>(const QString &title, const QStringList &extensions)> pickFile;
    std::shared_ptr<PluginNetwork> network;
    std::function<LeakCheckOutcome(const QString &password)> leakCheck;
    const Core::IClock *clock = nullptr; ///< default: system clock
};

/**
 * Builds the host side of easepass:plugin@1: the neutral interfaces (log, wasi random and
 * wall-clock) always, and the interface of a permission ONLY if that permission id is in
 * `approvedPermissions`. A plugin importing anything else cannot be instantiated.
 */
QList<HostInterface> buildHostInterfaces(const HostServices &services, const QStringList &approvedPermissions);

/// Log text as shown to the user: control characters removed, at most 4096 bytes.
QString sanitizeLogMessage(const QString &message);

} // namespace EasePass::Plugin
