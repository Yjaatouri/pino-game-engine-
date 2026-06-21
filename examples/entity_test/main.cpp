// Entity system lifecycle validation.
// Runs as a console test (no window, no GPU required).

#include "engine/ecs/entity.h"
#include "engine/ecs/entity_registry.h"
#include "engine/ecs/entity_handle.h"
#include <cstdio>
#include <cassert>

using namespace pino;

static int s_failures = 0;

#define TEST(name, expr) do { \
    if (!(expr)) { \
        printf("  FAIL: %s\n", name); \
        ++s_failures; \
    } \
} while(0)

int main() {
    // ── 1. EntityId basics ──────────────────────────────────────
    printf("Test 1: EntityId creation and equality\n");
    {
        EntityId a{0, 1};
        EntityId b{0, 1};
        EntityId c{1, 1};
        TEST("equal", a == b);
        TEST("not equal", a != c);
        TEST("null is false", !NullEntity);
        TEST("non-null is true", !!a);
    }

    // ── 2. Create entities ──────────────────────────────────────
    printf("Test 2: EntityRegistry::create\n");
    {
        EntityRegistry reg;
        EntityId e1 = reg.create();
        EntityId e2 = reg.create();

        TEST("e1 != e2", e1 != e2);
        TEST("e1 alive", reg.alive(e1));
        TEST("e2 alive", reg.alive(e2));
        TEST("count = 2", reg.count() == 2);
    }

    // ── 3. Destroy entities ──────────────────────────────────────
    printf("Test 3: EntityRegistry::destroy\n");
    {
        EntityRegistry reg;
        EntityId e1 = reg.create();
        EntityId e2 = reg.create();

        reg.destroy(e1);
        TEST("e1 dead", !reg.alive(e1));
        TEST("e2 still alive", reg.alive(e2));
        TEST("count = 1", reg.count() == 1);

        reg.destroy(e2);
        TEST("e2 dead", !reg.alive(e2));
        TEST("count = 0", reg.count() == 0);
    }

    // ── 4. Destroy invalid/non-existent ─────────────────────────
    printf("Test 4: destroy non-existent entity is safe\n");
    {
        EntityRegistry reg;
        reg.destroy(NullEntity);     // must not crash
        reg.destroy({0, 999});       // never-created ID — must not crash
        TEST("count still 0", reg.count() == 0);
    }

    // ── 5. Slot reuse with generation bump ───────────────────────
    printf("Test 5: slot reuse invalidates stale IDs\n");
    {
        EntityRegistry reg;
        EntityId original = reg.create();
        u32 orig_index = original.index;
        u32 orig_gen   = original.generation;

        reg.destroy(original);
        TEST("original dead", !reg.alive(original));

        // Reuse should pick the freed slot (LIFO free list)
        EntityId recycled = reg.create();
        TEST("recycles same index", recycled.index == orig_index);
        TEST("generation bumped", recycled.generation == orig_gen + 1);

        // Original ID is permanently dead
        TEST("stale ID detected", !reg.alive(original));
    }

    // ── 6. Clear ─────────────────────────────────────────────────
    printf("Test 6: clear destroys all\n");
    {
        EntityRegistry reg;
        EntityId e1 = reg.create();
        EntityId e2 = reg.create();
        EntityId e3 = reg.create();
        TEST("count = 3 before clear", reg.count() == 3);

        reg.clear();
        TEST("count = 0 after clear", reg.count() == 0);
        TEST("e1 dead", !reg.alive(e1));
        TEST("e2 dead", !reg.alive(e2));
        TEST("e3 dead", !reg.alive(e3));
    }

    // ── 7. EntityHandle safety ──────────────────────────────────
    printf("Test 7: EntityHandle stale detection\n");
    {
        EntityRegistry reg;
        EntityId id = reg.create();
        EntityHandle handle(&reg, id);

        TEST("handle alive after create", handle.alive());
        TEST("handle converts to true", !!handle);
        TEST("handle id matches", handle.id() == id);

        reg.destroy(id);
        TEST("handle dead after destroy", !handle.alive());
        TEST("handle converts to false", !handle);

        // Creating a new entity at the same slot does NOT revive the handle
        EntityId new_id = reg.create();
        (void)new_id;
        TEST("handle still dead after slot reuse", !handle.alive());
    }

    // ── 8. Many cycles (stress free list) ───────────────────────
    printf("Test 8: 1000 create/destroy cycles\n");
    {
        EntityRegistry reg;
        EntityId ids[1000];

        for (int i = 0; i < 1000; ++i)
            ids[i] = reg.create();
        TEST("count = 1000", reg.count() == 1000);

        for (int i = 0; i < 1000; ++i)
            reg.destroy(ids[i]);
        TEST("count = 0", reg.count() == 0);

        // All dead
        for (int i = 0; i < 1000; ++i)
            TEST("cycle dead", !reg.alive(ids[i]));
    }

    // ── 9. Iteration ────────────────────────────────────────────
    printf("Test 9: each() iteration\n");
    {
        EntityRegistry reg;
        EntityId ids[5];
        for (int i = 0; i < 5; ++i)
            ids[i] = reg.create();

        int count = 0;
        reg.each([&](EntityId e) {
            bool found = false;
            for (auto id : ids)
                if (id == e) { found = true; break; }
            TEST("iterated entity exists", found);
            ++count;
        });
        TEST("iterated count = 5", count == 5);
    }

    // ── 10. Capacity growth ─────────────────────────────────────
    printf("Test 10: capacity grows on demand\n");
    {
        EntityRegistry reg;
        EntityId ids[256];
        for (int i = 0; i < 256; ++i)
            ids[i] = reg.create();
        TEST("capacity grew", reg.capacity() >= 256);
        TEST("capacity >= count", reg.capacity() >= reg.count());
    }

    // ── Results ─────────────────────────────────────────────────
    printf("\n");
    if (s_failures == 0)
        printf("All entity tests passed!\n");
    else
        printf("%d test(s) FAILED.\n", s_failures);

    return s_failures > 0 ? 1 : 0;
}
