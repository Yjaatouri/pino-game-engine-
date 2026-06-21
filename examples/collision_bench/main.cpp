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

struct BenchResult {
    const char* name;
    double ms;
    double ms_per_frame;
    double speedup; // = bf_per_frame / this_per_frame
};

enum class Distribution { Scattered, Clustered };

static BenchResult bench_mode(pino::CollisionWorld& cw,
                              pino::BroadPhaseMode mode,
                              const char* name,
                              u32 half, u32 num_colliders,
                              u32 num_frames,
                              std::vector<pino::Entity*>& entities,
                              pino::Random& rng,
                              double bf_total_ms) {
    using Clock = std::chrono::high_resolution_clock;

    cw.set_broad_phase_mode(mode);
    auto start = Clock::now();
    for (u32 i = 0; i < num_frames; ++i) {
        for (u32 j = half; j < num_colliders; ++j) {
            entities[j]->local_transform().position.y -= 0.01f;
            entities[j]->local_transform().position.x += rng.range(-0.02f, 0.02f);
            entities[j]->local_transform().position.z += rng.range(-0.02f, 0.02f);
        }
        cw.update(1.0f / 60.0f);
    }
    auto end = Clock::now();
    double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
    double my_per_frame = total_ms / num_frames;
    double speedup = (mode == pino::BroadPhaseMode::BruteForce) ? 1.0 : bf_total_ms / my_per_frame;
    return {name, total_ms, my_per_frame, speedup};
}

