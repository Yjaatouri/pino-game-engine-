#include "math_utils.h"
#include <cmath>
#include <limits>

namespace pino {

static thread_local std::mt19937_64 s_rng(std::random_device{}());

std::mt19937_64& Random::rng() {
    return s_rng;
}

void Random::init(u64 seed) {
    s_rng.seed(seed);
}

void Random::seed(u64 s) {
    s_rng.seed(s);
}

f32 Random::range(f32 min, f32 max) {
    std::uniform_real_distribution<f32> dist(min, max);
    return dist(s_rng);
}

i32 Random::range(i32 min, i32 max) {
    std::uniform_int_distribution<i32> dist(min, max);
    return dist(s_rng);
}

glm::vec3 Random::unit_vector() {
    // Uniform direction on sphere via rejection sampling
    while (true) {
        glm::vec3 v = {
            range(-1.0f, 1.0f),
            range(-1.0f, 1.0f),
            range(-1.0f, 1.0f)
        };
        f32 len_sq = v.x * v.x + v.y * v.y + v.z * v.z;
        if (len_sq > 1e-6f && len_sq <= 1.0f) {
            f32 inv_len = 1.0f / std::sqrt(len_sq);
            return glm::vec3{v.x * inv_len, v.y * inv_len, v.z * inv_len};
        }
    }
}

} // namespace pino
