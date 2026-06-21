#pragma once

#include "engine/core/types.h"
#include <chrono>

namespace pino {

// Per-frame collision profiling data.
struct CollisionStats {
    // Phase timings (microseconds)
    f64 aabb_update_us      = 0.0;
    f64 broad_phase_us      = 0.0;
    f64 narrow_phase_us     = 0.0;
    f64 event_dispatch_us   = 0.0;
    f64 resolution_us       = 0.0;
    f64 total_us            = 0.0;

    // Grid diagnostics (populated when UniformGrid is active)
    u64 candidate_pairs_total   = 0;   // before dedup
    u64 candidate_pairs_unique  = 0;   // after dedup
    u64 actual_overlaps         = 0;   // after narrow-phase
    u64 cells_touched_total     = 0;   // sum of cells per collider
    f64 avg_colliders_per_cell  = 0.0f;
    u32 max_colliders_per_cell  = 0;
    f64 avg_cells_per_collider  = 0.0f;
    u32 active_cell_count       = 0;

    void reset() {
        aabb_update_us = broad_phase_us = narrow_phase_us = 0.0;
        event_dispatch_us = resolution_us = total_us = 0.0;
        candidate_pairs_total = candidate_pairs_unique = 0;
        actual_overlaps = cells_touched_total = 0;
        avg_colliders_per_cell = max_colliders_per_cell = 0;
        avg_cells_per_collider = 0.0f;
        active_cell_count = 0;
    }
};

// Simple scoped timer (nanosecond precision).
class ScopedTimer {
public:
    ScopedTimer(f64& out_us) : m_out(out_us), m_start(Clock::now()) {}
    ~ScopedTimer() {
        auto end = Clock::now();
        m_out = static_cast<f64>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - m_start).count()) / 1000.0;
    }
private:
    using Clock = std::chrono::high_resolution_clock;
    f64& m_out;
    Clock::time_point m_start;
};

} // namespace pino