static void create_entities(pino::Scene& scene, pino::CollisionWorld& cw,
                            u32 num_colliders, u32 half,
                            Distribution dist,
                            std::vector<pino::Entity*>& entities) {
    pino::Random rng;
    rng.init(42);

    for (u32 i = 0; i < num_colliders; ++i) {
        char name[32];
        std::snprintf(name, sizeof(name), "obj_%u", i);
        auto* e = scene.root()->create_child(name);

        if (dist == Distribution::Clustered) {
            // Cluster around origin: 80% in a small volume, 20% scattered
            if (i < num_colliders * 8 / 10) {
                e->local_transform().position = {
                    rng.range(-2.0f, 2.0f),
                    rng.range(0.0f, 2.0f),
                    rng.range(-2.0f, 2.0f)
                };
            } else {
                e->local_transform().position = {
                    rng.range(-15.0f, 15.0f),
                    rng.range(0.0f, 5.0f),
                    rng.range(-15.0f, 15.0f)
                };
            }
        } else {
            // Scattered randomly
            e->local_transform().position = {
                rng.range(-10.0f, 10.0f),
                rng.range(0.0f, 5.0f),
                rng.range(-10.0f, 10.0f)
            };
        }

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
}

static void reset_positions(pino::Random& rng, u32 num_colliders, u32 half,
                            Distribution dist,
                            std::vector<pino::Entity*>& entities) {
    rng.init(42);
    for (u32 i = 0; i < num_colliders; ++i) {
        if (dist == Distribution::Clustered) {
            if (i < num_colliders * 8 / 10) {
                entities[i]->local_transform().position = {
                    rng.range(-2.0f, 2.0f),
                    rng.range(0.0f, 2.0f),
                    rng.range(-2.0f, 2.0f)
                };
            } else {
                entities[i]->local_transform().position = {
                    rng.range(-15.0f, 15.0f),
                    rng.range(0.0f, 5.0f),
                    rng.range(-15.0f, 15.0f)
                };
            }
        } else {
            entities[i]->local_transform().position = {
                rng.range(-10.0f, 10.0f),
                rng.range(0.0f, 5.0f),
                rng.range(-10.0f, 10.0f)
            };
        }
    }
}

static void run_bench(u32 num_colliders, u32 num_frames,
                      Distribution dist, const char* dist_name) {
    pino::Scene scene;
    pino::CollisionWorld cw;

    u32 half = num_colliders / 2;
    if (half < 1) half = 1;

    std::vector<pino::Entity*> entities;
    create_entities(scene, cw, num_colliders, half, dist, entities);

    // Auto-size grid
    cw.auto_size_grid(2.0f);

    // Warmup
    cw.set_broad_phase_mode(pino::BroadPhaseMode::UniformGrid);
    for (u32 i = 0; i < 10; ++i) cw.update(1.0f / 60.0f);
    cw.set_broad_phase_mode(pino::BroadPhaseMode::BruteForce);
    for (u32 i = 0; i < 10; ++i) cw.update(1.0f / 60.0f);

    pino::Random rng;

    // Decide frames per mode: reduce brute-force frames at high counts
    u32 bf_frames = num_frames;
    if (num_colliders >= 1000) bf_frames = (std::min)(num_frames, 50u);
    if (num_colliders >= 5000) bf_frames = 20u;

    // Brute-force
    reset_positions(rng, num_colliders, half, dist, entities);
    rng.init(42);
    auto bf = bench_mode(cw, pino::BroadPhaseMode::BruteForce, "BruteForce",
                         half, num_colliders, bf_frames, entities, rng, 0.0);

    // Other modes use full frames
    reset_positions(rng, num_colliders, half, dist, entities);
    rng.init(42);
    auto ug = bench_mode(cw, pino::BroadPhaseMode::UniformGrid, "UniformGrid",
                         half, num_colliders, num_frames, entities, rng, bf.ms_per_frame);

    reset_positions(rng, num_colliders, half, dist, entities);
    rng.init(42);
    auto lg = bench_mode(cw, pino::BroadPhaseMode::LooseGrid, "LooseGrid",
                         half, num_colliders, num_frames, entities, rng, bf.ms_per_frame);

    reset_positions(rng, num_colliders, half, dist, entities);
    rng.init(42);
    auto sap = bench_mode(cw, pino::BroadPhaseMode::SweepAndPrune, "SweepAndPrune",
                          half, num_colliders, num_frames, entities, rng, bf.ms_per_frame);

    auto& s = cw.stats;

    PINO_INFO("=== Collision Bench [%u colliders, %s] ===", num_colliders, dist_name);
    PINO_INFO("  %-15s %8.3f ms (%3u fr)  %7.4f ms/fr",
              bf.name, bf.ms, bf_frames, bf.ms_per_frame);
    PINO_INFO("  %-15s %8.3f ms (%3u fr)  %7.4f ms/fr  %6.2fx",
              ug.name, ug.ms, num_frames, ug.ms_per_frame, ug.speedup);
    PINO_INFO("  %-15s %8.3f ms (%3u fr)  %7.4f ms/fr  %6.2fx",
              lg.name, lg.ms, num_frames, lg.ms_per_frame, lg.speedup);
    PINO_INFO("  %-15s %8.3f ms (%3u fr)  %7.4f ms/fr  %6.2fx",
              sap.name, sap.ms, num_frames, sap.ms_per_frame, sap.speedup);
    PINO_INFO("  Last-frame: AABB %.1f  Broad %.1f  Narrow %.1f  Total %.1f us",
              s.aabb_update_us, s.broad_phase_us, s.narrow_phase_us, s.total_us);
    PINO_INFO("  Candidates: %llu  Overlaps: %llu",
              (unsigned long long)s.candidate_pairs_unique,
              (unsigned long long)s.actual_overlaps);
    PINO_INFO("");
}

int main(int, char**) {
    pino::Engine engine;
    pino::EngineConfig cfg;
    cfg.app_title     = "Collision Bench";
    cfg.window_width  = 128;
    cfg.window_height = 128;

    if (!engine.init(cfg)) return 1;

    u32 frames = 200;

    run_bench(10, 50, Distribution::Scattered, "scattered"); // warmup

    PINO_INFO("========== SCATTERED DISTRIBUTION ==========\n");

    run_bench(100,  frames, Distribution::Scattered, "scattered");
    run_bench(500,  frames, Distribution::Scattered, "scattered");
    run_bench(1000, frames, Distribution::Scattered, "scattered");
    run_bench(2500, frames, Distribution::Scattered, "scattered");
    run_bench(5000, frames, Distribution::Scattered, "scattered");

    PINO_INFO("\n========== CLUSTERED DISTRIBUTION ==========\n");

    run_bench(100,  frames, Distribution::Clustered, "clustered");
    run_bench(500,  frames, Distribution::Clustered, "clustered");
    run_bench(1000, frames, Distribution::Clustered, "clustered");

    engine.shutdown();
    PINO_INFO("\nBenchmark complete.");
    return 0;
}
