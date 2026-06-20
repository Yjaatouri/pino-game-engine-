#pragma once

#include "engine/core/types.h"
#include "engine/renderer/mesh.h"
#include <vector>

namespace pino {

class LODGroup {
public:
    struct Level {
        Mesh* mesh;
        float max_distance; // use this level when distance <= max_distance
    };

    void add_level(Mesh* mesh, float max_distance);
    Mesh* get_mesh(float distance) const;

    int level_count() const { return static_cast<int>(m_levels.size()); }
    bool is_valid() const { return !m_levels.empty(); }

private:
    std::vector<Level> m_levels;
};

} // namespace pino
