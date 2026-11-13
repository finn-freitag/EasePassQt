#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

struct wasm_engine_t;

namespace EasePass::Plugin {

/**
 * The process-wide Wasmtime engine for plugins (T26 proof of concept).
 *
 * Timeouts use epoch interruption: while at least one plugin call is running, a ticker
 * thread advances the engine epoch every TickMs. Each store checks its own deadline in an
 * epoch callback, so one plugin running out of time never interrupts another one.
 */
class PluginEngine {
public:
    static constexpr int TickMs = 20;

    static PluginEngine &instance();

    wasm_engine_t *engine() const { return m_engine; }

    /// Keeps the epoch ticker running for the lifetime of the object (one per plugin call).
    class CallGuard {
    public:
        CallGuard();
        ~CallGuard();
        CallGuard(const CallGuard &) = delete;
        CallGuard &operator=(const CallGuard &) = delete;
    };

private:
    PluginEngine();
    ~PluginEngine();
    void tick();

    wasm_engine_t *m_engine = nullptr;
    std::mutex m_mutex;
    std::condition_variable m_wake;
    int m_activeCalls = 0;
    bool m_quit = false;
    std::thread m_ticker;
};

} // namespace EasePass::Plugin
