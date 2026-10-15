#pragma once

namespace EasePass::Core {

/**
 * @brief Interface for controlling process core dump behavior.
 *
 * Disabling core dumps prevents sensitive memory (passwords, keys) from being
 * written to disk in crash dumps or debug core files.
 */
class ICoreDumpControl {
public:
    virtual ~ICoreDumpControl() = default;

    /**
     * @brief Disables core dump generation for the current process.
     * @return true if core dumps were successfully disabled or not supported; false on error.
     */
    virtual bool disableCoreDumps() = 0;

    /**
     * @brief Checks whether the process is currently dumpable.
     * @return true if dumpable, false otherwise.
     */
    virtual bool isDumpable() const = 0;
};

/**
 * @brief Platform-native core dump controller (uses prctl PR_SET_DUMPABLE on Linux).
 */
class PlatformCoreDumpControl : public ICoreDumpControl {
public:
    bool disableCoreDumps() override;
    bool isDumpable() const override;
};

} // namespace EasePass::Core
