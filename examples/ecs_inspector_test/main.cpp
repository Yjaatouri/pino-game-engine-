#include "engine/engine.h"
#include "engine/renderer/font.h"
#include "engine/renderer/text_renderer.h"
#include "engine/renderer/render_stats.h"
#include "engine/ecs/ecs_scene.h"
#include "engine/ecs/ecs_inspector.h"
#include "engine/ecs/components.h"
#include <cstdio>
#include <chrono>
#include <thread>

using namespace pino;

int main() {
    EngineConfig cfg;
    cfg.app_title     = "ECS Inspector Test";
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

    // ── Setup ECS scene with test entities ──────────────────────
    EcsScene scene;

    // Entity 0: full featured
    EntityId e0 = scene.create_entity();
    scene.scene_graph().attach(e0, NullEntity);
    scene.scene_graph().set_position(e0, {1.0f, 2.0f, 3.0f});
    scene.scene_graph().set_scale(e0, {2.0f, 2.0f, 2.0f});
    auto& rc0 = scene.add_component<RenderComponent>(e0);
    rc0.transparent = false;
    auto& pc0 = scene.add_component<PhysicsComponent>(e0);
    pc0.local_min = {-1.0f, -1.0f, -1.0f};
    pc0.local_max = {1.0f, 1.0f, 1.0f};
    pc0.is_static = true;
    auto& ac0 = scene.add_component<AudioComponent>(e0);
    ac0.sound_path = "test.ogg";
    ac0.volume = 0.75f;
    ac0.looping = true;

    // Entity 1: transform + render only
    EntityId e1 = scene.create_entity();
    scene.scene_graph().attach(e1, NullEntity);
    scene.scene_graph().set_position(e1, {10.0f, 0.0f, -5.0f});
    scene.add_component<RenderComponent>(e1);

    // Entity 2: transform + physics only
    EntityId e2 = scene.create_entity();
    scene.scene_graph().attach(e2, NullEntity);
    scene.add_component<PhysicsComponent>(e2);

    // Entity 3: transform only
    EntityId e3 = scene.create_entity();
    scene.scene_graph().attach(e3, NullEntity);
    scene.scene_graph().set_position(e3, {100.0f, 0.0f, 0.0f});

    // Entity 4: full featured, offset transform
    EntityId e4 = scene.create_entity();
    scene.scene_graph().attach(e4, NullEntity);
    scene.scene_graph().set_position(e4, {-5.0f, 3.0f, 2.0f});
    scene.add_component<RenderComponent>(e4);
    scene.add_component<PhysicsComponent>(e4);

    // Entity 5: child of e0
    EntityId e5 = scene.create_entity();
    scene.scene_graph().attach(e5, e0);
    scene.scene_graph().set_position(e5, {0.5f, 0.0f, 0.0f});

    PINO_INFO("Created %u entities", scene.entity_count());

    // ── Setup inspector ─────────────────────────────────────────
    ECSInspector inspector;
    inspector.set_scene(&scene);

    auto t0 = std::chrono::steady_clock::now();
    int frames = 0;
    float fps = 60.0f;
    float fps_timer = 0;

    PINO_INFO("Press F4 to toggle ECS Inspector");

    while (eng.is_running()) {
        eng.begin_frame();

        inspector.handle_input(eng.input());

        ++frames;
        auto t1 = std::chrono::steady_clock::now();
        f32 dt = std::chrono::duration<f32>(t1 - t0).count();
        fps_timer += dt;
        if (fps_timer >= 0.5f) { fps = frames / fps_timer; frames = 0; fps_timer = 0; }
        t0 = t1;

        glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        tr.begin_frame();

        // Draw scene info
        char buf[128];
        std::snprintf(buf, sizeof(buf), "ECS Inspector Test — %u entities — FPS: %.1f",
                      scene.entity_count(), fps);
        tr.draw_text(font, buf, 30, 30, 1.0f, 1, 1, 1, 1);
        tr.draw_text(font, inspector.is_visible() ? "INSPECTOR VISIBLE (F4 to hide)" : "Press F4 to open inspector",
                     30, 52, 0.8f,
                     inspector.is_visible() ? 0.3f : 0.5f,
                     inspector.is_visible() ? 0.9f : 0.5f,
                     inspector.is_visible() ? 0.3f : 0.5f, 1);

        // Render the inspector
        inspector.render(tr, font, fw, fh);

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
