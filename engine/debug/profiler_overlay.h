#pragma once

#include "engine/core/types.h"
#include "engine/renderer/font.h"
#include "engine/renderer/text_renderer.h"
#include "engine/platform/input.h"
#include "engine/physics/collision_stats.h"

namespace pino {

// ─── Built-in zone IDs that Engine uses ──────────────────────────
enum ProfilerZone : u32 {
    ProfilerZone_TotalFrame = 0,
    ProfilerZone_BeginFrame,
    ProfilerZone_Audio,
    ProfilerZone_EcsUpdate,
    ProfilerZone_PhysicsAabb,
    ProfilerZone_PhysicsBroad,
    ProfilerZone_PhysicsNarrow,
    ProfilerZone_PhysicsDispatch,
    ProfilerZone_PhysicsResolve,
    ProfilerZone_Render,
    ProfilerZone_COUNT,
};

class ProfilerOverlay {
public:
    static constexpr u32 WINDOW = 60;
    static constexpr u32 MAX_ZONES = 32;

    struct ZoneData {
        const char* name       = nullptr;
        f64         elapsed_us = 0;
        f64         rolling_avg_us = 0;
        f64         min_us     = 0;
        f64         max_us     = 0;
        u32         call_count = 0;
        u32         sample_count = 0;
    };

    ProfilerOverlay();
    ~ProfilerOverlay();

    void toggle();
    bool is_visible() const { return m_visible; }
    bool handle_input(Input& input);
    void set_toggle_key(Key k) { m_toggle_key = k; }

    // Register a new zone (returns zone index, must be < MAX_ZONES)
    u32 register_zone(const char* name);

    // Timing
    void begin(u32 zone_id);
    void end(u32 zone_id);

    // Direct set for externally-measured zones
    void set_elapsed(u32 zone_id, f64 us, u32 call_count = 1);

    // Feed physics sub-timings from CollisionWorld::stats
    void feed_physics_stats(const CollisionStats& stats);

    // Finalize all zones for the frame (computes rolling stats)
    void end_frame();

    // Access
    u32           zone_count() const { return m_zone_count; }
    const ZoneData& zone(u32 idx) const { return m_zones[idx]; }
    u32           zone_id_by_name(const char* name) const;
    bool          has_data() const { return m_has_data; }

    void render(TextRenderer& tr, Font& font, i32 window_w, i32 window_h);

private:
    struct InternalZone {
        const char* name         = nullptr;
        u64         start_tick   = 0;
        f64         elapsed_us   = 0;
        f64         samples[WINDOW] = {};
        u32         sample_idx   = 0;
        f64         rolling_avg_us = 0;
        f64         min_us       = 1e18;
        f64         max_us       = -1e18;
        u32         sample_count = 0;
        u32         call_count   = 0;
    };

    static u64 now_ns();

    bool m_visible     = false;
    bool m_has_data    = false;
    Key  m_toggle_key  = Key::F2;

    InternalZone m_internal[MAX_ZONES];
    ZoneData     m_zones[MAX_ZONES];
    u32          m_zone_count = 0;

    // Sorted indices for top-N display
    u32 m_sorted[MAX_ZONES];
};

// ─── RAII profile zone helper ────────────────────────────────────
class ScopedProfileZone {
public:
    ScopedProfileZone(ProfilerOverlay& profiler, u32 zone_id)
        : m_profiler(profiler), m_zone_id(zone_id)
    {
        m_profiler.begin(m_zone_id);
    }
    ~ScopedProfileZone() {
        m_profiler.end(m_zone_id);
    }
    ScopedProfileZone(const ScopedProfileZone&) = delete;
    ScopedProfileZone& operator=(const ScopedProfileZone&) = delete;
private:
    ProfilerOverlay& m_profiler;
    u32 m_zone_id;
};

} // namespace pino
