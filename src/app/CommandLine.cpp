#include "CommandLine.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFileInfo>
#include <QUrl>

namespace EasePass::App {

namespace {
void setup(QCommandLineParser &parser) {
    parser.setApplicationDescription(QCoreApplication::translate("CommandLine", "Ease Pass password manager"));
    parser.addOption(QCommandLineOption({QStringLiteral("d"), QStringLiteral("db")},
                                        QCoreApplication::translate("CommandLine", "Database file to open."),
                                        QStringLiteral("path")));
    parser.addPositionalArgument(QStringLiteral("database"),
                                 QCoreApplication::translate("CommandLine", "Database file to open (same as --db)."),
                                 QStringLiteral("[database]"));
}

QString normalize(const QString &arg) {
    const QUrl url(arg);
    const QString local = url.isLocalFile() ? url.toLocalFile() : arg;
    return local.isEmpty() ? QString() : QFileInfo(local).absoluteFilePath();
}
} // namespace

CommandLineOptions parseCommandLine(const QStringList &arguments) {
    CommandLineOptions options;
    QCommandLineParser parser;
    setup(parser);
    const QCommandLineOption help = parser.addHelpOption();
    const QCommandLineOption version = parser.addVersionOption();
    if (!parser.parse(arguments)) {
        options.error = parser.errorText();
        return options;
    }
    options.helpRequested = parser.isSet(help);
    options.versionRequested = parser.isSet(version);
    QString path = parser.value(QStringLiteral("db"));
    const QStringList positional = parser.positionalArguments();
    if (path.isEmpty() && !positional.isEmpty())
        path = positional.first();
    if (positional.size() > (parser.isSet(QStringLiteral("db")) ? 0 : 1))
        options.error = QCoreApplication::translate("CommandLine", "Only one database can be opened.");
    options.databasePath = normalize(path);
    return options;
}

QString commandLineHelp() {
    QCommandLineParser parser;
    setup(parser);
    parser.addHelpOption();
    parser.addVersionOption();
    // The usage line names the executable from QCoreApplication (a full store path, or
    // "<executable_name>" when main() prints the help before the app exists): use the command.
    QString text = parser.helpText();
    const qsizetype usage = text.indexOf(QLatin1Char(' '));
    const qsizetype end = text.indexOf(QStringLiteral(" ["), usage);
    if (usage > 0 && end > usage)
        text.replace(usage + 1, end - usage - 1, QStringLiteral("easepass"));
    return text;
}

} // namespace EasePass::App
