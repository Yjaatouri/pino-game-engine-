// Component system lifecycle + EcsScene integration tests.
// Console test — no window, no GPU.

#include "engine/ecs/ecs_scene.h"
#include "engine/ecs/ecs_physics_adapter.h"
#include "engine/ecs/components.h"
#include "engine/ecs/component_pool.h"
#include "engine/core/transform.h"
#include <cstdio>

using namespace pino;

static int s_failures = 0;

#define TEST(name, expr) do { \
    if (!(expr)) { \
        printf("  FAIL: %s\n", name); \
        ++s_failures; \
    } \
} while(0)

int main() {
    // ── 1. ComponentPool basics ─────────────────────────────────
    printf("Test 1: ComponentPool add / get / has / remove\n");
    {
        ComponentPool<RenderComponent> pool;
        EntityId e0{0, 0};

        TEST("no component initially", !pool.has(e0));
        TEST("get returns null", pool.get(e0) == nullptr);

        auto& rc = pool.add(e0);
        rc.mesh = reinterpret_cast<const Mesh*>(0x1);
        TEST("has after add", pool.has(e0));
        TEST("get returns non-null", pool.get(e0) != nullptr);
        TEST("mesh preserved", pool.get(e0)->mesh == reinterpret_cast<const Mesh*>(0x1));

        pool.remove(e0);
        TEST("no component after remove", !pool.has(e0));
    }

    // ── 2. Stale ID safety ──────────────────────────────────────
    printf("Test 2: stale ID detection\n");
    {
        EntityRegistry reg;
        ComponentPool<RenderComponent> pool;

        // Without registry: generation check protects against slot reuse.
        EntityId e1 = reg.create();
        pool.add(e1);
        reg.destroy(e1);
        EntityId e2 = reg.create();  // reuses e1's slot with bumped gen
        TEST("new entity gets different gen", e2.index == e1.index && e2.generation != e1.generation);
        TEST("new e2 not in pool yet", !pool.has(e2));
        pool.add(e2);
        TEST("new e2 in pool", pool.has(e2));
        TEST("old e1 not found after slot taken", !pool.has(e1));

        // With registry link: also catches entity destroyed without replacement.
        ComponentPool<RenderComponent> pool2;
        pool2.set_registry(&reg);
        EntityId e3 = reg.create();
        pool2.add(e3);
        TEST("e3 in pool (registry)", pool2.has(e3));
        reg.destroy(e3);
        TEST("e3 stale via registry check", !pool2.has(e3));
    }

    // ── 3. Pool iteration ───────────────────────────────────────
    printf("Test 3: ComponentPool each()\n");
    {
        EntityRegistry reg;
        ComponentPool<RenderComponent> pool;

        EntityId ids[3];
        for (int i = 0; i < 3; ++i) {
            ids[i] = reg.create();
            pool.add(ids[i]);
        }

        int count = 0;
        pool.each([&](EntityId e, RenderComponent&) {
            bool found = false;
            for (auto id : ids)
                if (id == e) { found = true; break; }
            TEST("iterated entity exists", found);
            ++count;
        });
        TEST("count = 3", count == 3);
    }

    // ── 4. Pool clear ───────────────────────────────────────────
    printf("Test 4: ComponentPool clear\n");
    {
        EntityRegistry reg;
        ComponentPool<RenderComponent> pool;
        for (int i = 0; i < 10; ++i)
            pool.add(reg.create());
        TEST("count = 10", pool.count() == 10);
        pool.clear();
        TEST("count = 0 after clear", pool.count() == 0);
    }

    // ── 5. EcsScene create / destroy entity ─────────────────────
    printf("Test 5: EcsScene entity lifecycle\n");
    {
        EcsScene scene;
        EntityId e = scene.create_entity();
        TEST("entity alive", scene.alive(e));

        scene.add_component<RenderComponent>(e);
        TEST("has render", scene.has_component<RenderComponent>(e));

        scene.destroy_entity(e);
        TEST("entity dead", !scene.alive(e));
        TEST("component gone", !scene.has_component<RenderComponent>(e));
    }

    // ── 6. EcsScene component + transform ───────────────────────
    printf("Test 6: EcsScene component + transform\n");
    {
        EcsScene scene;
        EntityId e = scene.create_entity();
        scene.scene_graph().attach(e);
        scene.scene_graph().set_position(e, {10.0f, 20.0f, 30.0f});

        auto& rc = scene.add_component<RenderComponent>(e);
        rc.transparent = true;

        TEST("has render", scene.has_component<RenderComponent>(e));
        TEST("transparent", scene.get_component<RenderComponent>(e)->transparent);

        // World matrix reflects transform
        auto wm = scene.scene_graph().world_matrix(e);
        TEST("world position", glm::vec3(wm[3]).x == 10.0f);
    }

    // ── 7. EcsScene::clear ──────────────────────────────────────
    printf("Test 7: EcsScene clear\n");
    {
        EcsScene scene;
        for (int i = 0; i < 25; ++i) {
            EntityId e = scene.create_entity();
            scene.add_component<RenderComponent>(e);
            scene.scene_graph().attach(e);
        }
        TEST("25 entities", scene.entity_count() == 25);
        TEST("25 renders", scene.render_components().count() == 25);
        TEST("25 transforms", scene.scene_graph().count() == 25);

        scene.clear();
        TEST("0 entities", scene.entity_count() == 0);
        TEST("0 renders", scene.render_components().count() == 0);
        TEST("0 transforms", scene.scene_graph().count() == 0);
    }

    // ── 8. PhysicsComponent type ────────────────────────────────
    printf("Test 8: PhysicsComponent storage and access\n");
    {
        EcsScene scene;
        EntityId e = scene.create_entity();

        auto& pc = scene.add_component<PhysicsComponent>(e);
        pc.is_static = true;
        pc.collision_layer = 2;
        pc.collision_mask  = 6;

        TEST("has physics", scene.has_component<PhysicsComponent>(e));
        auto* pcp = scene.get_component<PhysicsComponent>(e);
        TEST("is_static", pcp->is_static);
        TEST("layer", pcp->collision_layer == 2);
        TEST("mask", pcp->collision_mask == 6);

        scene.remove_component<PhysicsComponent>(e);
        TEST("removed", !scene.has_component<PhysicsComponent>(e));
    }

    // ── 9. AudioComponent type ──────────────────────────────────
    printf("Test 9: AudioComponent storage and access\n");
    {
        EcsScene scene;
        EntityId e = scene.create_entity();

        auto& ac = scene.add_component<AudioComponent>(e);
        ac.sound_path = "sounds/boom.wav";
        ac.volume = 0.5f;
        ac.looping = false;

        TEST("has audio", scene.has_component<AudioComponent>(e));
        auto* acp = scene.get_component<AudioComponent>(e);
        TEST("path", acp->sound_path == "sounds/boom.wav");
        TEST("volume", acp->volume == 0.5f);

        scene.destroy_entity(e);
        TEST("audio removed on destroy", !scene.has_component<AudioComponent>(e));
    }

    // ── 10. All three components on one entity ──────────────────
    printf("Test 10: multiple components on one entity\n");
    {
        EcsScene scene;
        EntityId e = scene.create_entity();
        scene.scene_graph().attach(e);

        scene.add_component<RenderComponent>(e);
        scene.add_component<PhysicsComponent>(e);
        scene.add_component<AudioComponent>(e);

        TEST("has render", scene.has_component<RenderComponent>(e));
        TEST("has physics", scene.has_component<PhysicsComponent>(e));
        TEST("has audio", scene.has_component<AudioComponent>(e));
        TEST("has transform", scene.scene_graph().has(e));
    }

    // ── 11. EcsPhysicsAdapter basic lifecycle ───────────────────
    printf("Test 11: EcsPhysicsAdapter attach / detach\n");
    {
        EcsScene scene;
        CollisionWorld cw;
        EcsPhysicsAdapter adapter(&scene, &cw);

        EntityId e = scene.create_entity();
        scene.scene_graph().attach(e);
        scene.add_component<PhysicsComponent>(e);

        TEST("no proxy before attach", adapter.proxy_count() == 0);
        adapter.attach(e);
        TEST("proxy after attach", adapter.proxy_count() == 1);
        TEST("collider registered", cw.collider_count() == 1);

        adapter.detach(e);
        TEST("proxy removed", adapter.proxy_count() == 0);
        TEST("collider removed", cw.collider_count() == 0);
    }

    // ── 12. EcsPhysicsAdapter sync transforms ───────────────────
    printf("Test 12: EcsPhysicsAdapter transform sync\n");
    {
        EcsScene scene;
        CollisionWorld cw;
        EcsPhysicsAdapter adapter(&scene, &cw);

        EntityId e = scene.create_entity();
        scene.scene_graph().attach(e);
        scene.add_component<PhysicsComponent>(e);
        scene.scene_graph().set_position(e, {42.0f, 0.0f, 0.0f});

        adapter.attach(e);
        adapter.sync();

        // Sync copies ECS world position into old Entity proxy.
        // CollisionWorld uses the proxy in update_aabbs().
        // For this test we just verify the proxy was created and sync didn't crash.
        TEST("proxy exists after sync", adapter.proxy_count() == 1);
    }

    // ── 13. Entity destruction auto-cleanup via sync ────────────
    printf("Test 13: sync cleans up externally destroyed entities\n");
    {
        EcsScene scene;
        CollisionWorld cw;
        EcsPhysicsAdapter adapter(&scene, &cw);

        EntityId e = scene.create_entity();
        scene.scene_graph().attach(e);
        scene.add_component<PhysicsComponent>(e);

        adapter.attach(e);
        TEST("proxy before destroy", adapter.proxy_count() == 1);

        scene.destroy_entity(e);  // ECS destroys entity, adapter doesn't know yet
        TEST("entity dead", !scene.alive(e));

        adapter.sync();  // sync detects stale entry and cleans up
        TEST("proxy cleaned up", adapter.proxy_count() == 0);
    }

    // ── 14. Pool reuse after slot recycling ─────────────────────
    printf("Test 14: component pool slot reuse\n");
    {
        EntityRegistry reg;
        ComponentPool<RenderComponent> pool;

        EntityId ids[5];
        for (int i = 0; i < 5; ++i) {
            ids[i] = reg.create();
            pool.add(ids[i]);
        }
        TEST("5 components", pool.count() == 5);

        // Destroy all entities, removing components
        for (int i = 0; i < 5; ++i) {
            pool.remove(ids[i]);
            reg.destroy(ids[i]);
        }
        TEST("0 after remove", pool.count() == 0);

        // New entities at same slots should start fresh
        for (int i = 0; i < 5; ++i) {
            EntityId e = reg.create();
            TEST("new entity not in pool", !pool.has(e));
            pool.add(e);
            TEST("new entity added", pool.has(e));
        }
        TEST("5 new components", pool.count() == 5);
    }

    // ── Results ─────────────────────────────────────────────────
    printf("\n");
    if (s_failures == 0)
        printf("All component tests passed!\n");
    else
        printf("%d test(s) FAILED.\n", s_failures);

    return s_failures > 0 ? 1 : 0;
}
