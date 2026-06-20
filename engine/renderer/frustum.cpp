#include "engine/renderer/frustum.h"
#include <glm/gtc/matrix_access.hpp>

namespace pino {

void Frustum::extract(const glm::mat4& vp) {
    // Left   = column3 + column0
    // Right  = column3 - column0
    // Bottom = column3 + column1
    // Top    = column3 - column1
    // Near   = column3 + column2
    // Far    = column3 - column2

    auto c0 = glm::column(vp, 0);
    auto c1 = glm::column(vp, 1);
    auto c2 = glm::column(vp, 2);
    auto c3 = glm::column(vp, 3);

    auto extract_plane = [](glm::vec4 p) -> Plane {
        float len = glm::length(glm::vec3(p));
        return { glm::vec3(p) / len, p.w / len };
    };

    m_planes[0] = extract_plane(c3 + c0);  // left
    m_planes[1] = extract_plane(c3 - c0);  // right
    m_planes[2] = extract_plane(c3 + c1);  // bottom
    m_planes[3] = extract_plane(c3 - c1);  // top
    m_planes[4] = extract_plane(c3 + c2);  // near
    m_planes[5] = extract_plane(c3 - c2);  // far
}

bool Frustum::intersects(const glm::vec3& world_min,
                         const glm::vec3& world_max) const {
    for (int i = 0; i < 6; ++i) {
        const Plane& p = m_planes[i];

        // Positive vertex — the corner most in the direction of the normal
        glm::vec3 pv = world_min;
        if (p.normal.x >= 0) pv.x = world_max.x;
        if (p.normal.y >= 0) pv.y = world_max.y;
        if (p.normal.z >= 0) pv.z = world_max.z;

        if (glm::dot(p.normal, pv) + p.d < 0)
            return false; // fully outside this plane
    }
    return true;
}

} // namespace pino
