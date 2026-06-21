#include "engine/debug/profiler_overlay.h"

#include <chrono>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cmath>

namespace pino {

// ─── Built-in zone names ──────────────────────────────────────────
static const char* const s_builtin_names[ProfilerZone_COUNT] = {
    "Total Frame",
    "Begin Frame",
    "Audio",
    "ECS Update",
    "Physics: AABB",
    "Physics: Broad",
    "Physics: Narrow",
    "Physics: Dispatch",
    "Physics: Resolve",
    "Render",
};

// ─── Timer ────────────────────────────────────────────────────────

u64 ProfilerOverlay::now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
}

// ─── Constructor / Destructor ─────────────────────────────────────

ProfilerOverlay::ProfilerOverlay() {
    // Register built-in zones
    for (u32 i = 0; i < ProfilerZone_COUNT; ++i) {
        auto& z = m_internal[i];
        z.name = s_builtin_names[i];
        m_zones[i].name = s_builtin_names[i];
    }
    m_zone_count = ProfilerZone_COUNT;
}

ProfilerOverlay::~ProfilerOverlay() = default;

// ─── Toggle ───────────────────────────────────────────────────────

void ProfilerOverlay::toggle() {
    m_visible = !m_visible;
}

bool ProfilerOverlay::handle_input(Input& input) {
    if (input.is_key_just_pressed(m_toggle_key)) {
        toggle();
        return true;
    }
    return false;
}

// ─── Zone registration ───────────────────────────────────────────

u32 ProfilerOverlay::register_zone(const char* name) {
    for (u32 i = ProfilerZone_COUNT; i < m_zone_count; ++i) {
        if (std::strcmp(m_internal[i].name, name) == 0)
            return i;
    }
    if (m_zone_count >= MAX_ZONES) return UINT32_MAX;

    u32 id = m_zone_count++;
    m_internal[id].name = name;
    m_zones[id].name = name;
    return id;
}

u32 ProfilerOverlay::zone_id_by_name(const char* name) const {
    for (u32 i = 0; i < m_zone_count; ++i) {
        if (m_internal[i].name && std::strcmp(m_internal[i].name, name) == 0)
            return i;
    }
    return UINT32_MAX;
}

// ─── Timing ───────────────────────────────────────────────────────

void ProfilerOverlay::begin(u32 zone_id) {
    if (zone_id >= m_zone_count) return;
    m_internal[zone_id].start_tick = now_ns();
}

void ProfilerOverlay::end(u32 zone_id) {
    if (zone_id >= m_zone_count) return;
    auto& z = m_internal[zone_id];
    u64 end = now_ns();
    f64 elapsed = static_cast<f64>(end - z.start_tick) / 1000.0;
    if (elapsed < 0.0) elapsed = 0.0;
    z.elapsed_us += elapsed;
    z.call_count++;
}

void ProfilerOverlay::set_elapsed(u32 zone_id, f64 us, u32 call_count) {
    if (zone_id >= m_zone_count) return;
    auto& z = m_internal[zone_id];
    z.elapsed_us = us;
    z.call_count = call_count;
}

void ProfilerOverlay::feed_physics_stats(const CollisionStats& stats) {
    set_elapsed(ProfilerZone_PhysicsAabb,    stats.aabb_update_us);
    set_elapsed(ProfilerZone_PhysicsBroad,    stats.broad_phase_us);
    set_elapsed(ProfilerZone_PhysicsNarrow,   stats.narrow_phase_us);
    set_elapsed(ProfilerZone_PhysicsDispatch, stats.event_dispatch_us);
    set_elapsed(ProfilerZone_PhysicsResolve,  stats.resolution_us);
}

