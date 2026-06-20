#pragma once

#include "engine/core/types.h"
#include <glm/glm.hpp>

namespace pino {

class Frustum {
public:
    void extract(const glm::mat4& view_proj);

    // AABB intersection (world-space min/max)
    bool intersects(const glm::vec3& world_min, const glm::vec3& world_max) const;

private:
    struct Plane { glm::vec3 normal; float d; };
    Plane m_planes[6];
};

// Transform local AABB corners by model matrix to get world-space AABB
inline void compute_world_aabb(const glm::vec3& local_min,
                               const glm::vec3& local_max,
                               const glm::mat4& model,
                               glm::vec3& out_min,
                               glm::vec3& out_max) {
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
    out_min = glm::vec3(FLT_MAX);
    out_max = glm::vec3(-FLT_MAX);
    for (int i = 0; i < 8; ++i) {
        glm::vec4 w = model * glm::vec4(corners[i], 1.0f);
        out_min = glm::min(out_min, glm::vec3(w));
        out_max = glm::max(out_max, glm::vec3(w));
    }
}

} // namespace pino
