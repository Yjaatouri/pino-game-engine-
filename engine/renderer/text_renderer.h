#pragma once

#include "engine/core/types.h"
#include "engine/renderer/gl_es3.h"
#include "engine/renderer/shader.h"
#include "engine/renderer/font.h"
#include <glm/glm.hpp>
#include <vector>
#include <string>

namespace pino {

class TextRenderer {
public:
    TextRenderer() = default;
    ~TextRenderer();

    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;

    bool init(i32 w, i32 h);
    void destroy();

    void begin_frame();
    void draw_text(Font& font, const char* text, f32 x, f32 y, f32 scale,
                   f32 r, f32 g, f32 b, f32 a);
    void flush();
    void end_frame();

    bool is_valid() const { return m_vao != 0; }

private:
    struct Vertex { f32 x, y, u, v; };

    struct TextCommand {
        Font*   font;
        std::string text;
        f32     x, y;
        f32     scale;
        f32     r, g, b, a;
    };

    void rasterize(const TextCommand& cmd);

    Shader  m_shader;
    GLuint  m_vao = 0;
    GLuint  m_vbo = 0;
    GLuint  m_ibo = 0;
    GLint   m_u_mvp   = -1;
    GLint   m_u_tex   = -1;
    GLint   m_u_color = -1;

    glm::mat4 m_ortho{1.0f};

    static constexpr i32 MAX_QUADS = 2048;
    Vertex  m_verts[MAX_QUADS * 4];
    i32     m_quad_count = 0;

    std::vector<TextCommand> m_commands;
};

} // namespace pino
