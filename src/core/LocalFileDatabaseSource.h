#pragma once

#include "IDatabaseSource.h"

#include <QIODevice>
#include <atomic>

namespace EasePass::Core {

/**
 * @brief IDatabaseSource for a database file on the local file system.
 *
 * C#: Models/NativeDatabaseSource.cs. Differences: saving is atomic and keeps a backup.
 *
 * saveBytes():
 *  1. writes the bytes into a temporary file next to the target (QSaveFile),
 *  2. flushes and fsyncs it,
 *  3. copies the current file to "<path>.bak" (best effort; a failed backup is logged but does
 *     not prevent the save),
 *  4. renames the temporary file over the target (atomic on the same file system) and fsyncs
 *     the directory (POSIX).
 * If any step before the rename fails, the old file is left untouched and no temporary file
 * remains.
 */
class LocalFileDatabaseSource : public IDatabaseSource {
    Q_OBJECT
public:
    explicit LocalFileDatabaseSource(const QString &path, bool readOnly = false, QObject *parent = nullptr);

    QString path() const { return m_path; }
    static QString backupPathFor(const QString &path);

    /// Forces read-only mode (e.g. temporary databases). Emits changed().
    void setReadOnly(bool readOnly);

    QString databaseName() const override;
    QString sourceDescription() const override;
    /// true if forced read-only or the existing file is not writable.
    bool isReadOnly() const override;
    /// UnknownState (empty path), Unavailable (missing/unreadable file) or Available.
    Availability availability() const override;
    QDateTime lastModified() const override;

    std::optional<QByteArray> getBytes() override;
    bool saveBytes(const QByteArray &bytes) override;

protected:
    /// Writes @p bytes to the temporary file. Virtual so tests can simulate a failing write.
    virtual bool writeContents(QIODevice &device, const QByteArray &bytes);

private:
    const QString m_path;
    std::atomic<bool> m_forcedReadOnly;
};

} // namespace EasePass::Core
