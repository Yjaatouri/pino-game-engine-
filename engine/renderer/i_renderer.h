#pragma once

#include "engine/core/types.h"

namespace pino {

// ── Abstracts a single uniform or buffer binding point ──────────
enum class ShaderHandle  : u32 { Invalid = 0 };
enum class BufferHandle  : u32 { Invalid = 0 };
enum class TextureHandle : u32 { Invalid = 0 };

enum class BufferUsage {
    Static,
    Dynamic,
    Stream,
};

enum class TextureFormat {
    R8,
    RGBA8,
    Depth24,
};

enum class CullMode {
    None,
    Front,
    Back,
};

struct ClearFlags {
    bool color   = true;
    bool depth   = true;
    bool stencil = false;
};

// ── Renderer capabilities queried at init ───────────────────────
struct RendererCapabilities {
    const char* api_name          = "Unknown";
    bool        supports_shadows  = false;
    bool        supports_instancing = false;
    u32         max_texture_size  = 4096;
    u32         max_texture_units = 16;
    u32         max_lights        = 8;
};

// ── Abstract renderer interface ─────────────────────────────────
// Implementations: OpenGLES3Renderer (current), MetalRenderer (future)
class IRenderer {
public:
    virtual ~IRenderer() = default;

    // Lifecycle
    virtual bool init(void* native_window) = 0;
    virtual void shutdown() = 0;

    // Per-frame
    virtual void begin_frame() = 0;
    virtual void end_frame() = 0;

    // Capabilities
    virtual const RendererCapabilities& capabilities() const = 0;
    virtual const char* name() const = 0;

    // ── Shaders ────────────────────────────────────────────────
    virtual ShaderHandle create_shader(const char* vert_src,
                                       const char* frag_src) = 0;
    virtual void destroy_shader(ShaderHandle sh) = 0;
    virtual void bind_shader(ShaderHandle sh) = 0;

    // Uniforms (mapping to Metal's argument buffers)
    virtual void set_uniform_int(ShaderHandle sh, const char* name, i32 v) = 0;
    virtual void set_uniform_float(ShaderHandle sh, const char* name, f32 v) = 0;
    virtual void set_uniform_vec3(ShaderHandle sh, const char* name, const f32* v) = 0;
    virtual void set_uniform_vec4(ShaderHandle sh, const char* name, const f32* v) = 0;
    virtual void set_uniform_mat4(ShaderHandle sh, const char* name, const f32* m) = 0;

    // ── Buffers ────────────────────────────────────────────────
    virtual BufferHandle create_vertex_buffer(const void* data, u32 size,
                                              BufferUsage usage) = 0;
    virtual BufferHandle create_index_buffer(const void* data, u32 size,
                                             BufferUsage usage) = 0;
    virtual void destroy_buffer(BufferHandle buf) = 0;
    virtual void update_buffer(BufferHandle buf, const void* data,
                               u32 offset, u32 size) = 0;
    virtual void bind_vertex_buffer(BufferHandle buf, u32 binding, u32 stride) = 0;
    virtual void bind_index_buffer(BufferHandle buf) = 0;

    // ── Textures ───────────────────────────────────────────────
    virtual TextureHandle create_texture(u32 width, u32 height,
                                         TextureFormat fmt,
                                         const void* data) = 0;
    virtual void destroy_texture(TextureHandle tex) = 0;
    virtual void bind_texture(TextureHandle tex, u32 slot) = 0;

    // ── Draw calls ─────────────────────────────────────────────
    virtual void draw(u32 vertex_count, u32 instance_count = 1) = 0;
    virtual void draw_indexed(u32 index_count, u32 instance_count = 1) = 0;

    // ── State ──────────────────────────────────────────────────
    virtual void set_viewport(i32 x, i32 y, u32 w, u32 h) = 0;
    virtual void set_depth_test(bool enable) = 0;
    virtual void set_cull_mode(CullMode mode) = 0;
    virtual void clear(const ClearFlags& flags, const f32* color = nullptr) = 0;
};

} // namespace pino
