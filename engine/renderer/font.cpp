#include "font.h"

namespace pino {

Font::~Font() { destroy(); }

Font::Font(Font&& other) noexcept
    : m_font_size(other.m_font_size)
    , m_line_height(other.m_line_height)
    , m_valid(other.m_valid)
{
    for (int i = 0; i < 128; ++i)
        m_glyphs[i] = other.m_glyphs[i];
    other.m_valid = false;
}

Font& Font::operator=(Font&& other) noexcept {
    if (this != &other) {
        m_font_size   = other.m_font_size;
        m_line_height = other.m_line_height;
        m_valid       = other.m_valid;
        for (int i = 0; i < 128; ++i)
            m_glyphs[i] = other.m_glyphs[i];
        other.m_valid = false;
    }
    return *this;
}

bool Font::load_builtin() {
    return false;
}

bool Font::load(const char* /*atlas_path*/, FileSystem& /*fs*/) {
    return false;
}

void Font::destroy() {
    m_valid = false;
}

const Font::Glyph& Font::glyph(char c) const {
    static Glyph null_glyph{};
    u8 idx = static_cast<u8>(c);
    if (idx >= 128) return null_glyph;
    return m_glyphs[idx];
}

} // namespace pino
