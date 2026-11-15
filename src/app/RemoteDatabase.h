#pragma once

#include "IDatabaseSource.h"

#include <QList>
#include <QString>
#include <memory>

namespace EasePass::App {

/**
 * A database that does not live in a local file: it comes from a plugin of type
 * database-source (PluginsViewModel::remoteDatabases). The pages address databases by a
 * string; for local databases that is the file path, for these it is `key`.
 */
struct RemoteDatabase {
    QString key;         ///< "plugin://<plugin id>/<database id>"
    QString name;        ///< for lists: "<database name> (<plugin name>)"
    QString description; ///< where it is stored, as told by the plugin
    std::shared_ptr<Core::IDatabaseSource> source;

    static inline const QString KeyPrefix = QStringLiteral("plugin://");
    static bool isKey(const QString &pathOrKey) { return pathOrKey.startsWith(KeyPrefix); }
    static const RemoteDatabase *find(const QList<RemoteDatabase> &list, const QString &key) {
        for (const RemoteDatabase &database : list) {
            if (database.key == key)
                return &database;
        }
        return nullptr;
    }
    /// The entry whose source is @p source (the open database), or null.
    static const RemoteDatabase *find(const QList<RemoteDatabase> &list, const Core::IDatabaseSource *source) {
        for (const RemoteDatabase &database : list) {
            if (source && database.source.get() == source)
                return &database;
        }
        return nullptr;
    }
};

} // namespace EasePass::App
