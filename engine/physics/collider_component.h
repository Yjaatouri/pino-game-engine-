#pragma once
#include "engine/core/types.h"
#include "engine/core/transform.h"
#include "engine/core/math_utils.h"
#include "engine/physics/aabb.h"
#include <glm/glm.hpp>
#include <cfloat>

namespace pino {

inline bool rayAABBIntersection(const Ray& ray, const AABB& aabb, f32& out_t, glm::vec3& out_normal) {
    f32 tmin = -FLT_MAX;
    f32 tmax =  FLT_MAX;
    i32 hit_axis = 0;
    f32 hit_sign = 0;

    for (i32 axis = 0; axis < 3; ++axis) {
        f32 inv_dir = 1.0f / ray.direction[axis];
        f32 t0 = (aabb.min[axis] - ray.origin[axis]) * inv_dir;
        f32 t1 = (aabb.max[axis] - ray.origin[axis]) * inv_dir;
        if (inv_dir < 0.0f) std::swap(t0, t1);
        if (t0 > tmin) { tmin = t0; hit_axis = axis; hit_sign = -1.0f; }
        if (t1 < tmax) { tmax = t1; }
        if (tmax < tmin) return false;
    }

    if (tmax < 0.0f) return false;
    out_t = (tmin > 0.0f) ? tmin : tmax;

    // Face normal based on hit axis and direction
    glm::vec3 normals[] = {
        { hit_sign, 0, 0 },
        { 0, hit_sign, 0 },
        { 0, 0, hit_sign }
    };
    // Flip normal if ray is hitting from inside
    bool inside = ray.direction[hit_axis] * hit_sign > 0;
    out_normal = inside ? -normals[hit_axis] : normals[hit_axis];
    if (tmin <= 0.0f) {
        // Ray started inside — use outward normal
        out_normal = -normals[hit_axis];
    }

    return true;
}

struct ColliderComponent {
    glm::vec3 local_min = { -0.5f, -0.5f, -0.5f };
    glm::vec3 local_max = {  0.5f,  0.5f,  0.5f };
    AABB      world_aabb;
    bool      is_static = false;
    bool      enabled   = true;

    // Collision filtering: bitmask-based layer system
    // Entity only collides with another entity if:
    //   (this.collision_mask & other.collision_layer) != 0
    u32 collision_layer = 1;
    u32 collision_mask  = 1;

    void update_world_aabb(const Transform& t) {
        update_world_aabb(t.matrix());
    }

    void update_world_aabb(const glm::mat4& world_matrix) {
        glm::vec3 corners[8] = {
            {local_min.x, local_min.y, local_min.z},
            {local_max.x, local_min.y, local_min.z},
            {local_min.x, local_max.y, local_min.z},
            {local_max.x, local_max.y, local_min.z},
            {local_min.x, local_min.y, local_max.z},
            {local_max.x, local_min.y, local_max.z},
            {local_min.x, local_max.y, local_max.z},
            {local_max.x, local_max.y, local_max.z},
        };

        world_aabb.min = { FLT_MAX, FLT_MAX, FLT_MAX };
        world_aabb.max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
        for (i32 i = 0; i < 8; ++i) {
            glm::vec4 c = world_matrix * glm::vec4(corners[i], 1.0f);
            glm::vec3 wp = glm::vec3(c);
            if (std::isnan(wp.x) || std::isnan(wp.y) || std::isnan(wp.z))
                continue;
            world_aabb.min = glm::min(world_aabb.min, wp);
            world_aabb.max = glm::max(world_aabb.max, wp);
        }
    }

    // Test collision filtering between two colliders
    static bool should_collide(const ColliderComponent& a, const ColliderComponent& b) {
        return (a.collision_mask & b.collision_layer) != 0 &&
               (b.collision_mask & a.collision_layer) != 0;
    }
};

} // namespace pino
