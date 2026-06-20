#include "engine.h"
#include "engine/core/config_loader.h"
#include "engine/platform/platform.h"
#include "engine/renderer/gl_es3.h"
#include "engine/audio/audio_manager.h"

#include <filesystem>
#include <thread>
#include <system_error>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if defined(__ANDROID__) || (defined(__APPLE__) && TARGET_OS_IOS)
#include <time.h>
static u64 monotonic_now() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<u64>(ts.tv_sec) * 1000000000ULL + static_cast<u64>(ts.tv_nsec);
}
#endif

#if !defined(__ANDROID__) && !(defined(__APPLE__) && TARGET_OS_IOS)
#include <SDL.h>
#include <chrono>
#endif

// IntelliSense fallback — PINO_VERSION is set by CMake at build time
#ifndef PINO_VERSION
#define PINO_VERSION "0.0.0"
#endif

namespace pino {

// ─── Asset root discovery (desktop only) ───────────────────────
// Walks up from the executable directory to find an "assets/"
// directory at the build root. This eliminates the need for a
// fragile compile-time PINO_ASSET_DIR relative path.
#if !defined(__ANDROID__) && !(defined(__APPLE__) && TARGET_OS_IOS)
static std::string find_asset_root(const std::string& exe_dir) {
    namespace fs = std::filesystem;
    fs::path dir(exe_dir);
    std::error_code ec;
    for (int i = 0; i < 10; ++i) {
        auto candidate = dir / "assets";
        if (fs::is_directory(candidate, ec))
            return candidate.string() + '/';
        dir = dir.parent_path();
        if (dir == dir.root_path()) break;
    }
    // Fallback: assume assets/ is next to the executable
    return exe_dir + "assets/";
}
#endif

Engine::Engine() = default;
Engine::~Engine() { shutdown(); }

bool Engine::init(const EngineConfig& config) {
    // ─── Logger init ──────────────────────────────────────────
    Logger::init("engine.log");
    Logger::set_level(config.log_level);

    // ─── Config loading ──────────────────────────────────────
    EngineConfig effective;
#if !defined(__ANDROID__) && !(defined(__APPLE__) && TARGET_OS_IOS)
    // Desktop: load config file, then override with any explicit EngineConfig fields
    effective = load_config();
    if (config.app_title && config.app_title[0]) effective.app_title = config.app_title;
    if (config.window_width  != 1280) effective.window_width  = config.window_width;
    if (config.window_height != 720)  effective.window_height = config.window_height;
    if (config.fullscreen  != false)  effective.fullscreen  = config.fullscreen;
    if (config.resizable   != false)  effective.resizable   = config.resizable;
    if (config.vsync       != true)   effective.vsync       = config.vsync;
    if (config.fixed_update_rate != 60) effective.fixed_update_rate = config.fixed_update_rate;
    if (config.log_level   != LogLevel::Debug) effective.log_level = config.log_level;
    if (config.audio_enabled != true) effective.audio_enabled = config.audio_enabled;
    if (config.audio_max_voices != 32) effective.audio_max_voices = config.audio_max_voices;
#else
    // Mobile: use config as-is (config file not supported)
    effective = config;
#endif

    m_config = effective;
    Logger::set_level(effective.log_level);

    // ─── Startup diagnostics ──────────────────────────────────
    {
        const char* build =
#if defined(NDEBUG)
            "Release";
#elif defined(_DEBUG)
            "Debug";
#else
            "Release";
#endif
        const char* platform =
#if defined(__ANDROID__)
            "Android";
#elif defined(__APPLE__) && TARGET_OS_IOS
            "iOS";
#elif defined(__APPLE__) && TARGET_OS_MAC
            "macOS";
#elif defined(_WIN32)
            "Windows";
#elif defined(__linux__)
            "Linux";
#else
            "Unknown";
#endif

        PINO_INFO("--- Pino Engine v%s ---", PINO_VERSION);
        PINO_INFO("Build: %s  Platform: %s", build, platform);
        PINO_INFO("CPU threads: %d", static_cast<int>(std::thread::hardware_concurrency()));
        PINO_INFO("Config: %ux%u  title=\"%s\"  vsync=%s  log=%s  fps=%u",
                  effective.window_width, effective.window_height,
                  effective.app_title ? effective.app_title : "",
                  effective.vsync ? "on" : "off",
                  effective.log_level == LogLevel::Debug ? "debug" :
                  effective.log_level == LogLevel::Info  ? "info"  :
                  effective.log_level == LogLevel::Warn  ? "warn"  : "error",
                  effective.fixed_update_rate);
    }

    // ─── Window ───────────────────────────────────────────────
    WindowConfig wc;
    wc.title      = effective.app_title;
    wc.width      = effective.window_width;
    wc.height     = effective.window_height;
    wc.fullscreen = effective.fullscreen;
    wc.resizable  = effective.resizable;
    wc.gl_es      = effective.gl_es;
    wc.gl_major   = effective.gl_major;
    wc.gl_minor   = effective.gl_minor;

#if defined(__ANDROID__) || (defined(__APPLE__) && TARGET_OS_IOS)
    wc.existing_native_window = effective.native_window;
#endif

#if !defined(__ANDROID__) && !(defined(__APPLE__) && TARGET_OS_IOS)
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) < 0) {
        PINO_ERROR("SDL_Init failed: %s", SDL_GetError());
        return false;
    }
