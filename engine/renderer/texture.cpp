#include "texture.h"
#include "engine/serialization/cooked_asset.h"
#include <cstring>
#include <vector>

namespace pino {

Texture::~Texture() { destroy(); }

Texture::Texture(Texture&& other) noexcept
    : m_handle(other.m_handle), m_width(other.m_width), m_height(other.m_height),
      m_is_cubemap(other.m_is_cubemap)
{
    other.m_handle = 0;
    other.m_width = other.m_height = 0;
    other.m_is_cubemap = false;
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        destroy();
        m_handle = other.m_handle; other.m_handle = 0;
        m_width  = other.m_width;  other.m_width  = 0;
        m_height = other.m_height; other.m_height = 0;
        m_is_cubemap = other.m_is_cubemap;
        other.m_is_cubemap = false;
    }
    return *this;
}

void Texture::upload_rgba(const u8* pixels, i32 width, i32 height) {
    destroy();
    m_width  = width;
    m_height = height;
    m_is_cubemap = false;

    glGenTextures(1, &m_handle);
    glBindTexture(GL_TEXTURE_2D, m_handle);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::make_checkerboard(i32 size, i32 tile) {
    std::vector<u8> pixels(static_cast<usize>(size * size * 4));
    for (i32 y = 0; y < size; ++y) {
        for (i32 x = 0; x < size; ++x) {
            bool white = ((x / tile) + (y / tile)) % 2 == 0;
            u8 c = white ? 255 : 32;
            usize idx = static_cast<usize>((y * size + x) * 4);
            pixels[idx + 0] = c;
            pixels[idx + 1] = c;
            pixels[idx + 2] = c;
            pixels[idx + 3] = 255;
        }
    }
    upload_rgba(pixels.data(), size, size);
}

bool Texture::create_cubemap(const u8* face_data[6], i32 face_width, i32 face_height) {
    destroy();
    m_width  = face_width;
    m_height = face_height;
    m_is_cubemap = true;

    glGenTextures(1, &m_handle);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_handle);
    for (i32 i = 0; i < 6; ++i) {
        glTexImage2D(
            static_cast<GLenum>(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i),
            0, GL_RGBA, face_width, face_height, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, face_data[i]);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    return true;
}

void Texture::upload_cooked(const CookedTextureData& tex) {
    destroy();
    m_width  = static_cast<i32>(tex.width);
    m_height = static_cast<i32>(tex.height);
    m_is_cubemap = false;

    CookedTextureFormat fmt = static_cast<CookedTextureFormat>(tex.format);
    bool compressed = (fmt != CookedTextureFormat::RGBA8);

    glGenTextures(1, &m_handle);
    glBindTexture(GL_TEXTURE_2D, m_handle);

    u32 data_offset = 0;
    for (u32 level = 0; level < tex.mip_count; ++level) {
        u32 lw = std::max(tex.width >> level, 1u);
        u32 lh = std::max(tex.height >> level, 1u);
        u32 sz = tex.mip_sizes[level];
        const void* data = tex.mip_data.data() + data_offset;

        if (compressed) {
            GLenum gl_fmt = GL_RGBA8;
            switch (fmt) {
                case CookedTextureFormat::BC1:
                    gl_fmt = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
                    break;
                case CookedTextureFormat::BC3:
                    gl_fmt = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
                    break;
                case CookedTextureFormat::BC5:
                    gl_fmt = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
                    break;
                case CookedTextureFormat::ETC2_RGB:
                    gl_fmt = GL_COMPRESSED_RGB8_ETC2;
                    break;
                case CookedTextureFormat::ETC2_RGBA:
                    gl_fmt = GL_COMPRESSED_RGBA8_ETC2_EAC;
                    break;
                default:
                    break;
            }
            glCompressedTexImage2D(GL_TEXTURE_2D, static_cast<GLint>(level),
                                   gl_fmt,
                                   static_cast<GLsizei>(lw),
                                   static_cast<GLsizei>(lh),
                                   0, static_cast<GLsizei>(sz), data);
        } else {
            glTexImage2D(GL_TEXTURE_2D, static_cast<GLint>(level), GL_RGBA8,
                         static_cast<GLsizei>(lw), static_cast<GLsizei>(lh),
                         0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        }

        data_offset += sz;
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    tex.mip_count > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    if (tex.mip_count > 1) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL,
                        static_cast<GLint>(tex.mip_count - 1));
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::bind(u32 slot) const {
    if (!m_handle) return;
    glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + slot));
    GLenum target = m_is_cubemap ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;
    glBindTexture(target, m_handle);
}

void Texture::destroy() {
    if (m_handle) {
        glDeleteTextures(1, &m_handle);
        m_handle = 0;
    }
    m_is_cubemap = false;
}

} // namespace pino
