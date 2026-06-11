#pragma once
#include "engine/core/types.h"
#include "engine/core/transform.h"
#include <glm/glm.hpp>
#include <cmath>

namespace pino {

struct AABB {
    glm::vec3 min = {0,0,0};
    glm::vec3 max = {0,0,0};

    AABB() = default;
    AABB(const glm::vec3& mn, const glm::vec3& mx) : min(mn), max(mx) {}

    static AABB from_center_extents(const glm::vec3& center, const glm::vec3& half_extents) {
        return { center - half_extents, center + half_extents };
    }

    static AABB from_transform(const Transform& t, const glm::vec3& mesh_half_extents) {
        glm::vec3 s = t.scale * mesh_half_extents;
        return { t.position - s, t.position + s };
    }

    glm::vec3 center() const { return (min + max) * 0.5f; }
    glm::vec3 extents() const { return (max - min) * 0.5f; }

    bool overlaps(const AABB& other) const {
        return (min.x <= other.max.x && max.x >= other.min.x) &&
               (min.y <= other.max.y && max.y >= other.min.y) &&
               (min.z <= other.max.z && max.z >= other.min.z);
    }

    // Minimum Translation Vector — pushes `this` out of `other`.
    // Uses direct overlap comparison on the minimum-penetration axis
    // for deterministic, stable push-out independent of center alignment.
    glm::vec3 push_out(const AABB& other, f32 epsilon = 1e-6f) const {
        f32 ox = std::fmin(max.x - other.min.x, other.max.x - min.x);
        f32 oy = std::fmin(max.y - other.min.y, other.max.y - min.y);
        f32 oz = std::fmin(max.z - other.min.z, other.max.z - min.z);

        i32 axis = 0;
        f32 min_ov = ox;
        if (oy < min_ov) { min_ov = oy; axis = 1; }
        if (oz < min_ov) { min_ov = oz; axis = 2; }

        glm::vec3 result = {0,0,0};
        if (axis == 0) {
            f32 right = max.x - other.min.x;
            f32 left  = other.max.x - min.x;
            result.x = (right < left - epsilon) ? right : -left;
        } else if (axis == 1) {
            f32 up   = max.y - other.min.y;
            f32 down = other.max.y - min.y;
            result.y = (up < down - epsilon) ? up : -down;
        } else {
            f32 forward = max.z - other.min.z;
            f32 back    = other.max.z - min.z;
            result.z = (forward < back - epsilon) ? forward : -back;
        }
        return result;
    }

    // Check if this AABB contains a point
    bool contains(const glm::vec3& point) const {
        return point.x >= min.x && point.x <= max.x &&
               point.y >= min.y && point.y <= max.y &&
               point.z >= min.z && point.z <= max.z;
    }
};

} // namespace pino
