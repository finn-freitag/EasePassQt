#include "LocalFileDatabaseSource.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QSaveFile>

#ifdef Q_OS_UNIX
#include <fcntl.h>
#include <unistd.h>
#endif

namespace EasePass::Core {

Q_LOGGING_CATEGORY(lcLocalSource, "easepass.core.localsource")

namespace {

bool syncFileToDisk(QFileDevice &file) {
#ifdef Q_OS_UNIX
    const int fd = file.handle();
    if (fd < 0) {
        return false;
    }
    return ::fsync(fd) == 0;
#else
    // Windows: QSaveFile::commit() calls FlushFileBuffers through the file engine.
    Q_UNUSED(file);
    return true;
#endif
}

void syncDirectory(const QString &dirPath) {
#ifdef Q_OS_UNIX
    const QByteArray native = QFile::encodeName(dirPath);
    const int fd = ::open(native.constData(), O_RDONLY | O_DIRECTORY);
    if (fd >= 0) {
        ::fsync(fd); // best effort: makes the rename durable
        ::close(fd);
    }
#else
    Q_UNUSED(dirPath);
#endif
}

} // namespace

LocalFileDatabaseSource::LocalFileDatabaseSource(const QString &path, bool readOnly, QObject *parent)
    : IDatabaseSource(parent), m_path(path), m_forcedReadOnly(readOnly) {}

QString LocalFileDatabaseSource::backupPathFor(const QString &path) {
    return path + QStringLiteral(".bak");
}

void LocalFileDatabaseSource::setReadOnly(bool readOnly) {
    if (m_forcedReadOnly.exchange(readOnly) != readOnly) {
        emit changed();
    }
}

QString LocalFileDatabaseSource::databaseName() const {
    // C#: Path.GetFileNameWithoutExtension (removes only the last extension)
    return QFileInfo(m_path).completeBaseName();
}

QString LocalFileDatabaseSource::sourceDescription() const {
    return m_path;
}

bool LocalFileDatabaseSource::isReadOnly() const {
    if (m_forcedReadOnly.load()) {
        return true;
    }
    QFileInfo info(m_path);
    return info.exists() && !info.isWritable();
}

IDatabaseSource::Availability LocalFileDatabaseSource::availability() const {
    if (m_path.isEmpty()) {
        return Availability::UnknownState;
    }
    QFileInfo info(m_path);
    if (!info.exists() || !info.isFile() || !info.isReadable()) {
        return Availability::Unavailable;
    }
    return Availability::Available;
}

QDateTime LocalFileDatabaseSource::lastModified() const {
    QFileInfo info(m_path);
    return info.exists() ? info.lastModified() : QDateTime();
}

std::optional<QByteArray> LocalFileDatabaseSource::getBytes() {
    if (m_path.isEmpty()) {
        return std::nullopt;
    }
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    return file.readAll();
}

bool LocalFileDatabaseSource::writeContents(QIODevice &device, const QByteArray &bytes) {
    return device.write(bytes) == bytes.size();
}

bool LocalFileDatabaseSource::saveBytes(const QByteArray &bytes) {
    if (m_forcedReadOnly.load() || m_path.isEmpty()) {
        return false;
    }

    QSaveFile file(m_path);
    // Never fall back to writing the target directly: that would not be atomic.
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(lcLocalSource) << "Cannot create temporary file for" << m_path << ":" << file.errorString();
        return false;
    }

    if (!writeContents(file, bytes) || !file.flush() || !syncFileToDisk(file)) {
        qCWarning(lcLocalSource) << "Writing database failed for" << m_path << ":" << file.errorString();
        file.cancelWriting();
        return false;
    }

    if (QFile::exists(m_path)) {
        const QString bak = backupPathFor(m_path);
        QFile::remove(bak);
        if (!QFile::copy(m_path, bak)) {
            qCWarning(lcLocalSource) << "Could not create backup" << bak;
        }
    }

    if (!file.commit()) {
        qCWarning(lcLocalSource) << "Replacing database failed for" << m_path << ":" << file.errorString();
        return false;
    }

    syncDirectory(QFileInfo(m_path).absolutePath());
    emit changed();
    return true;
}

} // namespace EasePass::Core
