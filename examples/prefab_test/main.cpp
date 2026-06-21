// Prefab system integration test + benchmark.
// Console test — no window, no GPU.

#include "engine/ecs/prefab.h"
#include "engine/ecs/ecs_world.h"
#include <cstdio>
#include <chrono>
#include <cmath>

using namespace pino;

static int s_failures = 0;

#define TEST(name, expr) do { \
    if (!(expr)) { \
        printf("  FAIL: %s\n", name); \
        ++s_failures; \
    } \
} while(0)

int main() {
    // ── 1. Prefab construction ──────────────────────────────────
    printf("Test 1: prefab construction\n");
    {
        Prefab p;
        p.set_transform({1.0f, 2.0f, 3.0f}, {}, {1.0f, 1.0f, 1.0f});
        p.set_mesh("models/cube.obj");
        p.set_sound("sounds/step.wav");

        RenderComponent rc;
        rc.transparent = false;
        rc.has_bounds = true;
        rc.aabb_min = {-1,-1,-1};
        rc.aabb_max = {1,1,1};
        p.set_component(rc);

        PhysicsComponent pc;
        pc.is_static = true;
        pc.collision_layer = 2;
        p.set_component(pc);

        AudioComponent ac;
        ac.volume = 0.5f;
        ac.looping = true;
        p.set_component(ac);

        TEST("mesh path set", p.mesh_path() == "models/cube.obj");
        TEST("sound path set", p.sound_path() == "sounds/step.wav");
    }

    // ── 2. Prefab serialize/deserialize roundtrip ───────────────
    printf("Test 2: serialization roundtrip\n");
    {
        Prefab p1;
        p1.set_transform({10, 20, 30}, glm::quat(1,0,0,0), {2,2,2});
        p1.set_mesh("models/player.obj");

        RenderComponent rc;
        rc.transparent = true;
        rc.aabb_min = {-2,-2,-2};
        rc.aabb_max = {2,2,2};
        p1.set_component(rc);

        PhysicsComponent pc;
        pc.is_static = false;
        pc.collision_layer = 3;
        p1.set_component(pc);

        auto blob = p1.serialize();
        TEST("serialized size > 0", blob.size() > 0);

        Prefab p2;
        bool ok = p2.deserialize(blob);
        TEST("deserialize ok", ok);

        TEST("mesh path roundtrip", p2.mesh_path() == "models/player.obj");
        TEST("sound path empty roundtrip", p2.sound_path().empty());
    }

    // ── 3. Prefab instantiation into EcsScene ───────────────────
    printf("Test 3: prefab instantiation\n");
    {
        EcsScene scene;

        Prefab p;
        p.set_transform({5, 0, 0}, {}, {1,1,1});
        p.set_mesh("models/cube.obj");

        RenderComponent rc;
        rc.transparent = false;
        p.set_component(rc);

        PhysicsComponent pc;
        pc.is_static = true;
        p.set_component(pc);

        EntityId e = p.instantiate(scene);
        TEST("entity created", scene.alive(e));

        auto* rcp = scene.get_component<RenderComponent>(e);
        TEST("has render component", rcp != nullptr);
        TEST("render component not transparent", rcp && !rcp->transparent);

        auto* pcp = scene.get_component<PhysicsComponent>(e);
        TEST("has physics component", pcp != nullptr);
        TEST("physics is static", pcp && pcp->is_static);

        glm::vec3 pos = scene.scene_graph().world_position(e);
        TEST("position x = 5", std::abs(pos.x - 5.0f) < 0.001f);

        scene.destroy_entity(e);
    }

    // ── 4. Prefab instantiation with parent ─────────────────────
    printf("Test 4: prefab instantiation with parent\n");
    {
        EcsScene scene;

        EntityId parent = scene.create_entity();
        scene.scene_graph().attach(parent);
        scene.scene_graph().set_position(parent, {100, 0, 0});

        Prefab p;
        p.set_transform({0, 10, 0}, {}, {1,1,1});

        EntityId child = p.instantiate(scene, parent);
        TEST("child alive", scene.alive(child));

        glm::vec3 world_pos = scene.scene_graph().world_position(child);
        TEST("child world pos.y = 10", std::abs(world_pos.y - 10.0f) < 0.001f);
        TEST("child world pos.x = 100", std::abs(world_pos.x - 100.0f) < 0.001f);

        scene.destroy_entity(parent);
        scene.destroy_entity(child);
    }

    // ── 5. Prefab without transform ─────────────────────────────
    printf("Test 5: prefab without transform\n");
    {
        EcsScene scene;

        Prefab p;
        p.clear_transform();
        PhysicsComponent pc;
        p.set_component(pc);

        EntityId e = p.instantiate(scene);
        TEST("entity alive", scene.alive(e));
        TEST("has physics", scene.has_component<PhysicsComponent>(e));
        TEST("no transform in graph", !scene.scene_graph().has(e));

        scene.destroy_entity(e);
    }

    // ── 6. EcsWorld integration ─────────────────────────────────
    printf("Test 6: EcsWorld integration\n");
    {
        EcsWorld world;

        Prefab p;
        p.set_transform({0, 0, 0}, {}, {1,1,1});
        RenderComponent rc;
        p.set_component(rc);

        EntityId e = p.instantiate(world.scene());
        TEST("entity alive in world", world.alive(e));
        TEST("has render in world", world.scene().has_component<RenderComponent>(e));

        world.destroy_entity(e);
        world.update(0.016f);
        TEST("entity gone after flush", !world.alive(e));

        world.destroy();
    }

    // ── 7. Triple-component prefab ──────────────────────────────
    printf("Test 7: all three components on one prefab\n");
    {
        EcsScene scene;

        Prefab p;
        p.set_transform({}, {}, {});
        RenderComponent rc;
        p.set_component(rc);
        PhysicsComponent pc;
        p.set_component(pc);
        AudioComponent ac;
        p.set_component(ac);

        EntityId e = p.instantiate(scene);
        TEST("has render", scene.has_component<RenderComponent>(e));
        TEST("has physics", scene.has_component<PhysicsComponent>(e));
        TEST("has audio", scene.has_component<AudioComponent>(e));
        TEST("has transform", scene.scene_graph().has(e));

        scene.destroy_entity(e);
    }

    // ── 8. Replace component ────────────────────────────────────
    printf("Test 8: replace component in prefab\n");
    {
        Prefab p;
        {
            PhysicsComponent pc;
            pc.is_static = false;
            pc.enabled = false;
            pc.collision_layer = 1;
            pc.collision_mask = 1;
            p.set_component(pc);
        }
        {
            PhysicsComponent pc;
            pc.is_static = true;
            pc.enabled = true;
            pc.collision_layer = 5;
            pc.collision_mask = 10;
            p.set_component(pc);
        }

        EcsScene scene;
        EntityId e = p.instantiate(scene);
        auto* pcp = scene.get_component<PhysicsComponent>(e);
        TEST("is_static from second write", pcp && pcp->is_static);
        TEST("layer from second write", pcp && pcp->collision_layer == 5);
        scene.destroy_entity(e);
    }

    // ── 9. Save/Load via filesystem ─────────────────────────────
    printf("Test 9: save/load roundtrip\n");
    {
        Prefab p1;
        p1.set_transform({1,2,3}, glm::quat(1,0,0,0), {1,1,1});
        p1.set_mesh("models/test.obj");
        p1.set_sound("sounds/test.wav");
        RenderComponent rc;
        p1.set_component(rc);
        PhysicsComponent pc;
        p1.set_component(pc);

        bool saved = p1.save("test_prefab.bin");
        TEST("saved to file", saved);

        Prefab p2;
        bool loaded = p2.load("test_prefab.bin");
        TEST("loaded from file", loaded);

        TEST("mesh path preserved", p2.mesh_path() == "models/test.obj");
        TEST("sound path preserved", p2.sound_path() == "sounds/test.wav");

        std::remove("test_prefab.bin");
    }

    // ── 10. Benchmark: instantiate 1000 ─────────────────────────
    printf("Test 10: benchmark prefab instantiation (1000 entities)\n");
    {
        EcsScene scene;

        Prefab p;
        p.set_transform({}, {}, {});
        RenderComponent rc;
        p.set_component(rc);
        PhysicsComponent pc;
        p.set_component(pc);
        AudioComponent ac;
        p.set_component(ac);

        const int N = 1000;
        auto start = std::chrono::high_resolution_clock::now();

        std::vector<EntityId> entities;
        entities.reserve(N);
        for (int i = 0; i < N; ++i) {
            EntityId e = p.instantiate(scene);
            entities.push_back(e);
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        float avg_us = static_cast<float>(us) / N;

        printf("  Instantiated %d entities in %lld us (avg %.2f us/entity)\n", N, us, avg_us);
        printf("  Memory: %u entities alive\n", scene.entity_count());

        for (auto e : entities) {
            scene.destroy_entity(e);
        }
        TEST("1000 entities created/destroyed", scene.entity_count() == 0);
    }

    // ── 11. Benchmark: serialization size ───────────────────────
    printf("Test 11: benchmark serialization size\n");
    {
        Prefab p;
        p.set_transform({1,2,3}, {0,0,0,1}, {1,1,1});
        p.set_mesh("models/level/ground.obj");
        p.set_sound("sounds/ambient/wind.wav");
        RenderComponent rc;
        p.set_component(rc);
        PhysicsComponent pc;
        p.set_component(pc);

        auto blob = p.serialize();
        printf("  Prefab with 2 components: %zu bytes\n", blob.size());
        TEST("serialized size reasonable", blob.size() < 500);
    }

    // ── 12. AudioComponent prefab ───────────────────────────────
    printf("Test 12: prefab with audio component\n");
    {
        EcsScene scene;

        Prefab p;
        p.set_sound("sounds/explosion.wav");
        AudioComponent ac;
        ac.volume = 0.8f;
        ac.looping = false;
        ac.attenuation_model = 1;
        p.set_component(ac);

        EntityId e = p.instantiate(scene);
        auto* acp = scene.get_component<AudioComponent>(e);
        TEST("has audio", acp != nullptr);
        TEST("sound path set from prefab", acp && acp->sound_path == "sounds/explosion.wav");
        TEST("volume 0.8", acp && std::abs(acp->volume - 0.8f) < 0.001f);
        TEST("looping false", acp && !acp->looping);
        scene.destroy_entity(e);
    }

    // ── Summary ─────────────────────────────────────────────────
    printf("\n");
    if (s_failures == 0) {
        printf("All prefab tests passed!\n");
        return 0;
    } else {
        printf("%d test(s) FAILED!\n", s_failures);
        return 1;
    }
}