void ProfilerOverlay::end_frame() {
    for (u32 i = 0; i < m_zone_count; ++i) {
        auto& z = m_internal[i];

        // Store sample in circular buffer (records 0 if zone was idle)
        z.samples[z.sample_idx] = z.elapsed_us;
        z.sample_idx = (z.sample_idx + 1) % WINDOW;
        if (z.sample_count < WINDOW) z.sample_count++;

        // Rolling average over available window
        double sum = 0.0;
        for (u32 j = 0; j < z.sample_count; ++j)
            sum += z.samples[j];
        z.rolling_avg_us = sum / static_cast<double>(z.sample_count);

        // Min/max: skip frames where zone was idle (elapsed_us == 0 && call_count == 0)
        if (z.call_count > 0) {
            if (z.elapsed_us < z.min_us) z.min_us = z.elapsed_us;
            if (z.elapsed_us > z.max_us) z.max_us = z.elapsed_us;
        }

        // Public snapshot
        m_zones[i].elapsed_us     = z.elapsed_us;
        m_zones[i].rolling_avg_us = z.rolling_avg_us;
        m_zones[i].min_us         = (z.min_us > 1e17) ? 0.0 : z.min_us;
        m_zones[i].max_us         = (z.max_us < -1e17) ? 0.0 : z.max_us;
        m_zones[i].call_count     = z.call_count;
        m_zones[i].sample_count   = z.sample_count;

        // Reset for next frame
        z.elapsed_us = 0.0;
        z.call_count = 0;
    }

    m_has_data = true;
}

// ─── Rendering ─────────────────────────────────────────────────────

void ProfilerOverlay::render(TextRenderer& tr, Font& font, i32 window_w, i32 window_h) {
    (void)window_w; (void)window_h;
    if (!m_visible || !m_has_data) return;

    const f32 scale = 0.65f;
    const f32 line_h = std::ceil(font.line_height() * scale) + 2.0f;
    const f32 x = 12.0f;
    f32 y = 10.0f;
    const f32 frame_budget_us = 16666.667f; // 16.667ms in microseconds

    // Title
    tr.draw_text(font, "=== PROFILER ===", x, y, scale, 0.3f, 0.6f, 1.0f, 1.0f);
    y += line_h;
    tr.draw_text(font, "Zone                  Current    Avg       Min       Max       Calls",
                 x, y, scale, 0.5f, 0.5f, 0.5f, 1.0f);
    y += line_h;

    // Build sorted indices by current elapsed (descending), skip zones with 0 time
    u32 sorted_count = 0;
    for (u32 i = 0; i < m_zone_count; ++i) {
        if (m_internal[i].sample_count > 0 || m_zones[i].elapsed_us > 0.0) {
            m_sorted[sorted_count++] = i;
        }
    }
    std::sort(m_sorted, m_sorted + sorted_count,
              [this](u32 a, u32 b) {
                  return m_zones[a].elapsed_us > m_zones[b].elapsed_us;
              });

    u32 display_count = std::min(sorted_count, 10u);
    for (u32 si = 0; si < display_count; ++si) {
        u32 i = m_sorted[si];
        const auto& zd = m_zones[i];

        // Compute color based on percentage of frame budget
        f32 pct = static_cast<f32>(zd.elapsed_us / frame_budget_us);
        f32 r, g, b;
        if (pct < 0.33f) {
            r = 0.3f; g = 1.0f; b = 0.3f; // green
        } else if (pct < 0.66f) {
            r = 1.0f; g = 0.9f; b = 0.3f; // yellow
        } else {
            r = 1.0f; g = 0.3f; b = 0.3f; // red
        }

        char buf[128];
        std::snprintf(buf, sizeof(buf), "%-20s %8.1f  %8.1f  %8.1f  %8.1f  %u",
                      zd.name ? zd.name : "?",
                      zd.elapsed_us,
                      zd.rolling_avg_us,
                      zd.min_us,
                      zd.max_us,
                      zd.call_count);

        tr.draw_text(font, buf, x, y, scale, r, g, b, 1.0f);
        y += line_h;
    }

    // Legend
    y += line_h;
    tr.draw_text(font, "Color: green < 33% frame  yellow < 66%  red >= 66%",
                 x, y, scale, 0.5f, 0.5f, 0.5f, 1.0f);
    y += line_h;
    tr.draw_text(font, "Window: 60 frames  |  F2: toggle",
                 x, y, scale, 0.5f, 0.5f, 0.5f, 1.0f);
}

} // namespace pino
