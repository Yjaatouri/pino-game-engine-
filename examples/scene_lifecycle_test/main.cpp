// Scene lifecycle + deferred destruction + system dispatch integration tests.
// Console test — no window, no GPU.

#include "engine/ecs/ecs_world.h"
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
    // ── 1. Basic lifecycle: load / update / destroy ─────────────
    printf("Test 1: lifecycle load/update/destroy\n");
    {
        EcsWorld world;
        world.load();
        world.update(0.016f);
        world.destroy();
    }

    // ── 2. Entity creation ─────────────────────────────────────
    printf("Test 2: entity creation\n");
    {
        EcsWorld world;
        world.load();

        auto e = world.create_entity();
        TEST("entity alive", world.alive(e));
        TEST("count is 1", world.entity_count() == 1);

        world.destroy();
    }

    // ── 3. Deferred destruction ────────────────────────────────
    printf("Test 3: deferred destruction\n");
    {
        EcsWorld world;
        world.load();

        auto e = world.create_entity();
        world.destroy_entity(e);  // deferred

        // Entity is still alive until flush (next update)
        TEST("still alive before flush", world.alive(e));
        TEST("count still 1", world.entity_count() == 1);

        world.update(0.016f);  // triggers flush_destroyed()

        TEST("dead after update", !world.alive(e));
        TEST("count is 0", world.entity_count() == 0);

        world.destroy();
    }

    // ── 4. Deferred destruction within update callback ─────────
    printf("Test 4: deferred destruction in callback\n");
    {
        EcsWorld world;
        world.load();

        auto e = world.create_entity();
        world.scene().scene_graph().attach(e);
        world.scene().add_component<RenderComponent>(e);

        EntityId destroyed_in_callback;

        world.set_update_callback([&](EcsWorld& w, f32) {
            destroyed_in_callback = w.create_entity();
            w.destroy_entity(e);  // deferred — safe during iteration
        });

        world.update(0.016f);

        // e was destroyed in the callback — still alive during update,
        // will be flushed at start of next update
        TEST("e alive during same update", world.alive(e));

        world.update(0.016f);  // flush from previous callback

        TEST("e dead after next update", !world.alive(e));
        TEST("destroyed_in_callback alive", world.alive(destroyed_in_callback));

        world.destroy();
    }

    // ── 5. Multiple destroys of same entity are idempotent ─────
    printf("Test 5: multiple destroys idempotent\n");
    {
        EcsWorld world;
        world.load();

        auto e = world.create_entity();
        world.destroy_entity(e);
        world.destroy_entity(e);  // duplicate
        world.destroy_entity(e);  // triple

        TEST("count 1 before flush", world.entity_count() == 1);
        world.update(0.016f);
        TEST("count 0 after flush", world.entity_count() == 0);

        world.destroy();
    }

    // ── 6. Immediate destruction (outside update) ─────────────
    printf("Test 6: immediate destruction\n");
    {
        EcsWorld world;
        world.load();

        auto e = world.create_entity();
        world.destroy_entity_immediate(e);
        TEST("dead immediately", !world.alive(e));
        TEST("count 0", world.entity_count() == 0);

        world.destroy();
    }

    // ── 7. Clear resets everything ─────────────────────────────
    printf("Test 7: clear\n");
    {
        EcsWorld world;
        world.load();

        auto e1 = world.create_entity();
        auto e2 = world.create_entity();
        world.destroy_entity(e1);  // pending

        world.clear();
        TEST("count 0 after clear", world.entity_count() == 0);
        TEST("pending cleared", !world.alive(e1));  // entity already gone via clear
        TEST("e2 also gone", !world.alive(e2));

        world.destroy();
    }

    // ── 8. CollisionWorld integration via update ───────────────
    printf("Test 8: physics integration\n");
    {
        EcsWorld world;
        CollisionWorld cw;

        world.set_collision_world(&cw);
        world.load();

        auto e = world.create_entity();
        world.scene().scene_graph().attach(e);
        world.scene().add_component<PhysicsComponent>(e);

        world.update(0.016f);

        TEST("collider registered", cw.collider_count() == 1);

        world.destroy_entity(e);
        world.update(0.016f);  // flush + physics sync
        TEST("collider removed", cw.collider_count() == 0);

        world.destroy();
    }

    // ── 9. Null external systems are safely skipped ────────────
    printf("Test 9: null systems safely skipped\n");
    {
        EcsWorld world;
        world.load();
        // No systems set — should not crash
        world.update(0.016f);
        world.update(0.033f);
        world.destroy();
    }

    // ── 10. World destroy with pending entities ────────────────
    printf("Test 10: destroy with pending entities\n");
    {
        EcsWorld world;
        world.load();

        auto e1 = world.create_entity();
        auto e2 = world.create_entity();
        world.destroy_entity(e1);  // pending, never flushed

        world.destroy();  // should handle pending gracefully
        // destroy() already called, no assertion to make beyond no crash
        TEST("destroy handled pending", true);
    }

    // ── 11. Create entity during callback ──────────────────────
    printf("Test 11: create entity in callback\n");
    {
        EcsWorld world;
        world.load();

        EntityId created;

        world.set_update_callback([&](EcsWorld& w, f32) {
            created = w.create_entity();
            w.scene().scene_graph().attach(created);
        });

        world.update(0.016f);
        TEST("entity created in callback", world.alive(created));

        world.update(0.016f);
        TEST("still alive next frame", world.alive(created));

        world.destroy();
    }

    // ── 12. Destroy then recreate reuses slot ──────────────────
    printf("Test 12: destroy then recreate slot reuse\n");
    {
        EcsWorld world;
        world.load();

        auto e1 = world.create_entity();
        u32 idx1 = e1.index;

        world.destroy_entity_immediate(e1);
        auto e2 = world.create_entity();

        // After free-list reuse, e2 may have the same index but different generation
        TEST("slot reused or new", e2.index == idx1 || e2.index > idx1);
        TEST("generation bumped", e2.generation > e1.generation);
        TEST("e1 dead", !world.alive(e1));
        TEST("e2 alive", world.alive(e2));

        world.destroy();
    }

    // ── 13. RenderQueue skipped with no render components ──────
    printf("Test 13: no render components skips render queue\n");
    {
        EcsWorld world;
        RenderQueue rq;

        world.set_render_queue(&rq);
        world.load();

        world.update(0.016f);
        TEST("no commands", rq.command_count() == 0);

        world.destroy();
    }

    // ── Summary ────────────────────────────────────────────────
    printf("\n");
    if (s_failures == 0) {
        printf("All scene lifecycle tests passed!\n");
        return 0;
    } else {
        printf("%d test(s) FAILED!\n", s_failures);
        return 1;
    }
}
