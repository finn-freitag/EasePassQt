#include "ICoreDumpControl.h"

#if defined(__linux__)
#include <sys/prctl.h>
#endif
#if defined(__unix__) || defined(__APPLE__)
#include <sys/resource.h>
#endif

namespace EasePass::Core {

bool PlatformCoreDumpControl::disableCoreDumps() {
    bool ok = true;
#if defined(__unix__) || defined(__APPLE__)
    // T23: core size limit 0 as well. PR_SET_DUMPABLE alone is Linux-only, and the limit
    // is also inherited by helper processes we might start.
    struct rlimit limit {};
    limit.rlim_cur = 0;
    limit.rlim_max = 0;
    if (::setrlimit(RLIMIT_CORE, &limit) != 0) {
        ok = false;
    }
#endif
#if defined(__linux__)
    // Not dumpable: no core file (also not via systemd-coredump / a core_pattern pipe), and
    // other processes of the same user cannot ptrace or read /proc/<pid>/mem.
    if (::prctl(PR_SET_DUMPABLE, 0) != 0) {
        ok = false;
    }
#endif
    // Windows: Windows Error Reporting may still write a dump if the user enabled LocalDumps;
    // see docs/SECURITY.md.
    return ok;
}

bool PlatformCoreDumpControl::isDumpable() const {
#if defined(__linux__)
    int res = ::prctl(PR_GET_DUMPABLE, 0, 0, 0, 0);
    return res > 0;
#else
    return false;
#endif
}

} // namespace EasePass::Core
