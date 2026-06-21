#include "engine/engine.h"
#include "engine/physics/collision_world.h"
#include "engine/physics/debug_draw.h"
#include "engine/scene/entity.h"
#include "engine/scene/scene.h"
#include "engine/core/math_utils.h"
#include "engine/core/log.h"

#include <chrono>
#include <cstdio>
#include <vector>

using pino::u32;

// Benchmark: compare BruteForce vs UniformGrid broad-phase
// Spawns N colliders (half static ground, half dynamic) and runs M iterations.
static void run_bench(u32 num_colliders, u32 num_frames) {
    using Clock = std::chrono::high_resolution_clock;

    pino::Scene scene;
    pino::CollisionWorld cw;

    // Create colliders: num_colliders / 2 static (ground), rest dynamic (falling)
    u32 half = num_colliders / 2;
    if (half < 1) half = 1;

    pino::Random rng;
    rng.init(42);

    std::vector<pino::Entity*> entities;
    for (u32 i = 0; i < num_colliders; ++i) {
        char name[32];
        std::snprintf(name, sizeof(name), "obj_%u", i);
        auto* e = scene.root()->create_child(name);
        e->local_transform().position = {
            rng.range(-10.0f, 10.0f),
            rng.range(0.0f, 5.0f),
            rng.range(-10.0f, 10.0f)
        };
        e->local_transform().scale = {
            0.3f + rng.range(0.0f, 0.7f),
            0.3f + rng.range(0.0f, 0.7f),
            0.3f + rng.range(0.0f, 0.7f)
        };

        pino::ColliderComponent col;
        col.local_min = {-0.5f, -0.5f, -0.5f};
        col.local_max = {0.5f, 0.5f, 0.5f};
        col.is_static = (i < half);
        cw.register_collider(*e, col);
        entities.push_back(e);
    }

    // Warmup (both modes)
    cw.set_broad_phase_mode(pino::BroadPhaseMode::UniformGrid);
    for (u32 i = 0; i < 10; ++i) cw.update(1.0f / 60.0f);
    cw.set_broad_phase_mode(pino::BroadPhaseMode::BruteForce);
    for (u32 i = 0; i < 10; ++i) cw.update(1.0f / 60.0f);

    // ── Benchmark BruteForce ──
    cw.set_broad_phase_mode(pino::BroadPhaseMode::BruteForce);
    auto bf_start = Clock::now();
    for (u32 i = 0; i < num_frames; ++i) {
        // Move dynamic objects slightly to create varied overlaps
        for (u32 j = half; j < num_colliders; ++j) {
            entities[j]->local_transform().position.y -= 0.01f;
            entities[j]->local_transform().position.x +=
                rng.range(-0.02f, 0.02f);
            entities[j]->local_transform().position.z +=
                rng.range(-0.02f, 0.02f);
        }
        cw.update(1.0f / 60.0f);
    }
    auto bf_end = Clock::now();
    double bf_ms = std::chrono::duration<double, std::milli>(bf_end - bf_start).count();

    // ── Benchmark UniformGrid ──
    cw.set_broad_phase_mode(pino::BroadPhaseMode::UniformGrid);
    auto grid_start = Clock::now();
    for (u32 i = 0; i < num_frames; ++i) {
        for (u32 j = half; j < num_colliders; ++j) {
            entities[j]->local_transform().position.y -= 0.01f;
            entities[j]->local_transform().position.x +=
                rng.range(-0.02f, 0.02f);
            entities[j]->local_transform().position.z +=
                rng.range(-0.02f, 0.02f);
        }
        cw.update(1.0f / 60.0f);
    }
    auto grid_end = Clock::now();
    double grid_ms = std::chrono::duration<double, std::milli>(grid_end - grid_start).count();

    PINO_INFO("=== Collision Bench ===");
    PINO_INFO("Colliders: %u, Frames: %u", num_colliders, num_frames);
    PINO_INFO("  BruteForce:  %.3f ms (%u frames) = %.4f ms/frame",
              bf_ms, num_frames, bf_ms / num_frames);
    PINO_INFO("  UniformGrid: %.3f ms (%u frames) = %.4f ms/frame",
              grid_ms, num_frames, grid_ms / num_frames);
    PINO_INFO("  Speedup: %.2fx", bf_ms / grid_ms);
    PINO_INFO("");
}

int main(int, char**) {
    pino::Engine engine;
    pino::EngineConfig cfg;
    cfg.app_title     = "Collision Bench";
    cfg.window_width  = 128;
    cfg.window_height = 128;

    if (!engine.init(cfg)) return 1;

    // Run benchmarks at increasing collider counts
    u32 counts[] = {25, 50, 100, 200, 400};
    u32 frames   = 200;

    run_bench(10, 50); // light warmup
    PINO_INFO("--- Starting benchmarks ---\n");

    for (u32 n : counts) {
        run_bench(n, frames);
    }

    engine.shutdown();
    PINO_INFO("Benchmark complete.");
    return 0;
}
