#include "engine/renderer/lod_group.h"
#include <algorithm>

namespace pino {

void LODGroup::add_level(Mesh* mesh, float max_distance) {
    m_levels.push_back({mesh, max_distance});
    std::sort(m_levels.begin(), m_levels.end(),
        [](const Level& a, const Level& b) {
            return a.max_distance < b.max_distance;
        });
}

Mesh* LODGroup::get_mesh(float distance) const {
    if (m_levels.empty()) return nullptr;

    for (const auto& level : m_levels) {
        if (distance <= level.max_distance)
            return level.mesh;
    }
    return m_levels.back().mesh;
}

} // namespace pino
