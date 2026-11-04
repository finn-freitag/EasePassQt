#include "DatabaseOperationResult.h"

#include <QCoreApplication>

namespace EasePass::Core {

QString DatabaseOpResult::describe(DatabaseOpStatus status) {
    const char *ctx = "DatabaseOperations";
    switch (status) {
    case DatabaseOpStatus::Success:
        return QString();
    case DatabaseOpStatus::InvalidPath:
        return QCoreApplication::translate(ctx, "The database path or name is invalid.");
    case DatabaseOpStatus::PathAlreadyExists:
        return QCoreApplication::translate(ctx, "A database with that name already exists.");
    case DatabaseOpStatus::PasswordsDoNotMatch:
        return QCoreApplication::translate(ctx, "The passwords do not match.");
    case DatabaseOpStatus::PasswordTooShort:
        return QCoreApplication::translate(ctx, "The password must be at least %n characters long.", nullptr,
                                           MinMasterPasswordLength);
    case DatabaseOpStatus::InvalidSecondFactor:
        return QCoreApplication::translate(ctx, "The second factor token is missing or invalid.");
    case DatabaseOpStatus::UnsupportedSecondFactorType:
        return QCoreApplication::translate(ctx, "This second factor type is not supported.");
    case DatabaseOpStatus::WrongPassword:
        return QCoreApplication::translate(ctx, "The password or second factor is wrong.");
    case DatabaseOpStatus::NeedsSecondFactor:
        return QCoreApplication::translate(ctx, "The database requires a second factor.");
    case DatabaseOpStatus::NotFound:
        return QCoreApplication::translate(ctx, "The database could not be found.");
    case DatabaseOpStatus::WrongFormat:
        return QCoreApplication::translate(ctx, "The file is not a valid Ease Pass database.");
    case DatabaseOpStatus::SourceLocked:
        return QCoreApplication::translate(ctx, "The database is locked by another user.");
    case DatabaseOpStatus::NoDatabaseOpen:
        return QCoreApplication::translate(ctx, "No database is open.");
    case DatabaseOpStatus::ReadOnly:
        return QCoreApplication::translate(ctx, "The database is read-only.");
    case DatabaseOpStatus::IsLoadedDatabase:
        return QCoreApplication::translate(ctx, "This is the currently loaded database. Load another database first.");
    case DatabaseOpStatus::LastDatabase:
        return QCoreApplication::translate(ctx, "The only remaining database cannot be deleted.");
    case DatabaseOpStatus::SameAsOpenDatabase:
        return QCoreApplication::translate(ctx, "The open database cannot be overwritten by an export.");
    case DatabaseOpStatus::IoError:
        return QCoreApplication::translate(ctx, "The database file could not be written.");
    }
    return QString();
}

DatabaseOpStatus DatabaseOpResult::fromLoadResult(LoadResult result) {
    switch (result) {
    case LoadResult::Success:
        return DatabaseOpStatus::Success;
    case LoadResult::WrongPassword:
        return DatabaseOpStatus::WrongPassword;
    case LoadResult::DatabaseNotFound:
    case LoadResult::SourceUnavailable:
        return DatabaseOpStatus::NotFound;
    case LoadResult::WrongFormat:
        return DatabaseOpStatus::WrongFormat;
    case LoadResult::NeedsSecondFactor:
        return DatabaseOpStatus::NeedsSecondFactor;
    case LoadResult::SourceLocked:
        return DatabaseOpStatus::SourceLocked;
    case LoadResult::Error:
        return DatabaseOpStatus::IoError;
    }
    return DatabaseOpStatus::IoError;
}

} // namespace EasePass::Core
