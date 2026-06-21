#include "engine/engine.h"
#include "engine/renderer/font.h"
#include "engine/renderer/text_renderer.h"
#include "engine/renderer/render_stats.h"
#include "engine/renderer/debug_overlay.h"
#include "engine/ecs/ecs_scene.h"
#include "engine/ecs/ecs_inspector.h"
#include "engine/ecs/prefab_debug_viewer.h"
#include "engine/ecs/components.h"
#include "engine/debug/debug_console.h"
#include <cstdio>
#include <chrono>
#include <thread>

using namespace pino;

int main() {
    EngineConfig cfg;
    cfg.app_title     = "Debug Console Test";
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

    // ── Debug tools ──────────────────────────────────────────────
    DebugOverlay overlay;
    ECSInspector inspector;
    PrefabDebugViewer prefab_viewer;
    // ── ECS scene with some test entities ────────────────────────
    EcsScene scene;
    for (int i = 0; i < 5; ++i) {
        EntityId e = scene.create_entity();
        scene.scene_graph().attach(e, NullEntity);
        scene.scene_graph().set_position(e, {i * 2.0f, 0, 0});
        auto& rc = scene.add_component<RenderComponent>(e);
        rc.transparent = false;
        auto& pc = scene.add_component<PhysicsComponent>(e);
        pc.local_min = {-1, -1, -1};
        pc.local_max = { 1,  1,  1};
        pc.is_static = false;
    }

    inspector.set_scene(&scene);

    // ── Console ──────────────────────────────────────────────────
    DebugConsole console;
    Logger::set_callback(DebugConsole::log_capture, &console);
    console.register_builtins(&eng);

    // Wire up toggle features
    console.register_command("toggle", "toggle <overlay|inspector|prefab_viewer|physics_aabbs|physics_pairs|physics_grid|physics_vel>",
        [&](const std::vector<std::string>& args) {
            if (args.size() < 2) {
                PINO_WARN("Usage: toggle <feature>");
                return;
            }
            const auto& feat = args[1];
            if (feat == "overlay")       overlay.toggle();
            else if (feat == "inspector") inspector.toggle();
            else if (feat == "prefab_viewer") prefab_viewer.toggle();
            else if (feat == "physics_aabbs" ||
                     feat == "physics_pairs" ||
                     feat == "physics_grid"  ||
                     feat == "physics_vel") {
                PINO_INFO("Toggle %s (requires CollisionWorld setup)", feat.c_str());
            } else PINO_WARN("Unknown feature: %s", feat.c_str());
        }
    );

    console.register_command("list", "list entities",
        [&](const std::vector<std::string>&) {
            PINO_INFO("Entity count: %u", scene.entity_count());
            scene.registry().each([&](EntityId e) {
                PINO_INFO("  Entity #%u (gen:%u)", e.index, e.generation);
            });
        }
    );

    auto t0 = std::chrono::steady_clock::now();
    int frames = 0;
    float fps = 60.0f;

    while (eng.is_running()) {
        eng.begin_frame();

        // Handle input — console consumes F10 and blocks other input when visible
        console.handle_input(eng.input());
        if (!console.is_visible()) {
            overlay.handle_input(eng.input());
            inspector.handle_input(eng.input());
            prefab_viewer.handle_input(eng.input());
        }

        ++frames;
        auto t1 = std::chrono::steady_clock::now();
        f32 dt = std::chrono::duration<f32>(t1 - t0).count();
        if (dt >= 0.5f) { fps = frames / dt; frames = 0; t0 = t1; }

        // Feed stats to overlay
        f32 frame_ms = eng.delta_time() * 1000.0f;
        overlay.set_frame_stats(fps, frame_ms, 0.0f, 0.0f);
        auto& rs = RenderStats::instance();
        overlay.set_render_stats(rs.draw_calls, rs.triangles);
        overlay.set_entity_count(scene.entity_count());
        overlay.set_asset_counts(
            eng.assets().mesh_cache_size(),
            eng.assets().texture_cache_size(),
            eng.assets().shader_cache_size()
        );

        glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        tr.begin_frame();

        tr.draw_text(font, "Debug Console Test  —  F10: toggle console", 30, 30, 1.0f, 1,1,1,1);
        tr.draw_text(font, console.is_visible() ? "CONSOLE OPEN (F10/Esc to close)" : "Press F10 to open console",
                     30, 52, 0.8f,
                     console.is_visible() ? 0.3f : 0.5f,
                     console.is_visible() ? 0.9f : 0.5f,
                     console.is_visible() ? 0.3f : 0.5f, 1);

        // Render debug tools (console on top)
        overlay.render(tr, font, fw, fh);
        inspector.render(tr, font, fw, fh);
        prefab_viewer.render(tr, font, fw, fh);
        console.render(tr, font, fw, fh);

        tr.render(fw, fh);

        eng.end_frame();

        if (eng.input().is_key_pressed(Key::Escape) && !console.is_visible()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    Logger::set_callback(nullptr, nullptr);
    tr.destroy();
    font.destroy();
    PINO_INFO("Exiting cleanly");
    return 0;
}
