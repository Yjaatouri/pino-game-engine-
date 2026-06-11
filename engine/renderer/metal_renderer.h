#pragma once

#include "engine/renderer/i_renderer.h"

namespace pino {

// ── MetalRenderer stub ──────────────────────────────────────────
// No functional rendering — defines mapping points for future Metal
// migration. Each method documents the Metal API mapping.
class MetalRenderer final : public IRenderer {
public:
    MetalRenderer();
    ~MetalRenderer() override;

    MetalRenderer(const MetalRenderer&) = delete;
    MetalRenderer& operator=(const MetalRenderer&) = delete;

    // Lifecycle
    bool init(void* native_window) override;
    void shutdown() override;

    // Per-frame
    void begin_frame() override;
    void end_frame() override;

    // Capabilities
    const RendererCapabilities& capabilities() const override;
    const char* name() const override { return "MetalRenderer (stub)"; }

    // ── Shaders ────────────────────────────────────────────────
    ShaderHandle create_shader(const char* vert_src,
                               const char* frag_src) override;
    void destroy_shader(ShaderHandle sh) override;
    void bind_shader(ShaderHandle sh) override;

    void set_uniform_int(ShaderHandle sh, const char* name, i32 v) override;
    void set_uniform_float(ShaderHandle sh, const char* name, f32 v) override;
    void set_uniform_vec3(ShaderHandle sh, const char* name, const f32* v) override;
    void set_uniform_vec4(ShaderHandle sh, const char* name, const f32* v) override;
    void set_uniform_mat4(ShaderHandle sh, const char* name, const f32* m) override;

    // ── Buffers ────────────────────────────────────────────────
    BufferHandle create_vertex_buffer(const void* data, u32 size,
                                      BufferUsage usage) override;
    BufferHandle create_index_buffer(const void* data, u32 size,
                                     BufferUsage usage) override;
    void destroy_buffer(BufferHandle buf) override;
    void update_buffer(BufferHandle buf, const void* data,
                       u32 offset, u32 size) override;
    void bind_vertex_buffer(BufferHandle buf, u32 binding, u32 stride) override;
    void bind_index_buffer(BufferHandle buf) override;

    // ── Textures ───────────────────────────────────────────────
    TextureHandle create_texture(u32 width, u32 height,
                                 TextureFormat fmt,
                                 const void* data) override;
    void destroy_texture(TextureHandle tex) override;
    void bind_texture(TextureHandle tex, u32 slot) override;

    // ── Draw calls ─────────────────────────────────────────────
    void draw(u32 vertex_count, u32 instance_count = 1) override;
    void draw_indexed(u32 index_count, u32 instance_count = 1) override;

    // ── State ──────────────────────────────────────────────────
    void set_viewport(i32 x, i32 y, u32 w, u32 h) override;
    void set_depth_test(bool enable) override;
    void set_cull_mode(CullMode mode) override;
    void clear(const ClearFlags& flags, const f32* color = nullptr) override;

private:
    RendererCapabilities m_caps;
    bool m_initialized = false;

    // Stub handle tracking (placeholder for Metal resource references)
    u32 m_next_shader_handle  = 1;
    u32 m_next_buffer_handle  = 1;
    u32 m_next_texture_handle = 1;
};

} // namespace pino
