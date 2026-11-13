#include "PluginEngine.h"

#include <chrono>
#include <wasmtime.h>

namespace EasePass::Plugin {

PluginEngine &PluginEngine::instance() {
    static PluginEngine engine;
    return engine;
}

PluginEngine::PluginEngine() {
    wasm_config_t *config = wasm_config_new();
    wasmtime_config_wasm_component_model_set(config, true);
    wasmtime_config_epoch_interruption_set(config, true);
    m_engine = wasm_engine_new_with_config(config); // takes ownership of config
    m_ticker = std::thread([this] { tick(); });
}

PluginEngine::~PluginEngine() {
    {
        std::lock_guard lock(m_mutex);
        m_quit = true;
    }
    m_wake.notify_all();
    m_ticker.join();
    wasm_engine_delete(m_engine);
}

void PluginEngine::tick() {
    std::unique_lock lock(m_mutex);
    while (!m_quit) {
        if (m_activeCalls == 0) {
            m_wake.wait(lock, [this] { return m_quit || m_activeCalls > 0; });
            continue;
        }
        m_wake.wait_for(lock, std::chrono::milliseconds(TickMs), [this] { return m_quit; });
        wasmtime_engine_increment_epoch(m_engine);
    }
}

PluginEngine::CallGuard::CallGuard() {
    PluginEngine &engine = PluginEngine::instance();
    {
        std::lock_guard lock(engine.m_mutex);
        ++engine.m_activeCalls;
    }
    engine.m_wake.notify_all();
}

PluginEngine::CallGuard::~CallGuard() {
    PluginEngine &engine = PluginEngine::instance();
    std::lock_guard lock(engine.m_mutex);
    --engine.m_activeCalls;
}

} // namespace EasePass::Plugin