#endif

    m_window = create_window(wc);
    if (!m_window || !m_window->native_handle()) {
        PINO_ERROR("Window creation failed");
#if !defined(__ANDROID__) && !(defined(__APPLE__) && TARGET_OS_IOS)
        SDL_Quit();
#endif
        return false;
    }

#if !defined(__ANDROID__) && !(defined(__APPLE__) && TARGET_OS_IOS)
    SDL_GL_SetSwapInterval(effective.vsync ? 1 : 0);
#endif

    if (!gl::init()) {
        PINO_ERROR("OpenGL loading failed");
        m_window.reset();
#if !defined(__ANDROID__) && !(defined(__APPLE__) && TARGET_OS_IOS)
        SDL_Quit();
#endif
        return false;
    }

    m_input = create_input();
    Input::set_instance(m_input.get());

#if defined(__ANDROID__)
    m_filesystem = create_android_file_system(effective.asset_manager);
    m_last_tick  = monotonic_now();
#elif defined(__APPLE__) && TARGET_OS_IOS
    m_filesystem = create_ios_file_system();
    m_last_tick  = monotonic_now();
#else
    char* base = SDL_GetBasePath();
    std::string exe_dir = base ? base : ".";
    SDL_free(base);
    {
        std::string asset_root = find_asset_root(exe_dir);
        m_filesystem = create_file_system(asset_root);
    }
    m_last_tick = SDL_GetPerformanceCounter();
#endif

    m_audio.reset(new AudioManager);
    if (effective.audio_enabled) {
        if (!m_audio->init(*m_filesystem, effective.audio_max_voices)) {
            PINO_WARN("Audio disabled (init failed)");
        }
    }

    m_assets.reset(new AssetManager(*m_filesystem));

    EngineContext::create(*m_window, *m_input, *m_filesystem,
                          *m_audio, *m_assets,
                          m_timers, m_stats, m_config);

    m_running   = true;
    PINO_INFO("Engine initialized");
    return true;
}

void Engine::shutdown() {
    if (m_engine_shutdown) return;
    m_engine_shutdown = true;
    m_running = false;
    PINO_INFO("Engine shutting down");
    m_timers.clear();
    EngineContext::destroy();
    m_assets.reset();
    m_audio.reset();
    m_filesystem.reset();
    m_input.reset();
    m_window.reset();
#if !defined(__ANDROID__) && !(defined(__APPLE__) && TARGET_OS_IOS)
    SDL_Quit();
#endif
    Logger::shutdown();
}

