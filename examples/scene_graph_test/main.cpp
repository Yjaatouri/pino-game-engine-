// SceneGraph / Transform hierarchy lifecycle validation.
// Console test — no window, no GPU.

#include "engine/ecs/entity.h"
#include "engine/ecs/entity_registry.h"
#include "engine/ecs/scene_graph.h"
#include <cstdio>
#include <cassert>
#include <glm/gtc/quaternion.hpp>

using namespace pino;

static int s_failures = 0;

#define TEST(name, expr) do { \
    if (!(expr)) { \
        printf("  FAIL: %s\n", name); \
        ++s_failures; \
    } \
} while(0)

int main() {
    // ── 1. Basic attach and detach ──────────────────────────────
    printf("Test 1: attach / detach / has\n");
    {
        EntityRegistry reg;
        SceneGraph sg(&reg);
        EntityId e = reg.create();

        TEST("no transform before attach", !sg.has(e));
        TEST("attach succeeds", sg.attach(e));
        TEST("has transform after attach", sg.has(e));
        TEST("count = 1", sg.count() == 1);

        sg.detach(e);
        TEST("no transform after detach", !sg.has(e));
        TEST("count = 0", sg.count() == 0);
    }

    // ── 2. Attach with parent ───────────────────────────────────
    printf("Test 2: attach with parent\n");
    {
        EntityRegistry reg;
        SceneGraph sg(&reg);
        EntityId parent = reg.create();
        EntityId child  = reg.create();

        TEST("attach parent", sg.attach(parent));
        TEST("attach child to parent", sg.attach(child, parent));
        TEST("child parent == parent", sg.parent(child) == parent);
        TEST("parent has 1 child", sg.child_count(parent) == 1);
    }

    // ── 3. Local transform access ───────────────────────────────
    printf("Test 3: local transform get/set\n");
    {
        EntityRegistry reg;
        SceneGraph sg(&reg);
        EntityId e = reg.create();
        sg.attach(e);

        sg.set_position(e, {1.0f, 2.0f, 3.0f});
        sg.set_rotation(e, glm::quat(glm::vec3(0, 0, 0)));
        sg.set_scale(e, {2.0f, 2.0f, 2.0f});

        auto* t = sg.get(e);
        TEST("position x", t->position.x == 1.0f);
        TEST("position y", t->position.y == 2.0f);
        TEST("position z", t->position.z == 3.0f);
        TEST("scale x", t->scale.x == 2.0f);
    }

    // ── 4. World matrix computation ─────────────────────────────
    printf("Test 4: world matrix from hierarchy\n");
    {
        EntityRegistry reg;
        SceneGraph sg(&reg);
        EntityId parent = reg.create();
        EntityId child  = reg.create();
        sg.attach(parent);
        sg.attach(child, parent);

        sg.set_position(parent, {10.0f, 0.0f, 0.0f});
        sg.set_position(child,   {1.0f, 0.0f, 0.0f});

        auto child_world = sg.world_matrix(child);
        glm::vec3 wp = glm::vec3(child_world[3]);
        TEST("world position = parent + child", wp.x == 11.0f && wp.y == 0.0f && wp.z == 0.0f);
    }

    // ── 5. Dirty flag recompute ─────────────────────────────────
    printf("Test 5: dirty flag recomputes on transform change\n");
    {
        EntityRegistry reg;
        SceneGraph sg(&reg);
        EntityId e = reg.create();
        sg.attach(e);

        sg.set_position(e, {5.0f, 0.0f, 0.0f});
        auto w1 = sg.world_matrix(e);
        TEST("first world ok", glm::vec3(w1[3]).x == 5.0f);

        sg.set_position(e, {20.0f, 0.0f, 0.0f});
        auto w2 = sg.world_matrix(e);
        TEST("world updated after change", glm::vec3(w2[3]).x == 20.0f);
    }

    // ── 6. Deep hierarchy world matrix ──────────────────────────
    printf("Test 6: deep hierarchy (grandchild)\n");
    {
        EntityRegistry reg;
        SceneGraph sg(&reg);
        auto gp = reg.create(); // grandparent
        auto p  = reg.create(); // parent
        auto c  = reg.create(); // child

        sg.attach(gp);
        sg.attach(p, gp);
        sg.attach(c, p);

        sg.set_position(gp, {1.0f, 0.0f, 0.0f});
        sg.set_position(p,  {2.0f, 0.0f, 0.0f});
        sg.set_position(c,  {3.0f, 0.0f, 0.0f});

        auto w = sg.world_matrix(c);
        glm::vec3 wp = glm::vec3(w[3]);
        TEST("grandchild wp = 1+2+3", wp.x == 6.0f && wp.y == 0.0f && wp.z == 0.0f);
    }

    // ── 7. Reparent ─────────────────────────────────────────────
    printf("Test 7: reparent\n");
    {
        EntityRegistry reg;
        SceneGraph sg(&reg);
        auto p1 = reg.create();
        auto p2 = reg.create();
        auto c  = reg.create();
        sg.attach(p1);
        sg.attach(p2);
        sg.attach(c, p1);

        sg.set_position(p1, {10.0f, 0.0f, 0.0f});
        sg.set_position(p2, {100.0f, 0.0f, 0.0f});
        sg.set_position(c,   {1.0f, 0.0f, 0.0f});

        TEST("child under p1", glm::vec3(sg.world_matrix(c)[3]).x == 11.0f);

        sg.set_parent(c, p2);
        TEST("child now under p2", sg.parent(c) == p2);
        TEST("p1 has 0 children", sg.child_count(p1) == 0);
        TEST("p2 has 1 child", sg.child_count(p2) == 1);
        TEST("world updated after reparent", glm::vec3(sg.world_matrix(c)[3]).x == 101.0f);
    }

    // ── 8. Cycle detection ──────────────────────────────────────
    printf("Test 8: cycle detection\n");
    {
        EntityRegistry reg;
        SceneGraph sg(&reg);
        auto a = reg.create();
        auto b = reg.create();
        sg.attach(a);
        sg.attach(b, a); // b is child of a

        // Try to make a a child of b (would create cycle)
        TEST("cycle rejected", !sg.set_parent(a, b));
        TEST("parent unchanged", sg.parent(a) == NullEntity);
        TEST("b still child of a", sg.parent(b) == a);
    }

    // ── 9. Detach orphans children ──────────────────────────────
    printf("Test 9: detach orphans children\n");
    {
        EntityRegistry reg;
        SceneGraph sg(&reg);
        auto p = reg.create();
        auto c = reg.create();
        sg.attach(p);
        sg.attach(c, p);
        TEST("child has parent", sg.parent(c) == p);

        sg.detach(p);
        TEST("child orphaned", !sg.parent(c));
        TEST("p has no transform", !sg.has(p));
    }

    // ── 10. transform after entity destruction ──────────────────
    printf("Test 10: access after entity destruction is safe\n");
    {
        EntityRegistry reg;
        SceneGraph sg(&reg);
        auto e = reg.create();
        sg.attach(e);

        sg.set_position(e, {42.0f, 0.0f, 0.0f});
        TEST("alive works", sg.has(e));

        reg.destroy(e);
        TEST("dead returns null get", sg.get(e) == nullptr);
        // world_matrix should return identity matrix for dead entity
        auto w = sg.world_matrix(e);
        TEST("dead returns identity", w == glm::mat4(1.0f));
    }

    // ── 11. Clear ───────────────────────────────────────────────
    printf("Test 11: clear\n");
    {
        EntityRegistry reg;
        SceneGraph sg(&reg);
        for (int i = 0; i < 50; ++i) {
            auto e = reg.create();
            sg.attach(e);
        }
        TEST("50 before clear", sg.count() == 50);
        sg.clear();
        TEST("0 after clear", sg.count() == 0);
    }

    // ── 12. Scale inheritance ───────────────────────────────────
    printf("Test 12: scale inheritance\n");
    {
        EntityRegistry reg;
        SceneGraph sg(&reg);
        auto p = reg.create();
        auto c = reg.create();
        sg.attach(p);
        sg.attach(c, p);
        sg.set_scale(p, {2.0f, 2.0f, 2.0f});
        sg.set_position(c, {1.0f, 0.0f, 0.0f});

        auto w = sg.world_matrix(c);
        TEST("child wp scaled by parent", glm::vec3(w[3]).x == 2.0f);
    }

    // ── 13. Slot reuse after detach ─────────────────────────────
    printf("Test 13: slot reuse after detach\n");
    {
        EntityRegistry reg;
        SceneGraph sg(&reg);
        EntityId ids[10];
        for (int i = 0; i < 10; ++i) {
            ids[i] = reg.create();
            sg.attach(ids[i]);
        }
        for (int i = 0; i < 10; ++i)
            sg.detach(ids[i]);
        TEST("0 after detach all", sg.count() == 0);

        // New attaches should reuse slots
        auto e = reg.create();
        sg.attach(e);
        TEST("has new entity", sg.has(e));
    }

    // ── Results ─────────────────────────────────────────────────
    printf("\n");
    if (s_failures == 0)
        printf("All scene graph tests passed!\n");
    else
        printf("%d test(s) FAILED.\n", s_failures);

    return s_failures > 0 ? 1 : 0;
}
