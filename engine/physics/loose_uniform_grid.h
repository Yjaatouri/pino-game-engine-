#pragma once

#include "engine/physics/uniform_grid.h"

namespace pino {

// Loose Uniform Grid — same as UniformGrid but with a configurable
// looseness factor that multiplies the current cell size.
// Larger cells → fewer insertions but more intra-cell pair tests.
class LooseUniformGrid : public UniformGrid {
public:
    explicit LooseUniformGrid(f32 cell_size = 2.0f, f32 looseness = 3.0f)
        : UniformGrid(cell_size)
        , m_looseness(looseness)
    {
        set_effective_cell_size();
    }

    void set_looseness(f32 l) { m_looseness = l; set_effective_cell_size(); }

    // Override set_cell_size to keep effective size in sync
    void set_cell_size(f32 s) {
        UniformGrid::set_cell_size(s);
        set_effective_cell_size();
    }

    // Override auto_size
    f32 auto_size(const std::vector<AABB>& aabbs, f32 multiplier = 2.0f) {
        f32 base = UniformGrid::auto_size(aabbs, multiplier);
        set_effective_cell_size();
        return base * m_looseness;
    }

private:
    void set_effective_cell_size() {
        UniformGrid::set_cell_size(m_looseness * UniformGrid::cell_size());
    }

    f32 m_looseness;
};

} // namespace pino
