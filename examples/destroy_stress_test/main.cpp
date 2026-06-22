// Stress test: high-churn entity spawn/destroy cycle.
// Spawns 100 entities per frame (with physics, render, audio components)
// and immediately queues them for destruction.  Runs for 1 simulated second
// (60 frames) and verifies no performance spikes and no leaks.
// Console test — no window, no GPU.

#include "engine/ecs/ecs_scene.h"
#include <cstdio>
#include <chrono>
#include <vector>

static int s_failures = 0;

#define TEST(name, expr) do { \
    if (!(expr)) { \
        printf("  FAIL: %s\n", name); \
        ++s_failures; \
    } \
} while(0)

int main() {
    printf("=== Destroy Stress Test ===\n\n");

    pino::EcsScene scene;
    auto& sg = scene.scene_graph();

    const int FRAMES        = 60;
    const int SPAWN_PER_FRAME = 100;
    const int TOTAL_SPAWNED = FRAMES * SPAWN_PER_FRAME;

    // Helpers
    auto spawn_one = [&](int idx) -> pino::EntityId {
        pino::EntityId e = scene.create_entity();
        sg.attach(e);
        sg.set_position(e, {static_cast<float>(idx) * 0.1f, 0.0f, 0.0f});
        sg.set_scale(e, {1.0f, 1.0f, 1.0f});

        scene.add_component<pino::RenderComponent>(e);
        auto& pc = scene.add_component<pino::PhysicsComponent>(e);
        pc.local_min = {-0.5f, -0.5f, -0.5f};
        pc.local_max = { 0.5f,  0.5f,  0.5f};
        pc.is_static = false;
        pc.collision_layer = 1;
        pc.collision_mask  = 1;
        scene.add_component<pino::AudioComponent>(e);
        return e;
    };

    // Track timing
    std::vector<long long> frame_flush_us;
    frame_flush_us.reserve(FRAMES);

    long long total_spawn_us = 0;
    long long total_flush_us = 0;

    printf("Running %d frames, spawning %d entities per frame...\n", FRAMES, SPAWN_PER_FRAME);

    std::vector<pino::EntityId> batch;
    batch.reserve(SPAWN_PER_FRAME);

    for (int frame = 0; frame < FRAMES; ++frame) {
        // ── Spawn ───────────────────────────────────────────
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < SPAWN_PER_FRAME; ++i) {
            batch.push_back(spawn_one(frame * SPAWN_PER_FRAME + i));
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        total_spawn_us += std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

        // ── Destroy (deferred, should be cheap) ─────────────
        auto t2 = std::chrono::high_resolution_clock::now();
        for (auto e : batch) {
            scene.destroy_entity(e);
        }
        batch.clear();
        auto t3 = std::chrono::high_resolution_clock::now();
        long long enqueue_us = std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count();

        // ── Flush (actual destruction work) ─────────────────
        auto t4 = std::chrono::high_resolution_clock::now();
        scene.flush_destroyed_entities();
        auto t5 = std::chrono::high_resolution_clock::now();
        long long flush_us = std::chrono::duration_cast<std::chrono::microseconds>(t5 - t4).count();
        frame_flush_us.push_back(flush_us);
        total_flush_us += flush_us;

        // Verify frame-level invariants
        TEST("no leaks after flush", scene.entity_count() == 0);
        TEST("zero pending destroys", scene.pending_destroy_count() == 0);

        if (frame % 10 == 0) {
            printf("  Frame %3d: enqueue %lld us, flush %lld us\n", frame, enqueue_us, flush_us);
        }
    }

    // ── Stats ──────────────────────────────────────────────────
    long long avg_flush = total_flush_us / FRAMES;
    long long max_flush = 0;
    for (auto v : frame_flush_us) if (v > max_flush) max_flush = v;
    long long min_flush = frame_flush_us[0];
    for (auto v : frame_flush_us) if (v < min_flush) min_flush = v;

    printf("\n");
    printf("  Total spawned:    %d\n", TOTAL_SPAWNED);
    printf("  Total flush time: %lld us\n", total_flush_us);
    printf("  Avg flush/frame:  %lld us\n", avg_flush);
    printf("  Min flush/frame:  %lld us\n", min_flush);
    printf("  Max flush/frame:  %lld us\n", max_flush);

    // No single flush should exceed 3x the average (no spikes).
    // The threshold is generous to avoid flaky CI failures while
    // still catching catastrophic O(N) regressions.
    long long spike_threshold = avg_flush * 3 + 500;  // +500 us tolerance
    TEST("no performance spikes", max_flush <= spike_threshold);
    if (max_flush > spike_threshold) {
        printf("  WARNING: spike detected: max %lld us > threshold %lld us\n",
               max_flush, spike_threshold);
    }

    // ── Final state ────────────────────────────────────────────
    TEST("final count 0", scene.entity_count() == 0);
    TEST("final pending 0", scene.pending_destroy_count() == 0);
    TEST("final graph 0", scene.scene_graph().count() == 0);
    TEST("final renders 0", scene.render_components().count() == 0);
    TEST("final physics 0", scene.physics_components().count() == 0);
    TEST("final audio 0", scene.audio_components().count() == 0);

    // ── Results ────────────────────────────────────────────────
    printf("\n=== Results: %s ===\n", s_failures == 0 ? "ALL PASSED" : "SOME FAILED");
    printf("Failures: %d\n", s_failures);
    return s_failures;
}
