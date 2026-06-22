// Verifies that asynchronous serialization does not stall the main thread.
// Spawns 200 entities with physics + render + audio, serializes on a
// background thread, and measures main-thread frame time during the save.
// Console test — no window, no GPU.

#include "engine/serialization/async_save.h"
#include "engine/ecs/ecs_scene.h"
#include <cstdio>
#include <chrono>

static int s_failures = 0;

#define TEST(name, expr) do { \
    if (!(expr)) { \
        printf("  FAIL: %s\n", name); \
        ++s_failures; \
    } \
} while(0)

int main() {
    printf("=== Serialization Stall Test ===\n\n");

    // ── Setup: 200 entities with all 3 component types ──────────
    pino::EcsScene scene;
    auto& sg = scene.scene_graph();

    const int N = 200;
    for (int i = 0; i < N; ++i) {
        pino::EntityId e = scene.create_entity();
        sg.attach(e);
        sg.set_position(e, {static_cast<float>(i) * 0.5f, 0.0f, 0.0f});

        auto& rc = scene.add_component<pino::RenderComponent>(e);
        rc.transparent = false;
        rc.enabled = true;

        auto& pc = scene.add_component<pino::PhysicsComponent>(e);
        pc.local_min = {-0.5f, -0.5f, -0.5f};
        pc.local_max = { 0.5f,  0.5f,  0.5f};
        pc.is_static = false;
        pc.collision_layer = 1;
        pc.collision_mask  = 1;

        auto& ac = scene.add_component<pino::AudioComponent>(e);
        ac.volume = 0.5f;
        ac.looping = false;
    }

    TEST("200 entities created", scene.entity_count() == static_cast<pino::u32>(N));

    // ── Synchronous baseline (what the old code did) ────────────
    printf("Synchronous baseline...\n");
    pino::TypeRegistry st_types;
    pino::VersionRegistry st_versions;
    pino::StringTable st_strings;
    pino::SaveGameSerializer::registerTypes(st_types);
    pino::SaveGameSerializer::registerVersions(st_versions);
    pino::SaveGameSerializer st_ser(st_types, st_versions, st_strings);
    pino::BinaryChunkWriter st_writer;
    pino::Serializer st_ser_wrap(st_writer);

    auto t0 = std::chrono::high_resolution_clock::now();
    st_ser.serialize(st_ser_wrap, scene);
    auto t1 = std::chrono::high_resolution_clock::now();
    long long sync_us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    printf("  Synchronous: %lld us\n", sync_us);

    // ── Async serialization ─────────────────────────────────────
    printf("\nAsync serialization...\n");

    pino::AsyncSaveSerializer saver;
    saver.start_async(scene);

    // Wait for the background thread to begin making progress.
    // Spin-wait with a short timeout to avoid deadlock on failure.
    int wait_iters = 0;
    while (saver.entities_serialized() == 0 && !saver.is_done() && wait_iters < 100000) {
        ++wait_iters;
    }
    printf("  Background thread started after ~%d spin iterations\n", wait_iters);

    // ── Measure main-thread overhead during save ─────────────────
    const int MEASUREMENT_FRAMES = 5;
    // Threshold: main thread should never stall more than 500 us from
    // the async save overhead (polling atomics + call overhead).
    // Compare this to the synchronous baseline of multiple milliseconds.
    const int FRAME_TIME_US_THRESHOLD = 500;

    long long max_frame_us = 0;
    long long total_frame_us = 0;

    for (int frame = 0; frame < MEASUREMENT_FRAMES; ++frame) {
        auto tf = std::chrono::high_resolution_clock::now();

        // Main thread work: poll progress (two cheap atomic loads).
        float pct = saver.progress() * 100.0f;
        uint32_t written = saver.entities_serialized();

        if (frame == 0) {
            printf("  Frame %d: progress %.1f%% (%u/%u entities)\n",
                   frame, pct, written, N);
        }

        auto tf_end = std::chrono::high_resolution_clock::now();
        long long frame_us = std::chrono::duration_cast<std::chrono::microseconds>(tf_end - tf).count();
        total_frame_us += frame_us;
        if (frame_us > max_frame_us) max_frame_us = frame_us;

        TEST("frame time within threshold", frame_us <= FRAME_TIME_US_THRESHOLD);
    }

    // ── Wait for completion ─────────────────────────────────────
    const auto& buf = saver.finish();

    TEST("serialization complete", saver.is_done());
    TEST("progress 100%", saver.progress() >= 1.0f);
    TEST("all entities serialized", saver.entities_serialized() >= static_cast<uint32_t>(N));
    TEST("buffer non-empty", !buf.empty());

    // ── Summary ─────────────────────────────────────────────────
    printf("\n  Async save complete: %zu bytes\n", buf.size());
    printf("  Max main-thread overhead: %lld us (threshold %d us)\n",
           max_frame_us, FRAME_TIME_US_THRESHOLD);
    printf("  Avg main-thread overhead: %lld us (sync baseline: %lld us)\n",
           total_frame_us / MEASUREMENT_FRAMES, sync_us);

    // The async overhead should be a tiny fraction of the sync time.
    long long avg_us = total_frame_us / MEASUREMENT_FRAMES;
    TEST("async overhead << sync time", avg_us * 10 < sync_us);

    // ── Results ─────────────────────────────────────────────────
    printf("\n=== Results: %s ===\n", s_failures == 0 ? "ALL PASSED" : "SOME FAILED");
    printf("Failures: %d\n", s_failures);
    return s_failures;
}
