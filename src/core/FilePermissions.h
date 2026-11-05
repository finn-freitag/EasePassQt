#pragma once

#include <QFileDevice>

namespace EasePass::Core {

/// Owner read/write only (0600 on Unix). Used for files that hold database contents, exports
/// and logs (steps.txt T23). On Windows Qt maps this to the read-only attribute only, so it is
/// effectively a no-op there; NTFS ACLs of the user profile protect the files instead.
inline constexpr QFileDevice::Permissions ownerOnlyFilePermissions =
    QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadUser | QFileDevice::WriteUser;

/// Best effort: restricts an open (or not yet opened) file to its owner.
inline bool restrictToOwner(QFileDevice &file) { return file.setPermissions(ownerOnlyFilePermissions); }

} // namespace EasePass::Core
