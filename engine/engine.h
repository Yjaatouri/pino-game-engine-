#pragma once

#include "engine/core/types.h"
#include "engine/core/log.h"
#include "engine/core/engine_stats.h"
#include "engine/core/timer.h"
#include "engine/platform/window.h"
#include "engine/platform/input.h"
#include "engine/platform/file_system.h"
#include "engine/game.h"
#include "engine/audio/audio_manager.h"
#include "engine/assets/asset_manager.h"
#include "engine/core/engine_context.h"

#include <memory>

#if defined(__ANDROID__)
#include <android/asset_manager.h>
#include <android/native_window.h>
#endif

namespace pino {

struct EngineConfig {
    const char* app_title        = "Pino Engine";
    u32         window_width     = 1280;
    u32         window_height    = 720;
    bool        fullscreen      = false;
    bool        resizable       = false;
    bool        vsync           = true;
    bool        gl_es           = true;
    u32         gl_major        = 3;
    u32         gl_minor        = 0;
    u32         fixed_update_rate = 60;
    LogLevel    log_level       = LogLevel::Debug;
    bool        audio_enabled   = true;
    u32         audio_max_voices = 32;

#if defined(__ANDROID__)
    ANativeWindow* native_window = nullptr;
    AAssetManager* asset_manager = nullptr;
#elif defined(__APPLE__) && TARGET_OS_IOS
    void*          native_window = nullptr;  // EAGLView*
#endif
};

class Engine {
public:
    Engine();
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    bool init(const EngineConfig& config);
    void shutdown();

    // High-level game loop: calls game.init/update/render/shutdown.
    void run(IGame& game);

    // Single game step — for custom loops (Android NativeActivity).
    void step_game(IGame& game);

    bool is_running() const { return m_running; }

    // Pause / resume (pauses update loop; render continues).
    void pause()  { m_paused = true; }
    void resume() { m_paused = false; }
    bool is_paused() const { return m_paused; }

    // Access platform objects
    Window&     window()     { return *m_window; }
    Input&      input()      { return *m_input; }
    FileSystem& filesystem() { return *m_filesystem; }

    // Access audio (may not be initialized if config disables it)
    AudioManager& audio() { return *m_audio; }

    // Access asset manager (always available after init)
    AssetManager& assets() { return *m_assets; }

    // Access config
    const EngineConfig& config() const { return m_config; }

    // Frame control — used directly only when not using run()
    void begin_frame();
    void end_frame();

    // Time
    f32 delta_time()  const { return m_dt; }
    f32 elapsed_time() const { return m_elapsed; }

    // Stats (rolling averages, updated every frame)
    const EngineStats& stats() const { return m_stats; }

    // Timer manager (updated by engine each frame)
    TimerManager& timers() { return m_timers; }

    // Request graceful quit (checked at next begin_frame)
    void request_quit() { m_quit_requested = true; }

private:
    bool m_running = false;
    bool m_paused  = false;
    bool m_quit_requested = false;
    bool m_engine_shutdown = false;

    EngineConfig m_config;

    std::unique_ptr<Window>     m_window;
    std::unique_ptr<Input>      m_input;
    std::unique_ptr<FileSystem> m_filesystem;
    std::unique_ptr<AudioManager> m_audio;
    std::unique_ptr<AssetManager> m_assets;

    TimerManager  m_timers;
    EngineStats   m_stats;

    f32  m_dt          = 0.016667f;
    f32  m_elapsed     = 0.0f;
    f32  m_accumulator = 0.0f;
    u64  m_last_tick   = 0;
};

} // namespace pino
