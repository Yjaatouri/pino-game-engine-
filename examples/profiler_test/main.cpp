#include "engine/engine.h"
#include "engine/renderer/font.h"
#include "engine/renderer/text_renderer.h"
#include "engine/renderer/render_stats.h"
#include "engine/scene/entity.h"
#include "engine/scene/scene.h"
#include "engine/physics/collision_world.h"
#include <cstdio>
#include <chrono>
#include <thread>

using namespace pino;

int main() {
    EngineConfig cfg;
    cfg.app_title     = "Profiler Overlay Test";
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

    // ── Collision world with test colliders ──────────────────────
    Scene scene;
    Entity* e1 = scene.root()->create_child("box1");
    Entity* e2 = scene.root()->create_child("box2");

    e1->local_transform().position = {-1.5f, 0, 0};
    e2->local_transform().position = { 1.5f, 0, 0};

    CollisionWorld cw;
    ColliderComponent cc1, cc2;
    cc1.local_min = {-0.5f, -0.5f, -0.5f};
    cc1.local_max = { 0.5f,  0.5f,  0.5f};
    cc2 = cc1;
    cw.register_collider(*e1, cc1);
    cw.register_collider(*e2, cc2);

    // ── Profiler ─────────────────────────────────────────────────
    ProfilerOverlay& profiler = eng.profiler();

    // Register a custom zone for the physics update
    u32 zone_physics = profiler.register_zone("Physics Total");

    PINO_INFO("Press F2 to toggle profiler overlay");

    float move_dir = 1.0f;

    while (eng.is_running()) {
        profiler.handle_input(eng.input());

        // ── Manual profiling (matches what step_game would do) ──
        profiler.begin(ProfilerZone_TotalFrame);

        profiler.begin(ProfilerZone_BeginFrame);
        eng.begin_frame();
        profiler.end(ProfilerZone_BeginFrame);

        f32 dt = eng.delta_time();

        // Move box1 back and forth for dynamic collision behavior
        e1->local_transform().position.x += move_dir * 0.5f * dt;
        if (e1->local_transform().position.x > 1.0f)  move_dir = -1.0f;
        if (e1->local_transform().position.x < -2.5f) move_dir =  1.0f;

        // ── Physics update (profiled) ───────────────────────────
        {
            ScopedProfileZone _p(profiler, zone_physics);
            cw.update(dt);
        }
        profiler.feed_physics_stats(cw.stats);

        // ── Render ──────────────────────────────────────────────
        glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        tr.begin_frame();

        profiler.begin(ProfilerZone_Render);
        // (no game objects to draw — this test is overlay-only)
        profiler.end(ProfilerZone_Render);

        // Screen labels
        tr.draw_text(font, "Profiler Overlay Test  —  F2: toggle profiler", 30, 30, 1.0f, 1,1,1,1);
        tr.draw_text(font, profiler.is_visible() ? "PROFILER VISIBLE" : "Press F2 to open profiler",
                     30, 52, 0.8f,
                     profiler.is_visible() ? 0.3f : 0.5f,
                     profiler.is_visible() ? 0.9f : 0.5f,
                     profiler.is_visible() ? 0.3f : 0.5f, 1);

        profiler.render(tr, font, fw, fh);

        tr.render(fw, fh);

        profiler.end(ProfilerZone_TotalFrame);
        profiler.end_frame();

        eng.end_frame();

        if (eng.input().is_key_pressed(Key::Escape)) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    tr.destroy();
    font.destroy();
    PINO_INFO("Exiting cleanly");
    return 0;
}
