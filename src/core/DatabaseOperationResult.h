#pragma once

#include "DatabaseFile.h"

#include <QString>
#include <functional>

namespace EasePass::Core {

/**
 * @brief Outcome of a database management operation (DatabaseOperations / DatabaseService).
 *
 * C# reports these cases through InfoMessages (C#: Helper/InfoMessages.cs) and per-dialog result
 * enums (ChangePasswordPageResult, CreateDatabaseDialog checks). Here they are one enum so the
 * UI layer can map every case to one message; describe() returns a translated default text.
 */
enum class DatabaseOpStatus {
    Success,
    // Input validation
    InvalidPath,         ///< empty/invalid path or database name (C#: InvalidDatabasePath)
    PathAlreadyExists,   ///< C#: DatabaseWithThatNameAlreadyExists
    PasswordsDoNotMatch, ///< C#: PasswordsDoNotMatch
    PasswordTooShort,    ///< C#: PasswordTooShort (< MinMasterPasswordLength characters)
    InvalidSecondFactor, ///< second factor type needs a token but none was given
    UnsupportedSecondFactorType, ///< e.g. Authenticator (declared but never implemented in C#)
    // Opening / decrypting
    WrongPassword,       ///< wrong password or second factor; also: wrong "current password"
    NeedsSecondFactor,
    NotFound,
    WrongFormat,
    SourceLocked,
    // State
    NoDatabaseOpen,
    ReadOnly,
    IsLoadedDatabase,    ///< C#: CantDeleteLoadedDatabase (also used for rename)
    LastDatabase,        ///< C#: CantDeleteDatabase (the only known database)
    SameAsOpenDatabase,  ///< export target is the file of the open database
    // I/O
    IoError,
};

struct DatabaseOpResult {
    DatabaseOpStatus status = DatabaseOpStatus::Success;
    /// Operation-specific path: created/renamed/exported/imported file.
    QString path;

    DatabaseOpResult() = default;
    DatabaseOpResult(DatabaseOpStatus s, QString p = QString()) : status(s), path(std::move(p)) {}

    bool ok() const { return status == DatabaseOpStatus::Success; }
    /// Translated, user-presentable default message for status (empty for Success).
    QString message() const { return describe(status); }

    static QString describe(DatabaseOpStatus status);
    /// Maps a decryption/open result to an operation status.
    static DatabaseOpStatus fromLoadResult(LoadResult result);
};

using DatabaseOpCallback = std::function<void(const DatabaseOpResult &)>;

/// Minimum master password length (C#: CreateDatabaseDialogPage.PasswordLengthCorrect "> 3",
/// ChangePasswordPage "< 4" => too short). Counted in Unicode code points.
inline constexpr int MinMasterPasswordLength = 4;

} // namespace EasePass::Core
