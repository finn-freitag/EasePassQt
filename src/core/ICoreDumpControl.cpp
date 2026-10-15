#include "ICoreDumpControl.h"

#if defined(__linux__)
#include <sys/prctl.h>
#endif

namespace EasePass::Core {

bool PlatformCoreDumpControl::disableCoreDumps() {
#if defined(__linux__)
    if (::prctl(PR_SET_DUMPABLE, 0) != 0) {
        return false;
    }
    return true;
#else
    return true;
#endif
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
