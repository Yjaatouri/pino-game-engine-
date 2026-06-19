#include "text_renderer.h"
#include "engine/core/log.h"
#include "render_stats.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cstdio>

namespace pino {

static const char* kVertSrc = R"(
#version 300 es
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;
uniform mat4 u_mvp;
out vec2 v_uv;
void main() {
    gl_Position = u_mvp * vec4(a_pos, 0.0, 1.0);
    v_uv = a_uv;
}
)";

static const char* kFragSrc = R"(
#version 300 es
precision mediump float;
in vec2 v_uv;
uniform sampler2D u_tex;
uniform vec4 u_color;
out vec4 frag_color;
void main() {
    float alpha = texture(u_tex, v_uv).r;
    frag_color = vec4(u_color.rgb, u_color.a * alpha);
}
)";

TextRenderer::~TextRenderer() { destroy(); }

bool TextRenderer::init(i32 w, i32 h) {
    if (!m_shader.load(kVertSrc, kFragSrc)) {
        PINO_ERROR("TextRenderer: failed to load shader");
        return false;
    }

    m_u_mvp   = glGetUniformLocation(m_shader.handle(), "u_mvp");
    m_u_tex   = glGetUniformLocation(m_shader.handle(), "u_tex");
    m_u_color = glGetUniformLocation(m_shader.handle(), "u_color");

    // Pre-compute index buffer
    GLushort idx[MAX_QUADS * 6];
    for (i32 i = 0; i < MAX_QUADS; ++i) {
        idx[i * 6 + 0] = static_cast<GLushort>(i * 4 + 0);
        idx[i * 6 + 1] = static_cast<GLushort>(i * 4 + 1);
        idx[i * 6 + 2] = static_cast<GLushort>(i * 4 + 2);
        idx[i * 6 + 3] = static_cast<GLushort>(i * 4 + 0);
        idx[i * 6 + 4] = static_cast<GLushort>(i * 4 + 2);
        idx[i * 6 + 5] = static_cast<GLushort>(i * 4 + 3);
    }

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ibo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(m_verts), nullptr, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(8));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);
    glBindVertexArray(0);

    return true;
}

void TextRenderer::destroy() {
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_ibo) glDeleteBuffers(1, &m_ibo);
    m_vao = m_vbo = m_ibo = 0;
    m_shader.destroy();
}

void TextRenderer::begin_frame() {
    m_commands.clear();
}

void TextRenderer::draw_text(Font& font, const char* text, f32 x, f32 y,
                             f32 scale, f32 r, f32 g, f32 b, f32 a) {
    m_commands.push_back({&font, std::string(text), x, y, scale, r, g, b, a});
}

void TextRenderer::rasterize(const TextCommand& cmd) {
    Font& font = *cmd.font;
    f32 cx = cmd.x, cy = cmd.y;
    for (const char* p = cmd.text.c_str(); *p; ++p) {
        if (m_quad_count >= MAX_QUADS) break;
        if (*p == '\n') { cx = cmd.x; cy += font.line_height() * cmd.scale; continue; }
        const auto& glyph = font.glyph(*p);
        if (glyph.width == 0 && glyph.height == 0) { cx += 6.0f * cmd.scale; continue; }

        f32 x0 = cx + glyph.bearing_x * cmd.scale;
        f32 y0 = cy;
        f32 x1 = x0 + glyph.width * cmd.scale;
        f32 y1 = y0 + glyph.height * cmd.scale;

        Vertex* v = &m_verts[m_quad_count * 4];
        v[0] = {x0, y0, glyph.u0, glyph.v0};
        v[1] = {x1, y0, glyph.u1, glyph.v0};
        v[2] = {x1, y1, glyph.u1, glyph.v1};
        v[3] = {x0, y1, glyph.u0, glyph.v1};
        ++m_quad_count;

        cx += glyph.advance * cmd.scale;
    }
}

void TextRenderer::render(i32 w, i32 h) {
    if (m_commands.empty()) return;

    glm::mat4 ortho = glm::ortho(0.0f, static_cast<f32>(w),
                                 static_cast<f32>(h), 0.0f, -1.0f, 1.0f);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    m_shader.bind();
    m_shader.set_mat4("u_mvp", ortho);

    for (const auto& cmd : m_commands) {
        m_quad_count = 0;
        rasterize(cmd);
        if (m_quad_count == 0) continue;

        cmd.font->atlas().bind(0);
        glUniform1i(m_u_tex, 0);
        glUniform4f(m_u_color, cmd.r, cmd.g, cmd.b, cmd.a);

        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(m_quad_count * 4 * sizeof(Vertex)),
                     m_verts, GL_STREAM_DRAW);

        glBindVertexArray(m_vao);
        glDrawElements(GL_TRIANGLES, m_quad_count * 6, GL_UNSIGNED_SHORT, nullptr);
        glBindVertexArray(0);

        RenderStats::instance().add_draw_call();
        RenderStats::instance().add_triangles(m_quad_count * 2);
    }

    m_shader.unbind();
}

void TextRenderer::end_frame() {
    m_commands.clear();
}

} // namespace pino
