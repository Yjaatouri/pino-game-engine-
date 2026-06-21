#include "engine/physics/physics_debug_draw.h"
#include "engine/ecs/ecs_scene.h"
#include "engine/ecs/components.h"
#include <glm/gtc/matrix_transform.hpp>

namespace pino {

PhysicsDebugDraw::PhysicsDebugDraw() = default;

bool PhysicsDebugDraw::handle_input(Input& input) {
    bool changed = false;
    if (input.is_key_just_pressed(Key::F5)) { toggle_aabbs(); changed = true; }
    if (input.is_key_just_pressed(Key::F6)) { toggle_pairs(); changed = true; }
    if (input.is_key_just_pressed(Key::F7)) { toggle_grid();  changed = true; }
    if (input.is_key_just_pressed(Key::F8)) { toggle_vel();   changed = true; }
    return changed;
}

void PhysicsDebugDraw::render(CollisionWorld& cw, DebugRenderer& dr,
                               const glm::mat4& view_proj,
                               EcsScene* scene) {
    if (m_show_aabbs)  draw_aabbs(cw, dr, view_proj);
    if (m_show_pairs)  draw_pairs(cw, dr, view_proj);
    if (m_show_grid)   draw_grid(cw, dr, view_proj);
    if (m_show_vel)    draw_velocities(dr, view_proj, scene);
}

void PhysicsDebugDraw::draw_aabbs(CollisionWorld& cw, DebugRenderer& dr,
                                   const glm::mat4& view_proj) {
    (void)view_proj;
    std::vector<AABB> aabbs;
    cw.collect_collider_aabbs(aabbs);
    for (const auto& aabb : aabbs) {
        glm::vec3 half = aabb.extents();
        glm::vec3 center = aabb.center();
        dr.draw_box(center, half, {0.0f, 1.0f, 0.0f, 1.0f});
    }
}

void PhysicsDebugDraw::draw_pairs(CollisionWorld& cw, DebugRenderer& dr,
                                   const glm::mat4& view_proj) {
    (void)view_proj;
    const auto& pairs = cw.overlapping_pairs();
    for (u64 pair_id : pairs) {
        u32 i = static_cast<u32>(pair_id >> 32);
        u32 j = static_cast<u32>(pair_id & 0xFFFFFFFF);
        if (i >= cw.collider_count() || j >= cw.collider_count()) continue;

        glm::vec3 ca = cw.collider_aabb(i).center();
        glm::vec3 cb = cw.collider_aabb(j).center();

        dr.draw_line(ca, cb, {1.0f, 1.0f, 0.0f, 0.9f});

        // Small sphere at each pair endpoint
        dr.draw_sphere(ca, 0.05f, {1.0f, 0.5f, 0.0f, 1.0f});
        dr.draw_sphere(cb, 0.05f, {1.0f, 0.5f, 0.0f, 1.0f});
    }
}

void PhysicsDebugDraw::draw_grid(CollisionWorld& cw, DebugRenderer& dr,
                                  const glm::mat4& view_proj) {
    (void)view_proj;
    std::vector<AABB> cells;
    cw.collect_grid_cells(cells);
    for (const auto& cell : cells) {
        glm::vec3 half = cell.extents();
        glm::vec3 center = cell.center();
        dr.draw_box(center, half, {0.3f, 0.5f, 0.8f, 0.4f});
    }
}

void PhysicsDebugDraw::draw_velocities(DebugRenderer& dr,
                                        const glm::mat4& view_proj,
                                        EcsScene* scene) {
    (void)view_proj;
    if (!scene) return;

    auto& sg = scene->scene_graph();
    scene->physics_components().each([&](EntityId e, PhysicsComponent& pc) {
        if (!sg.has(e)) return;
        glm::vec3 vel = pc.velocity;
        f32 speed = glm::length(vel);
        if (speed < 0.001f) return;

        glm::vec3 origin = sg.world_position(e);
        glm::vec3 tip = origin + vel;

        f32 t = glm::clamp(speed / 10.0f, 0.0f, 1.0f);
        dr.draw_line(origin, tip, {t, 1.0f - t, 0.0f, 1.0f});

        glm::vec3 dir = glm::normalize(vel);
        f32 arrow_len = 0.15f;
        glm::vec3 perp = glm::abs(dir.x) < 0.9f
                         ? glm::cross(dir, glm::vec3(1,0,0))
                         : glm::cross(dir, glm::vec3(0,1,0));
        perp = glm::normalize(perp) * arrow_len;
        dr.draw_line(tip, tip - dir * arrow_len + perp, {t, 1.0f - t, 0.0f, 1.0f});
        dr.draw_line(tip, tip - dir * arrow_len - perp, {t, 1.0f - t, 0.0f, 1.0f});
    });
}

} // namespace pino
