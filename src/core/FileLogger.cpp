#include "FileLogger.h"

#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTimeZone>
#include <algorithm>
#include <cstdio>

namespace EasePass::Core {

namespace {
QtMessageHandler g_previousHandler = nullptr;
FileLogger *g_logger = nullptr;
QMutex g_installMutex;

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message) {
    {
        QMutexLocker lock(&g_installMutex);
        if (g_logger)
            g_logger->write(type, context, message);
    }
    const QString redacted = FileLogger::redact(message);
    if (g_previousHandler) {
        g_previousHandler(type, context, redacted);
    } else {
        const QByteArray line = qFormatLogMessage(type, context, redacted).toLocal8Bit();
        fprintf(stderr, "%s\n", line.constData());
    }
}

const char *typeName(QtMsgType type) {
    switch (type) {
    case QtDebugMsg: return "debug";
    case QtInfoMsg: return "info";
    case QtWarningMsg: return "warning";
    case QtCriticalMsg: return "critical";
    case QtFatalMsg: return "fatal";
    }
    return "log";
}

const QString kPrefix = QStringLiteral("easepass_");
const QString kSuffix = QStringLiteral(".log");
} // namespace

FileLogger::FileLogger(Options options) : m_options(std::move(options)) {
    m_options.maxFiles = std::max(1, m_options.maxFiles);
    m_options.maxFileBytes = std::max<qint64>(1024, m_options.maxFileBytes);
}

FileLogger::~FileLogger() {
    uninstall();
    close();
}

QString FileLogger::defaultDirectory() {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath(QStringLiteral("logs"));
}

bool FileLogger::open() {
    QMutexLocker lock(&m_mutex);
    if (m_file.isOpen())
        return true;
    if (m_options.directory.isEmpty() || !QDir().mkpath(m_options.directory))
        return false;
    m_baseName = kPrefix + QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss"));
    m_part = 1;
    prune();
    if (!startFile())
        return false;
    writeHeader();
    return true;
}

void FileLogger::close() {
    QMutexLocker lock(&m_mutex);
    if (m_file.isOpen()) {
        m_file.flush();
        m_file.close();
    }
}

bool FileLogger::isOpen() const {
    QMutexLocker lock(&m_mutex);
    return m_file.isOpen();
}

QString FileLogger::currentFile() const {
    QMutexLocker lock(&m_mutex);
    return m_file.fileName();
}

bool FileLogger::startFile() {
    if (m_file.isOpen())
        m_file.close();
    QString name = m_baseName;
    if (m_part > 1)
        name += QStringLiteral("_%1").arg(m_part);
    m_file.setFileName(QDir(m_options.directory).filePath(name + kSuffix));
    // Log files can contain paths and database names: owner-only where the OS supports it.
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return false;
    m_file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return true;
}

void FileLogger::prune() {
    QDir dir(m_options.directory);
    QFileInfoList files = dir.entryInfoList({kPrefix + QStringLiteral("*") + kSuffix}, QDir::Files, QDir::Time);
    m_removed = 0;
    const QDateTime threshold = QDateTime::currentDateTime().addDays(-m_options.maxAgeDays);
    int kept = 0;
    for (const QFileInfo &fi : files) { // newest first
        // Room for the file of this session: keep maxFiles - 1 old ones.
        const bool tooOld = fi.lastModified() < threshold;
        const bool tooMany = kept >= m_options.maxFiles - 1;
        if (tooOld || tooMany) {
            if (QFile::remove(fi.absoluteFilePath()))
                ++m_removed;
        } else {
            ++kept;
        }
    }
}

void FileLogger::writeHeader() {
    QString h;
    h += QStringLiteral("Ease Pass Version: %1\n").arg(m_options.appVersion);
    h += QStringLiteral("Qt Version: %1 (built with %2)\n").arg(QString::fromLatin1(qVersion()), QStringLiteral(QT_VERSION_STR));
    h += QStringLiteral("OS: %1\n").arg(QSysInfo::prettyProductName());
    h += QStringLiteral("Kernel: %1 %2\n").arg(QSysInfo::kernelType(), QSysInfo::kernelVersion());
    h += QStringLiteral("CPU Architecture: %1 (build ABI %2)\n").arg(QSysInfo::currentCpuArchitecture(), QSysInfo::buildAbi());
    h += QStringLiteral("Locale: %1, UI languages: %2\n").arg(QLocale().name(), QLocale().uiLanguages().join(QLatin1Char(',')));
    h += QStringLiteral("Timezone: %1\n").arg(QString::fromLatin1(QTimeZone::systemTimeZoneId()));
    if (m_removed > 0)
        h += QStringLiteral("Removed %1 old log files.\n").arg(m_removed);
    h += QLatin1Char('\n');
    m_file.write(h.toUtf8());
    m_file.flush();
}

void FileLogger::write(QtMsgType type, const QMessageLogContext &context, const QString &message) {
    QMutexLocker lock(&m_mutex);
    if (!m_file.isOpen())
        return;
    QString line = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    line += QStringLiteral(" [%1]").arg(QLatin1String(typeName(type)));
    if (context.category && qstrcmp(context.category, "default") != 0)
        line += QLatin1Char(' ') + QLatin1String(context.category) + QLatin1Char(':');
    line += QLatin1Char(' ') + redact(message) + QLatin1Char('\n');
    m_file.write(line.toUtf8());
    if (type >= QtWarningMsg)
        m_file.flush();
    if (m_file.size() >= m_options.maxFileBytes) {
        ++m_part;
        startFile();
    }
}

void FileLogger::flush() {
    QMutexLocker lock(&m_mutex);
    if (m_file.isOpen())
        m_file.flush();
}

QString FileLogger::redact(const QString &message) {
    static const QRegularExpression otpauth(QStringLiteral("otpauth://\\S+"),
                                            QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression keyValue(
        QStringLiteral("\\b(password|passwd|pwd|secret|token|second ?factor|masterpassword|key)(\\s*[=:]\\s*)(\"[^\"]*\"|'[^']*'|\\S+)"),
        QRegularExpression::CaseInsensitiveOption);
    QString out = message;
    out.replace(otpauth, QStringLiteral("otpauth://[redacted]"));
    out.replace(keyValue, QStringLiteral("\\1\\2[redacted]"));
    return out;
}

void FileLogger::install() {
    QMutexLocker lock(&g_installMutex);
    if (m_installed)
        return;
    g_logger = this;
    m_installed = true;
    QtMessageHandler previous = qInstallMessageHandler(messageHandler);
    if (previous != messageHandler)
        g_previousHandler = previous;
}

void FileLogger::uninstall() {
    QMutexLocker lock(&g_installMutex);
    if (!m_installed)
        return;
    m_installed = false;
    if (g_logger == this) {
        g_logger = nullptr;
        qInstallMessageHandler(g_previousHandler);
        g_previousHandler = nullptr;
    }
}

} // namespace EasePass::Core
