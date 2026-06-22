#pragma once

#include "engine/core/types.h"
#include "engine/renderer/font.h"
#include "engine/renderer/text_renderer.h"
#include "engine/platform/input.h"

namespace pino {

class DebugOverlay {
public:
    DebugOverlay();
    ~DebugOverlay();

    DebugOverlay(const DebugOverlay&) = delete;
    DebugOverlay& operator=(const DebugOverlay&) = delete;

    void toggle();
    bool is_visible() const { return m_visible; }
    bool handle_input(Input& input);

    void set_frame_stats(f32 fps, f32 frame_ms, f32 update_ms, f32 render_ms);
    void set_render_stats(u32 draw_calls, u32 triangles);
    void set_uniform_stats(u32 uniform_calls);
    void set_entity_count(u32 count);
    void set_asset_counts(u32 meshes, u32 textures, u32 shaders);
    void set_physics_stats(u32 colliders, u64 broad_pairs, u64 overlaps);

    void render(TextRenderer& tr, Font& font, i32 window_w, i32 window_h);

private:
    bool m_visible = false;

    f32 m_fps = 0;
    f32 m_frame_ms = 0;
    f32 m_update_ms = 0;
    f32 m_render_ms = 0;
    u32 m_draw_calls = 0;
    u32 m_triangles = 0;
    u32 m_uniform_calls = 0;
    u32 m_entity_count = 0;
    u32 m_meshes = 0;
    u32 m_textures = 0;
    u32 m_shaders = 0;
    u32 m_colliders = 0;
    u64 m_broad_pairs = 0;
    u64 m_overlaps = 0;
};

} // namespace pino
