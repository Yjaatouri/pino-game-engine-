#include "engine/renderer/debug_overlay.h"
#include <cstdio>
#include <cmath>

namespace pino {

DebugOverlay::DebugOverlay() = default;
DebugOverlay::~DebugOverlay() = default;

void DebugOverlay::toggle() { m_visible = !m_visible; }

bool DebugOverlay::handle_input(Input& input) {
    if (input.is_key_just_pressed(Key::F3)) {
        toggle();
        return true;
    }
    return false;
}

void DebugOverlay::set_frame_stats(f32 fps, f32 frame_ms, f32 update_ms, f32 render_ms) {
    m_fps = fps;
    m_frame_ms = frame_ms;
    m_update_ms = update_ms;
    m_render_ms = render_ms;
}

void DebugOverlay::set_render_stats(u32 draw_calls, u32 triangles) {
    m_draw_calls = draw_calls;
    m_triangles = triangles;
}

void DebugOverlay::set_entity_count(u32 count) {
    m_entity_count = count;
}

void DebugOverlay::set_asset_counts(u32 meshes, u32 textures, u32 shaders) {
    m_meshes = meshes;
    m_textures = textures;
    m_shaders = shaders;
}

void DebugOverlay::set_physics_stats(u32 colliders, u64 broad_pairs, u64 overlaps) {
    m_colliders = colliders;
    m_broad_pairs = broad_pairs;
    m_overlaps = overlaps;
}

void DebugOverlay::render(TextRenderer& tr, Font& font, i32 window_w, i32 window_h) {
    (void)window_w;
    (void)window_h;
    if (!m_visible) return;

    const f32 scale = 0.65f;
    const f32 x = 12.0f;
    const f32 line_h = std::ceil(font.line_height() * scale) + 2.0f;

    char buf[128];
    f32 y = 10.0f;

    tr.draw_text(font, "=== PINO DEBUG ===", x, y, scale, 0.3f, 0.9f, 0.3f, 1.0f);
    y += line_h;

    std::snprintf(buf, sizeof(buf), "FPS: %.1f  Frame: %.2f ms", m_fps, m_frame_ms);
    tr.draw_text(font, buf, x, y, scale, 0.9f, 0.9f, 0.9f, 1.0f);
    y += line_h;

    std::snprintf(buf, sizeof(buf), "Update: %.2f ms  Render: %.2f ms", m_update_ms, m_render_ms);
    tr.draw_text(font, buf, x, y, scale, 0.7f, 0.7f, 0.7f, 1.0f);
    y += line_h;

    std::snprintf(buf, sizeof(buf), "Draw: %u  Tris: %u", m_draw_calls, m_triangles);
    tr.draw_text(font, buf, x, y, scale, 0.9f, 0.9f, 0.9f, 1.0f);
    y += line_h;

    if (m_entity_count > 0 || m_colliders > 0) {
        std::snprintf(buf, sizeof(buf), "Entities: %u  Colliders: %u", m_entity_count, m_colliders);
        tr.draw_text(font, buf, x, y, scale, 0.9f, 0.9f, 0.9f, 1.0f);
        y += line_h;
    }

    std::snprintf(buf, sizeof(buf), "Meshes: %u  Tex: %u  Shaders: %u", m_meshes, m_textures, m_shaders);
    tr.draw_text(font, buf, x, y, scale, 0.7f, 0.7f, 0.7f, 1.0f);
    y += line_h;

    if (m_broad_pairs > 0 || m_overlaps > 0) {
        std::snprintf(buf, sizeof(buf), "Broad pairs: %llu  Overlaps: %llu",
                      (unsigned long long)m_broad_pairs,
                      (unsigned long long)m_overlaps);
        tr.draw_text(font, buf, x, y, scale, 0.7f, 0.7f, 0.7f, 1.0f);
        y += line_h;
    }
}

} // namespace pino
