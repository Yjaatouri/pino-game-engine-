#pragma once

#include "engine/core/types.h"
#include <glm/glm.hpp>
#include <random>
#include <cmath>

namespace pino {

// ── Engine-wide precision rules ─────────────────────────────────
namespace Math {
    static constexpr f32 PI      = 3.14159265358979323846f;
    static constexpr f32 TWO_PI  = 6.28318530717958647693f;
    static constexpr f32 HALF_PI = 1.57079632679489661923f;
    static constexpr f32 EPSILON = 1e-6f;
    static constexpr f32 DEG_TO_RAD = PI / 180.0f;
    static constexpr f32 RAD_TO_DEG = 180.0f / PI;

    // Safe comparison utilities
    inline bool equals(f32 a, f32 b, f32 eps = EPSILON) {
        return std::fabs(a - b) < eps;
    }

    inline bool near_zero(f32 v, f32 eps = EPSILON) {
        return std::fabs(v) < eps;
    }

    inline bool near_zero(const glm::vec3& v, f32 eps = EPSILON) {
        return near_zero(v.x, eps) && near_zero(v.y, eps) && near_zero(v.z, eps);
    }

    // Utility functions
    inline f32 clamp(f32 value, f32 min_val, f32 max_val) {
        if (value < min_val) return min_val;
        if (value > max_val) return max_val;
        return value;
    }

    inline f32 lerp(f32 a, f32 b, f32 t) {
        return a + (b - a) * t;
    }

    inline f32 smoothstep(f32 t) {
        return t * t * (3.0f - 2.0f * t);
    }

    inline f32 smootherstep(f32 t) {
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }

    inline f32 remap(f32 value, f32 in_min, f32 in_max, f32 out_min, f32 out_max) {
        f32 t = (value - in_min) / (in_max - in_min);
        return lerp(out_min, out_max, t);
    }

    inline f32 radians(f32 degrees) {
        return degrees * DEG_TO_RAD;
    }

    inline f32 degrees(f32 radians) {
        return radians * RAD_TO_DEG;
    }

    inline f32 wrap(f32 value, f32 range) {
        return value - std::floor(value / range) * range;
    }
} // namespace Math

// ── Ray ──────────────────────────────────────────────────────────
struct Ray {
    glm::vec3 origin;
    glm::vec3 direction; // always normalized

    glm::vec3 at(f32 t) const {
        return origin + direction * t;
    }
};

// Ray-AABB intersection (slab method)
// Returns true if the ray hits the AABB, and sets out_t to the distance.
inline bool rayAABBIntersection(const Ray& ray, const glm::vec3& aabb_min,
                                 const glm::vec3& aabb_max, f32& out_t)
{
    f32 tmin = -FLT_MAX;
    f32 tmax =  FLT_MAX;

    for (i32 axis = 0; axis < 3; ++axis) {
        f32 inv_dir = 1.0f / ray.direction[axis];
        f32 t0 = (aabb_min[axis] - ray.origin[axis]) * inv_dir;
        f32 t1 = (aabb_max[axis] - ray.origin[axis]) * inv_dir;
        if (inv_dir < 0.0f) std::swap(t0, t1);
        tmin = std::max(tmin, t0);
        tmax = std::min(tmax, t1);
        if (tmax < tmin) return false;
    }

    if (tmax < 0.0f) return false;

    out_t = (tmin > 0.0f) ? tmin : tmax;
    return true;
}

// ── Random ───────────────────────────────────────────────────────
class Random {
public:
    // Initialize with a seed (deterministic if same seed given)
    static void init(u64 seed);

    // Re-seed the generator
    static void seed(u64 s);

    // Random float in [min, max]
    static f32 range(f32 min, f32 max);

    // Random integer in [min, max] (inclusive)
    static i32 range(i32 min, i32 max);

    // Random unit-length direction vector (uniform on sphere)
    static glm::vec3 unit_vector();

private:
    static std::mt19937_64& rng();
};

} // namespace pino
