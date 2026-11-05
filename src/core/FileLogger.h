#pragma once

#include <QDateTime>
#include <QFile>
#include <QMutex>
#include <QString>
#include <QtLogging>

namespace EasePass::Core {

/**
 * @brief Writes Qt log messages (qDebug/qInfo/qWarning/qCritical, QLoggingCategory) to a log
 *        file in the app data directory (steps.txt T19; C#: Manager/LoggingManager.cs,
 *        Models/Logger/FileLogger.cs).
 *
 * One file per session ("easepass_<yyyy-MM-dd_HH-mm-ss>.log") starting with a system header
 * (versions, OS, CPU, locale) like C# FileLogger.Init. A file that grows beyond maxFileBytes
 * is continued in a new file ("..._2.log"). On open() files older than maxAgeDays (C#: 4 weeks)
 * are deleted and only the newest maxFiles are kept.
 *
 * Secrets are never logged on purpose; as a second line of defence every message passes
 * redact(), which masks otpauth URIs and "password=/secret=/token=..." style values.
 * Writing is thread-safe. install() makes this logger the process-wide Qt message handler and
 * forwards messages to the previous handler (stderr) as well.
 */
class FileLogger {
public:
    struct Options {
        QString directory;              ///< e.g. <AppDataLocation>/logs
        int maxFiles = 10;              ///< files kept after pruning (including the new one)
        qint64 maxFileBytes = 1 << 20;  ///< 1 MiB per file
        int maxAgeDays = 28;            ///< C#: thresholdWeeks = 4
        QString appVersion;             ///< written into the header
    };

    explicit FileLogger(Options options);
    ~FileLogger();
    FileLogger(const FileLogger &) = delete;
    FileLogger &operator=(const FileLogger &) = delete;

    /// Creates the directory, prunes old files and starts a new file with the header.
    bool open();
    void close();
    bool isOpen() const;
    QString currentFile() const;
    /// Number of old files deleted by the last open().
    int removedFiles() const { return m_removed; }

    void write(QtMsgType type, const QMessageLogContext &context, const QString &message);
    void flush();

    /// Masks secrets in @p message (otpauth URIs, key=value pairs with secret-like keys).
    static QString redact(const QString &message);
    /// Default directory: <AppDataLocation>/logs.
    static QString defaultDirectory();

    /// Routes all Qt messages to this logger (and the previous handler). One logger at a time.
    void install();
    void uninstall();

private:
    bool startFile();
    void prune();
    void writeHeader();

    Options m_options;
    mutable QMutex m_mutex;
    QFile m_file;
    QString m_baseName;
    int m_part = 1;
    int m_removed = 0;
    bool m_installed = false;
};

} // namespace EasePass::Core
