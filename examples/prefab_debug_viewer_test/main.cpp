#include "engine/engine.h"
#include "engine/renderer/font.h"
#include "engine/renderer/text_renderer.h"
#include "engine/ecs/prefab.h"
#include "engine/ecs/prefab_debug_viewer.h"
#include "engine/ecs/components.h"
#include <cstdio>
#include <chrono>
#include <thread>

using namespace pino;

int main() {
    EngineConfig cfg;
    cfg.app_title     = "Prefab Debug Viewer Test";
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

    // ── Build test prefabs ──────────────────────────────────────
    Prefab valid_prefab;
    valid_prefab.set_transform({1.0f, 2.0f, 3.0f}, {}, {2.0f, 2.0f, 2.0f});
    valid_prefab.set_component(RenderComponent{});
    valid_prefab.set_mesh("models/test.obj");
    PhysicsComponent pc;
    pc.local_min = {-1.0f, -1.0f, -1.0f};
    pc.local_max = {1.0f, 1.0f, 1.0f};
    pc.is_static = false;
    pc.enabled = true;
    valid_prefab.set_component(pc);
    AudioComponent ac;
    ac.sound_path = "sounds/test.ogg";
    ac.volume = 0.8f;
    ac.looping = true;
    valid_prefab.set_component(ac);

    // Corrupt prefab (bad physics AABB: min > max)
    Prefab corrupt_prefab;
    corrupt_prefab.set_transform({0,0,0}, {}, {1,1,1});
    PhysicsComponent bad_pc;
    bad_pc.local_min = {5.0f, 5.0f, 5.0f};
    bad_pc.local_max = {-5.0f, -5.0f, -5.0f};
    bad_pc.collision_layer = 0;
    corrupt_prefab.set_component(bad_pc);
    corrupt_prefab.set_component(RenderComponent{});
    // No mesh path set → warning

    // Minimal prefab (transform only)
    Prefab minimal_prefab;
    minimal_prefab.set_transform({10.0f, 0.0f, 0.0f}, {}, {1,1,1});

    PINO_INFO("Created 3 test prefabs");

    // ── Setup viewer ────────────────────────────────────────────
    PrefabDebugViewer viewer;
    viewer.load_prefab(valid_prefab);

    auto t0 = std::chrono::steady_clock::now();
    int frames = 0;
    float fps = 60.0f;
    float fps_timer = 0;
    int current_prefab = 0;

    PINO_INFO("Press F9 to toggle Prefab Debug Viewer");
    PINO_INFO("Press 1/2/3 to switch between test prefabs");

    while (eng.is_running()) {
        eng.begin_frame();

        viewer.handle_input(eng.input());

        // Switch prefabs with 1/2/3 keys
        if (eng.input().is_key_just_pressed(Key::_1)) {
            viewer.load_prefab(valid_prefab); current_prefab = 0;
        }
        if (eng.input().is_key_just_pressed(Key::_2)) {
            viewer.load_prefab(corrupt_prefab); current_prefab = 1;
        }
        if (eng.input().is_key_just_pressed(Key::_3)) {
            viewer.load_prefab(minimal_prefab); current_prefab = 2;
        }

        ++frames;
        auto t1 = std::chrono::steady_clock::now();
        f32 dt = std::chrono::duration<f32>(t1 - t0).count();
        fps_timer += dt;
        if (fps_timer >= 0.5f) { fps = frames / fps_timer; frames = 0; fps_timer = 0; }
        t0 = t1;

        glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        tr.begin_frame();

        char buf[128];
        std::snprintf(buf, sizeof(buf),
                      "Prefab Debug Test — FPS: %.1f  [1: valid 2: corrupt 3: minimal]",
                      fps);
        tr.draw_text(font, buf, 30, 30, 1.0f, 1, 1, 1, 1);

        const char* names[] = {"VALID prefab", "CORRUPT prefab", "MINIMAL prefab"};
        tr.draw_text(font, names[current_prefab], 30, 52, 0.9f,
                     current_prefab == 0 ? 0.3f : (current_prefab == 1 ? 1.0f : 0.5f),
                     current_prefab == 0 ? 0.9f : (current_prefab == 1 ? 0.3f : 0.5f),
                     current_prefab == 0 ? 0.3f : (current_prefab == 1 ? 0.3f : 0.5f), 1);

        tr.draw_text(font, "Press F9 to toggle viewer", 30, 72, 0.7f, 0.6f, 0.6f, 0.6f, 1);

        viewer.render(tr, font, fw, fh);

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
