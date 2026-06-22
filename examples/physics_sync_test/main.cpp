// Verifies that after collision resolution, corrected proxy positions
// are written back to ECS SceneGraph transforms (bidirectional sync).
// No window, no GPU required — pure CPU test.

#include "engine/ecs/ecs_scene.h"
#include "engine/physics/collision_world.h"
#include "engine/scene/entity.h"
#include "engine/scene/scene.h"
#include <cstdio>
#include <cassert>
#include <cmath>

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
    printf("=== Physics Bidirectional Sync Test ===\n\n");

    // ── Setup ─────────────────────────────────────────────────
    pino::EcsScene scene;
    pino::CollisionWorld cw;
    auto& sg = scene.scene_graph();

    // Wall: tree Entity with static collider
    pino::Scene wall_scene;
    pino::Entity* wall = wall_scene.root()->create_child("wall");
    wall->local_transform().position = { 2.0f, 0.0f, 0.0f };
    wall->local_transform().scale    = { 1.0f, 1.0f, 1.0f };
    {
        pino::ColliderComponent wc;
        wc.local_min = { -0.5f, -0.5f, -0.5f };
        wc.local_max = {  0.5f,  0.5f,  0.5f };
        wc.is_static = true;
        wc.collision_layer = 1;
        wc.collision_mask  = 1;
        cw.register_collider(*wall, wc);
    }

    // Dynamic entity (player-like)
    pino::EntityId ent = scene.create_entity();
    sg.attach(ent);
    sg.set_position(ent, { 0.0f, 0.0f, 0.0f });
    sg.set_scale(ent, { 1.0f, 1.0f, 1.0f });

    pino::PhysicsComponent& pc = scene.add_component<pino::PhysicsComponent>(ent);
    pc.local_min = { -0.5f, -0.5f, -0.5f };
    pc.local_max = {  0.5f,  0.5f,  0.5f };
    pc.is_static = false;
    pc.collision_layer = 1;
    pc.collision_mask  = 1;

    // ── Step 1: Initial sync — entity at origin ───────────────
    printf("Test 1: Initial position is preserved\n");
    scene.update_physics(cw, 0.016f);
    glm::vec3 pos = sg.world_position(ent);
    TEST_NEAR("ECS X at 0", pos.x, 0.0f, 0.01f);

    // ── Step 2: Move entity into wall, then resolve ──────────
    // Wall at x=2.0, half-size 0.5 → wall occupies [1.5, 2.5]
    // Entity at x=1.6, half-size 0.5 → entity occupies [1.1, 2.1]
    // Overlap = [1.5, 2.1] → push entity left so right edge ≤ 1.5
    // Corrected position: entity center = 1.5 - 0.5 = 1.0
    printf("Test 2: ECS position corrected after wall collision\n");
    sg.set_position(ent, { 1.6f, 0.0f, 0.0f });
    scene.update_physics(cw, 0.016f);

    pos = sg.world_position(ent);
    printf("  ECS entity X after resolution: %.4f\n", pos.x);
    TEST("ECS entity pushed left of wall boundary", pos.x < 1.49f);
    TEST("ECS entity not past wall center", pos.x < 1.5f);

    // ── Step 3: Push entity deep into wall ────────────────────
    // Entity at x=1.7 (AABB [1.2, 2.2]) inside wall [1.5, 2.5]
    printf("Test 3: Deep overlap fully resolved\n");
    sg.set_position(ent, { 1.7f, 0.0f, 0.0f });
    scene.update_physics(cw, 0.016f);

    pos = sg.world_position(ent);
    printf("  ECS entity X after deep overlap: %.4f\n", pos.x);
    TEST("ECS entity pushed out of wall (X ≤ 1.0)", pos.x <= 1.0f + 0.01f);

    // ── Step 4: Static entity is NOT pushed back ──────────────
    printf("Test 4: Static entity ignores sync_back\n");
    pino::EntityId static_ent = scene.create_entity();
    sg.attach(static_ent);
    sg.set_position(static_ent, { 0.0f, 0.0f, 0.0f });
    pino::PhysicsComponent& spc = scene.add_component<pino::PhysicsComponent>(static_ent);
    spc.local_min = { -0.5f, -0.5f, -0.5f };
    spc.local_max = {  0.5f,  0.5f,  0.5f };
    spc.is_static = true;
    spc.collision_layer = 1;
    spc.collision_mask  = 1;

    sg.set_position(static_ent, { 5.0f, 0.0f, 0.0f });
    scene.update_physics(cw, 0.016f);

    pos = sg.world_position(static_ent);
    printf("  Static ECS entity X: %.4f (should be unchanged)\n", pos.x);
    TEST_NEAR("Static entity stays at x=5", pos.x, 5.0f, 0.01f);

    // ── Step 5: Entity at safe distance NOT moved ─────────────
    printf("Test 5: Non-overlapping entity left unchanged\n");
    sg.set_position(ent, { -3.0f, 0.0f, 0.0f });
    scene.update_physics(cw, 0.016f);

    pos = sg.world_position(ent);
    TEST_NEAR("Far entity stays at x=-3", pos.x, -3.0f, 0.01f);

    // ── Summary ────────────────────────────────────────────
    printf("\n=== Results: %s ===\n", s_failures == 0 ? "ALL PASSED" : "SOME FAILED");
    printf("Failures: %d\n", s_failures);
    return s_failures;
}
