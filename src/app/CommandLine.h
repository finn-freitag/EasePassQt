#pragma once

#include <QString>
#include <QStringList>

namespace EasePass::App {

/**
 * Command line of the QML app (steps.txt T19):
 *   easepass [--db <path>] [<path>]
 * A database given either way is preselected on the login page; a file that is not one of
 * the known databases is offered as temporary database (C#: TemporaryDatabaseHelper, file
 * association). --help / --version are handled by QCommandLineParser in main().
 */
struct CommandLineOptions {
    QString databasePath; ///< absolute local path, empty if none
    QString error;        ///< parse error (unknown option, ...)
    bool helpRequested = false;
    bool versionRequested = false;
};

/// Parses @p arguments (including the program name as first element).
CommandLineOptions parseCommandLine(const QStringList &arguments);
/// Help text shown for --help.
QString commandLineHelp();

} // namespace EasePass::App
