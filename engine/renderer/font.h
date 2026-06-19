#pragma once

#include "engine/core/types.h"
#include "engine/renderer/texture.h"

namespace pino {

class FileSystem;

class Font {
public:
    Font() = default;
    ~Font();

    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;

    Font(Font&& other) noexcept;
    Font& operator=(Font&& other) noexcept;

    struct Glyph {
        f32 u0 = 0, v0 = 0, u1 = 0, v1 = 0;
        f32 advance = 0;
        f32 bearing_x = 0, bearing_y = 0;
        f32 width = 0, height = 0;
    };

    bool load_builtin();
    bool load(const char* atlas_path, FileSystem& fs);
    void destroy();

    const Glyph& glyph(char c) const;

    f32      font_size()   const;
    f32      line_height() const;
    bool     is_valid()    const;
    Texture& atlas();

private:
    Glyph   m_glyphs[128] = {};
    Texture m_atlas;
    f32     m_font_size   = 16.0f;
    f32     m_line_height = 20.0f;
    bool    m_valid       = false;
};

} // namespace pino
