#pragma once

#include "engine/core/types.h"
#include "engine/renderer/gl_es3.h"

namespace pino {

struct CookedTextureData;  // defined in cooked_asset.h

class Texture {
public:
    Texture() = default;
    ~Texture();

    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    // Upload raw RGBA pixels to the GPU (2D texture)
    void upload_rgba(const u8* pixels, i32 width, i32 height);

    // Upload from CookedTextureData (supports mip chains and compressed formats)
    void upload_cooked(const CookedTextureData& tex);

    // Create a procedural checkerboard texture (2D)
    void make_checkerboard(i32 size = 64, i32 tile = 8);

    // Create a cubemap from 6 RGBA face buffers (each face_width x face_height).
    // face_data[i] = +X, -X, +Y, -Y, +Z, -Z
    bool create_cubemap(const u8* face_data[6], i32 face_width, i32 face_height);

    void bind(u32 slot = 0) const;
    void destroy();

    bool is_valid() const { return m_handle != 0; }
    bool is_cubemap() const { return m_is_cubemap; }
    i32  width()  const { return m_width; }
    i32  height() const { return m_height; }

    GLuint handle() const { return m_handle; }

private:
    GLuint m_handle   = 0;
    i32    m_width    = 0;
    i32    m_height   = 0;
    bool   m_is_cubemap = false;
};

} // namespace pino
