#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QString>
#include <optional>

namespace EasePass::Core {

/**
 * @brief Where an encrypted database lives (local file, later: plugin-provided remote storage).
 *
 * C++ mirror of the C# EasePassExtensibility.IDatabaseSource (C#: Models/NativeDatabaseSource.cs,
 * Models/DatabaseSourceErrorHandlingWrapper.cs). A source only ever sees ENCRYPTED bytes
 * (epdb v1.4 blob incl. version tag); encryption is done by DatabaseService/DatabaseFile.
 *
 * Threading contract (used by DatabaseService):
 *  - databaseName(), sourceDescription(), isReadOnly(), availability(), lastModified() are
 *    called on the thread that owns the object (normally the UI thread) and must be cheap.
 *  - getBytes(), saveBytes(), login(), logout() are called on DatabaseService's worker thread.
 *    They may block. The service never calls them concurrently for one source.
 *  - changed() may be emitted from any thread (connect with an auto/queued connection).
 *    Emit it whenever name, availability, read-only state or lastModified change.
 *  - Implementations must not throw; report failures through the return values.
 */
class IDatabaseSource : public QObject {
    Q_OBJECT
public:
    enum class Availability { Available, LockedByOtherUser, UnknownState, Unavailable };
    Q_ENUM(Availability)

    using QObject::QObject;
    ~IDatabaseSource() override = default;

    /// Display name, e.g. the file name without extension.
    virtual QString databaseName() const = 0;
    /// Longer description, e.g. the full path or remote location.
    virtual QString sourceDescription() const = 0;
    /// true => DatabaseService rejects every mutation and never calls saveBytes().
    virtual bool isReadOnly() const = 0;
    virtual Availability availability() const = 0;
    /// Last modification time of the stored database; invalid QDateTime if unknown.
    virtual QDateTime lastModified() const = 0;

    /// Returns the stored (encrypted) database bytes, or std::nullopt if they cannot be read.
    virtual std::optional<QByteArray> getBytes() = 0;
    /// Replaces the stored database with @p bytes. Must not leave a half-written database behind.
    virtual bool saveBytes(const QByteArray &bytes) = 0;

    /// Called after the database was unlocked successfully (start a session, acquire a lock, ...).
    virtual void login() {}
    /// Called after the database was closed/locked and every pending save has finished.
    virtual void logout() {}

signals:
    void changed();
};

} // namespace EasePass::Core
