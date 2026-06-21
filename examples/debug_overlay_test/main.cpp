#include "engine/engine.h"
#include "engine/renderer/font.h"
#include "engine/renderer/text_renderer.h"
#include "engine/renderer/render_stats.h"
#include "engine/renderer/debug_overlay.h"
#include <cstdio>
#include <chrono>
#include <thread>

using namespace pino;

int main() {
    EngineConfig cfg;
    cfg.app_title     = "Debug Overlay Test";
    cfg.window_width  = 800;
    cfg.window_height = 600;
    cfg.resizable     = true;

    Engine eng;
    if (!eng.init(cfg)) return 1;

    Font font;
    if (!font.load_builtin()) { PINO_ERROR("Font::load_builtin() failed"); return 1; }

    TextRenderer tr;
    if (!tr.init(cfg.window_width, cfg.window_height)) return 1;
    i32 fw = cfg.window_width, fh = cfg.window_height;

    DebugOverlay overlay;

    auto t0 = std::chrono::steady_clock::now();
    int frames = 0;
    float fps = 60.0f;
    float fps_timer = 0;

    PINO_INFO("Press F3 to toggle debug overlay");

    while (eng.is_running()) {
        eng.begin_frame();

        overlay.handle_input(eng.input());

        ++frames;
        auto t1 = std::chrono::steady_clock::now();
        f32 dt = std::chrono::duration<f32>(t1 - t0).count();
        fps_timer += dt;
        if (fps_timer >= 0.5f) { fps = frames / fps_timer; frames = 0; fps_timer = 0; }
        t0 = t1;

        f32 frame_ms = dt * 1000.0f;
        overlay.set_frame_stats(fps, frame_ms, 0.0f, 0.0f);

        auto& rs = RenderStats::instance();
        overlay.set_render_stats(rs.draw_calls, rs.triangles);
        overlay.set_entity_count(42);
        overlay.set_asset_counts(
            eng.assets().mesh_cache_size(),
            eng.assets().texture_cache_size(),
            eng.assets().shader_cache_size()
        );

        glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        tr.begin_frame();

        tr.draw_text(font, "Debug Overlay Test  —  Press F3 to toggle", 30, 30, 1.0f, 1,1,1,1);
        tr.draw_text(font, overlay.is_visible() ? "OVERLAY VISIBLE" : "overlay hidden",
                     30, 52, 0.8f,
                     overlay.is_visible() ? 0.3f : 0.5f,
                     overlay.is_visible() ? 0.9f : 0.5f,
                     overlay.is_visible() ? 0.3f : 0.5f, 1);

        overlay.render(tr, font, fw, fh);

        tr.render(fw, fh);

        eng.end_frame();

        if (eng.input().key_pressed(Key::Escape)) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    tr.destroy();
    font.destroy();
    PINO_INFO("Exiting cleanly");
    return 0;
}