void Engine::begin_frame() {
    m_audio->tick();
    m_input->begin_frame();

#if !defined(__ANDROID__) && !(defined(__APPLE__) && TARGET_OS_IOS)
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) m_running = false;
        m_input->process_event(&event);
    }

    if (m_input->quit_requested())
        m_running = false;
#endif

    if (m_quit_requested) {
        m_running = false;
        m_quit_requested = false;
    }

#if defined(__ANDROID__) || (defined(__APPLE__) && TARGET_OS_IOS)
    u64 now = monotonic_now();
    u64 freq = 1000000000ULL;
#else
    u64 now  = SDL_GetPerformanceCounter();
    u64 freq = SDL_GetPerformanceFrequency();
#endif

    m_dt = static_cast<f32>(static_cast<f64>(now - m_last_tick) / static_cast<f64>(freq));
    if (m_dt > 0.1f) m_dt = 0.1f;
    if (m_dt < 0.0001f) m_dt = 0.0001f;
    m_elapsed  += m_dt;
    m_last_tick = now;
}

void Engine::end_frame() {
    m_window->swap_buffers();
}

void Engine::step_game(IGame& game) {
    // Handle pause toggle (e.g. Android back button) — check BEFORE
    // begin_frame() clears the request flag
    if (m_input && m_input->pause_requested()) {
        m_paused = !m_paused;
    }

    begin_frame();

    f32 update_dur = 0.0f;
    f32 render_dur = 0.0f;

    if (!m_paused) {
        m_accumulator += m_dt;
        if (m_accumulator > 0.25f) m_accumulator = 0.25f;

        f32 FIXED_DT = 1.0f / static_cast<f32>(m_config.fixed_update_rate);
        while (m_accumulator >= FIXED_DT) {
#if !defined(__ANDROID__) && !(defined(__APPLE__) && TARGET_OS_IOS)
            u64 update_start = SDL_GetPerformanceCounter();
#else
            u64 update_start = monotonic_now();
#endif
            game.update(FIXED_DT);
#if !defined(__ANDROID__) && !(defined(__APPLE__) && TARGET_OS_IOS)
            u64 update_end = SDL_GetPerformanceCounter();
            update_dur += static_cast<f32>(static_cast<f64>(update_end - update_start) /
                                           static_cast<f64>(SDL_GetPerformanceFrequency()));
#else
            u64 update_end = monotonic_now();
            update_dur += static_cast<f32>(static_cast<f64>(update_end - update_start) / 1e9);
#endif
            m_accumulator -= FIXED_DT;
        }

        // Advance timers (respects pause via m_paused check above)
        m_timers.update(m_dt, true);
    }

#if !defined(__ANDROID__) && !(defined(__APPLE__) && TARGET_OS_IOS)
    u64 render_start = SDL_GetPerformanceCounter();
#else
    u64 render_start = monotonic_now();
#endif
    game.render(m_dt);
#if !defined(__ANDROID__) && !(defined(__APPLE__) && TARGET_OS_IOS)
    u64 render_end = SDL_GetPerformanceCounter();
    render_dur = static_cast<f32>(static_cast<f64>(render_end - render_start) /
                                   static_cast<f64>(SDL_GetPerformanceFrequency()));
#else
    u64 render_end = monotonic_now();
    render_dur = static_cast<f32>(static_cast<f64>(render_end - render_start) / 1e9);
#endif

    m_stats.tick(m_dt, update_dur, render_dur);
    end_frame();
}

void Engine::run(IGame& game) {
    if (!m_running) return;

    if (!game.init()) {
        PINO_ERROR("IGame::init() returned false - aborting");
        shutdown();
        return;
    }

    m_accumulator = 0.0f;
    u32 frame_count = 0;

    PINO_INFO("Game loop started (fixed update: %u Hz)", m_config.fixed_update_rate);

    while (m_running) {
        step_game(game);
        ++frame_count;
    }

    PINO_INFO("Game loop exited after %u frames  (avg FPS: %.1f)", frame_count, m_stats.fps);
    game.shutdown();
}

} // namespace pino
