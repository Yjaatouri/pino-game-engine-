#pragma once

#include "engine/core/types.h"
#include "engine/physics/aabb.h"
#include <vector>
#include <algorithm>

namespace pino {

// Sweep-And-Prune broad-phase.
// Sorts AABBs by min X, then sweeps left-to-right with an active list.
// Uses insertion sort (temporal coherence: O(n) in practice).
class SweepAndPrune {
public:
    // Insert all enabled AABBs and collect candidate pairs.
    // idx_aabb: array of (index, AABB) pairs for enabled colliders.
    void collect_pairs(
        const std::vector<std::pair<u32, AABB>>& idx_aabb,
        std::vector<u64>& out_pairs)
    {
        // Build sorted endpoint array
        m_endpoints.resize(idx_aabb.size());
        for (size_t i = 0; i < idx_aabb.size(); ++i) {
            m_endpoints[i].idx = idx_aabb[i].first;
            m_endpoints[i].min_x = idx_aabb[i].second.min.x;
            m_endpoints[i].max_x = idx_aabb[i].second.max.x;
            m_endpoints[i].aabb = &idx_aabb[i].second;
        }

        // Insertion sort (temporal coherence: mostly sorted between frames)
        for (size_t i = 1; i < m_endpoints.size(); ++i) {
            Endpoint key = m_endpoints[i];
            i64 j = static_cast<i64>(i) - 1;
            while (j >= 0 && m_endpoints[j].min_x > key.min_x) {
                m_endpoints[j + 1] = m_endpoints[j];
                --j;
            }
            m_endpoints[j + 1] = key;
        }

        // Sweep
        m_active.clear();
        out_pairs.clear();

        for (size_t i = 0; i < m_endpoints.size(); ++i) {
            const auto& cur = m_endpoints[i];

            // Remove expired active entries
            for (i64 ai = static_cast<i64>(m_active.size()) - 1; ai >= 0; --ai) {
                if (m_active[ai].max_x < cur.min_x) {
                    m_active[ai] = m_active.back();
                    m_active.pop_back();
                }
            }

            // Test against all active
            for (const auto& act : m_active) {
                if (act.aabb->overlaps(*cur.aabb)) {
                    u32 a = act.idx;
                    u32 b = cur.idx;
                    if (a > b) std::swap(a, b);
                    out_pairs.push_back((static_cast<u64>(a) << 32) | b);
                }
            }

            m_active.push_back(cur);
        }

        // Final sort + unique (SAP can produce duplicates when AABBs start/stop overlapping mid-sweep)
        std::sort(out_pairs.begin(), out_pairs.end());
        auto last = std::unique(out_pairs.begin(), out_pairs.end());
        out_pairs.erase(last, out_pairs.end());
    }

    void clear() {
        m_endpoints.clear();
        m_active.clear();
    }

private:
    struct Endpoint {
        u32 idx;
        f32 min_x, max_x;
        const AABB* aabb;
    };
    std::vector<Endpoint> m_endpoints;
    std::vector<Endpoint> m_active;
};

} // namespace pino
