#pragma once

#include "engine/core/types.h"
#include "engine/physics/aabb.h"
#include <unordered_map>
#include <vector>
#include <glm/glm.hpp>

namespace pino {

// Sparse uniform grid broad-phase.  Each cell stores indices of colliders
// whose AABB overlaps that cell.  Collecting all intra-cell pairs produces
// candidate overlap pairs for narrow-phase testing.
class UniformGrid {
public:
    explicit UniformGrid(f32 cell_size = 2.0f)
        : m_cell_size(cell_size) {}

    void set_cell_size(f32 s) { m_cell_size = s; }

    // Auto-size cells based on average AABB extent across a range.
    // cell_size = extent * multiplier  (extent = average max-extent of all AABBs).
    // Returns the chosen cell size.
    f32 auto_size(const std::vector<AABB>& aabbs, f32 multiplier = 2.0f) {
        if (aabbs.empty()) return m_cell_size;
        f32 sum = 0.0f;
        u32 count = 0;
        for (const auto& aabb : aabbs) {
            glm::vec3 ext = aabb.max - aabb.min;
            sum += (std::max)({ext.x, ext.y, ext.z});
            ++count;
        }
        m_cell_size = (sum / static_cast<f32>(count)) * multiplier;
        if (m_cell_size < 0.1f) m_cell_size = 0.1f;
        return m_cell_size;
    }

    // Total number of (index, cell) insertions since last clear.
    u64 total_insertions() const { return m_total_insertions; }

    // Read-only access to cell map (for diagnostics).
    const std::unordered_map<u64, std::vector<u32>>& cells() const { return m_cells; }

    // Clear all cells (preserves vector capacities).
    void clear() {
        for (auto& kv : m_cells)
            kv.second.clear();
        m_total_insertions = 0;
    }

    // Insert index idx into every cell its AABB covers.
    void insert(u32 idx, const AABB& aabb) {
        i32 min_x = cell_idx(aabb.min.x);
        i32 min_y = cell_idx(aabb.min.y);
        i32 min_z = cell_idx(aabb.min.z);
        i32 max_x = cell_idx(aabb.max.x);
        i32 max_y = cell_idx(aabb.max.y);
        i32 max_z = cell_idx(aabb.max.z);

        for (i32 z = min_z; z <= max_z; ++z) {
            for (i32 y = min_y; y <= max_y; ++y) {
                for (i32 x = min_x; x <= max_x; ++x) {
                    u64 key = cell_key(x, y, z);
                    m_cells[key].push_back(idx);
                    ++m_total_insertions;
                }
            }
        }
    }

    // Collect unique pair IDs (pack(i,j) with i<j) from every cell.
    // Caller must sort+unique the result.
    void collect_pairs(std::vector<u64>& out_pairs) const {
        for (const auto& kv : m_cells) {
            const auto& indices = kv.second;
            for (size_t i = 0; i < indices.size(); ++i) {
                for (size_t j = i + 1; j < indices.size(); ++j) {
                    u32 a = indices[i];
                    u32 b = indices[j];
                    if (a > b) std::swap(a, b);
                    out_pairs.push_back((static_cast<u64>(a) << 32) | b);
                }
            }
        }
    }

    f32 cell_size() const { return m_cell_size; }

    // ── Debug visualization ─────────────────────────────────────
    // Decode a cell key back to cell coordinates.
    static void decode_cell_key(u64 key, i32& x, i32& y, i32& z) {
        x = static_cast<i32>(static_cast<u32>(key & 0x1FFFFF));
        y = static_cast<i32>(static_cast<u32>((key >> 21) & 0x1FFFFF));
        z = static_cast<i32>(static_cast<u32>((key >> 42) & 0x1FFFFF));
        if (x & 0x100000) x |= ~0x1FFFFF;
        if (y & 0x100000) y |= ~0x1FFFFF;
        if (z & 0x100000) z |= ~0x1FFFFF;
    }

    // Compute the AABB for a cell given its key and the grid's cell size.
    static AABB cell_aabb(u64 key, f32 cell_size) {
        i32 cx, cy, cz;
        decode_cell_key(key, cx, cy, cz);
        glm::vec3 min(static_cast<f32>(cx) * cell_size,
                      static_cast<f32>(cy) * cell_size,
                      static_cast<f32>(cz) * cell_size);
        return AABB(min, min + glm::vec3(cell_size));
    }

    AABB cell_aabb(u64 key) const { return cell_aabb(key, m_cell_size); }

    // Collect AABBs for all active cells (for debug rendering).
    void collect_cell_aabbs(std::vector<AABB>& out) const {
        out.clear();
        out.reserve(m_cells.size());
        for (const auto& kv : m_cells) {
            out.push_back(cell_aabb(kv.first, m_cell_size));
        }
    }

private:
    i32 cell_idx(f32 coord) const {
        return static_cast<i32>(std::floor(coord / m_cell_size));
    }

    static u64 cell_key(i32 x, i32 y, i32 z) {
        // Pack 21-bit signed coords into 64 bits (reversible).
        return (static_cast<u64>(static_cast<u32>(x) & 0x1FFFFF)) |
               ((static_cast<u64>(static_cast<u32>(y) & 0x1FFFFF)) << 21) |
               ((static_cast<u64>(static_cast<u32>(z) & 0x1FFFFF)) << 42);
    }

    f32 m_cell_size;
    u64 m_total_insertions = 0;
    std::unordered_map<u64, std::vector<u32>> m_cells;
};

} // namespace pino
