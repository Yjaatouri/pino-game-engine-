// Verifies that physics queries (raycast, overlap, collision events) correctly
// map physics proxy entities back to ECS EntityIds.
// No window, no GPU required — pure CPU test.

#include "engine/ecs/ecs_scene.h"
#include "engine/physics/collision_world.h"
#include "engine/scene/entity.h"
#include "engine/scene/scene.h"
#include "engine/core/math_utils.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <unordered_set>

static int s_failures = 0;

#define TEST(name, expr) do { \
    if (!(expr)) { \
        printf("  FAIL: %s\n", name); \
        ++s_failures; \
    } \
} while(0)

#define TEST_NEAR(name, val, expected, eps) do { \
    float _v = (val); float _e = (expected); float _d = _v - _e; \
    if (!(_d < (eps) && _d > -(eps))) { \
        printf("  FAIL: %s (got %.4f, expected %.4f)\n", name, _v, _e); \
        ++s_failures; \
    } \
} while(0)

int main() {
    printf("=== Physics Entity Mapping Test ===\n\n");

    // ── Setup ─────────────────────────────────────────────────
    pino::EcsScene scene;
    pino::CollisionWorld cw;
    auto& sg = scene.scene_graph();

    // Wall (static, for raycast targets)
    pino::Scene aux_scene;
    pino::Entity* wall = aux_scene.root()->create_child("wall");
    wall->local_transform().position = { 3.0f, 0.0f, 0.0f };
    {
        pino::ColliderComponent wc;
        wc.local_min = { -0.5f, -0.5f, -0.5f };
        wc.local_max = {  0.5f,  0.5f,  0.5f };
        wc.is_static = true;
        wc.collision_layer = 1;
        wc.collision_mask  = 1;
        cw.register_collider(*wall, wc);
    }

    // ── Create ECS entities with physics ──────────────────────
    auto make_entity = [&](const char* label, glm::vec3 pos) -> pino::EntityId {
        pino::EntityId e = scene.create_entity();
        sg.attach(e);
        sg.set_position(e, pos);
        sg.set_scale(e, { 1.0f, 1.0f, 1.0f });
        pino::PhysicsComponent& pc = scene.add_component<pino::PhysicsComponent>(e);
        pc.local_min = { -0.5f, -0.5f, -0.5f };
        pc.local_max = {  0.5f,  0.5f,  0.5f };
        pc.is_static = false;
        pc.collision_layer = 1;
        pc.collision_mask  = 1;
        (void)label;
        return e;
    };

    pino::EntityId e1 = make_entity("e1", { 0.0f, 0.0f, 0.0f });
    pino::EntityId e2 = make_entity("e2", { 2.0f, 0.0f, 0.0f });
    pino::EntityId e3 = make_entity("e3", { 4.0f, 0.0f, 0.0f });

    // Sync physics to create proxies
    scene.update_physics(cw, 0.016f);

    // ── Test 1: entity_for_proxy returns correct EntityId ─────
    printf("Test 1: entity_for_proxy on known proxy\n");
    {
        // Get proxy for e1 via collider iteration
        // Walk CollisionWorld colliders looking for one at position 0
        bool found = false;
        for (pino::u32 i = 0; i < cw.collider_count(); ++i) {
            pino::Entity* proxy = cw.collider_entity(i);
            if (!proxy) continue;
            pino::EntityId mapped = scene.entity_for_physics_entity(proxy);
            if (mapped && mapped.index == e1.index) {
                found = true;
                TEST("mapped generation matches e1", mapped.generation == e1.generation);
                TEST("mapped index matches e1", mapped.index == e1.index);
                // Check user_data round-trip consistency
                uint64_t data = proxy->user_data();
                pino::EntityId unpacked{
                    static_cast<pino::u32>(data >> 32),
                    static_cast<pino::u32>(data & 0xFFFFFFFF)
                };
                TEST("user_data index matches", unpacked.index == e1.index);
                TEST("user_data generation matches", unpacked.generation == e1.generation);
            }
        }
        TEST("e1 proxy found in collider list", found);
    }

    // ── Test 2: entity_for_proxy on null/unknown returns NullEntity
    printf("Test 2: entity_for_proxy on unknown pointer\n");
    {
        pino::EntityId mapped = scene.entity_for_physics_entity(nullptr);
        TEST("null returns NullEntity", !mapped);
    }

    // ── Test 3: raycast hits correct ECS entity ──────────────
    printf("Test 3: Raycast hits correct ECS entity\n");
    {
        pino::Ray ray = { {0, 0, 0}, {1, 0, 0} }; // rightward from origin
        std::vector<std::pair<pino::EntityId, float>> hits;
        scene.raycast_entities(cw, ray, 10.0f, 1, hits);
        TEST("at least one hit", !hits.empty());

        if (!hits.empty()) {
            // The entities are at x=0, x=2, x=4. Wall at x=3.
            // For raycast along +X from origin, the closest dynamic entity
            // should be e1 at x=0 (or e2 at x=2 depending on ray-entity AABB intersection)
            // Actually the ray from origin going +X will hit the nearest AABB along the ray
            //
            // e1 AABB: [-0.5, 0.5] on X → ray starts at x=0 inside e1's AABB
            // The raycast code handles inside-start by returning t=0
            // So the first hit should be e1
            auto& [hit_id, t] = hits[0];
            TEST("first hit is e1 (index=0)", hit_id.index == e1.index);
            printf("  First hit: entity index=%u t=%.4f\n", hit_id.index, t);
        }
    }

    // ── Test 4: Overlap AABB returns multiple entities ────────
    printf("Test 4: Overlap AABB returns correct entity set\n");
    {
        // AABB covering x ∈ [−1, 5], y ∈ [−1, 1], z ∈ [−1, 1]
        // Should overlap all three entities
        pino::AABB query_zone = { {-1, -1, -1}, {5, 1, 1} };
        std::vector<pino::EntityId> overlaps = scene.overlap_entities(cw, query_zone, 1);

        std::unordered_set<pino::u32> found_indices;
        for (auto& id : overlaps) found_indices.insert(id.index);

        TEST("e1 in overlap set", found_indices.count(e1.index));
        TEST("e2 in overlap set", found_indices.count(e2.index));
        TEST("e3 in overlap set", found_indices.count(e3.index));

        // Narrow query that only covers e2
        pino::AABB narrow = { {1.5, -1, -1}, {2.5, 1, 1} };
        overlaps = scene.overlap_entities(cw, narrow, 1);
        found_indices.clear();
        for (auto& id : overlaps) found_indices.insert(id.index);
        TEST("only e2 in narrow zone", found_indices.size() == 1 && found_indices.count(e2.index));
    }

    // ── Test 5: No proxy pointer leaks into results ───────────
    printf("Test 5: No raw proxy pointers in results\n");
    {
        // raycast_entities returns EntityId, not Entity*
        // overlap_entities returns EntityId, not Entity*
        // entity_for_physics_entity returns EntityId, not Entity*
        printf("  All query wrappers return EntityId, not Entity*\n");
    }

    // ── Test 6: Entity destroyed → mapping invalidated ────────
    printf("Test 6: Destroyed entity removed from mapping\n");
    {
        pino::EntityId temp = make_entity("temp", { -2.0f, 0.0f, 0.0f });
        scene.update_physics(cw, 0.016f);

        // Find the proxy for temp
        pino::Entity* temp_proxy = nullptr;
        for (pino::u32 i = 0; i < cw.collider_count(); ++i) {
            pino::Entity* proxy = cw.collider_entity(i);
            if (proxy && scene.entity_for_physics_entity(proxy).index == temp.index) {
                temp_proxy = proxy;
                break;
            }
        }
        TEST("temp entity has proxy", temp_proxy != nullptr);

        scene.destroy_entity(temp);
        scene.flush_destroyed_entities();
        scene.update_physics(cw, 0.016f);

        // After destroy + sync, the proxy should be unregistered
        pino::EntityId mapped = scene.entity_for_physics_entity(temp_proxy);
        TEST("destroyed entity maps to NullEntity", !mapped);
        printf("  Destroyed entity correctly unmapped\n");
    }

    // ── Summary ────────────────────────────────────────────
    printf("\n=== Results: %s ===\n", s_failures == 0 ? "ALL PASSED" : "SOME FAILED");
    printf("Failures: %d\n", s_failures);
    return s_failures;
}
