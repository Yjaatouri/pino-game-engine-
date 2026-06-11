#include "debug_renderer.h"
#include "engine/core/log.h"
#include "render_stats.h"
#include <glm/gtc/matrix_transform.hpp>

namespace pino {

static const char* line_vert_src = R"(
#version 300 es
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec4 a_color;
uniform mat4 u_mvp;
out vec4 v_color;
void main() {
    gl_Position = u_mvp * vec4(a_pos, 1.0);
    v_color = a_color;
}
)";

static const char* line_frag_src = R"(
#version 300 es
precision mediump float;
in vec4 v_color;
out vec4 frag_color;
void main() { frag_color = v_color; }
)";

static const char* point_vert_src = R"(
#version 300 es
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec4 a_color;
uniform mat4 u_mvp;
uniform float u_point_size;
out vec4 v_color;
void main() {
    gl_Position = u_mvp * vec4(a_pos, 1.0);
    gl_PointSize = u_point_size;
    v_color = a_color;
}
)";

static const char* point_frag_src = R"(
#version 300 es
precision mediump float;
in vec4 v_color;
out vec4 frag_color;
void main() {
    frag_color = v_color;
}
)";

DebugRenderer::~DebugRenderer() { destroy(); }

bool DebugRenderer::init() {
    if (!m_line_shader.load(line_vert_src, line_frag_src)) {
        PINO_ERROR("DebugRenderer: failed to load line shader");
        return false;
    }
    if (!m_point_shader.load(point_vert_src, point_frag_src)) {
        PINO_ERROR("DebugRenderer: failed to load point shader");
        return false;
    }

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(f32) * 7,
                          reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(f32) * 7,
                          reinterpret_cast<void*>(sizeof(f32) * 3));

    glBindVertexArray(0);
    return true;
}

void DebugRenderer::begin_frame() {
    m_lines.clear();
    m_points.clear();
}

void DebugRenderer::draw_line(const glm::vec3& from, const glm::vec3& to,
                               const glm::vec4& color) {
    if (m_lines.size() >= MAX_VERTS / 2) return;
    m_lines.push_back({from, to, color});
}

void DebugRenderer::draw_box(const glm::vec3& center, const glm::vec3& half_extents,
                              const glm::vec4& color) {
    glm::vec3 min = center - half_extents;
    glm::vec3 max = center + half_extents;

    glm::vec3 c[8] = {
        {min.x, min.y, min.z}, {max.x, min.y, min.z},
        {max.x, max.y, min.z}, {min.x, max.y, min.z},
        {min.x, min.y, max.z}, {max.x, min.y, max.z},
        {max.x, max.y, max.z}, {min.x, max.y, max.z},
    };

    static const int edges[24] = {
        0,1, 1,2, 2,3, 3,0,
        4,5, 5,6, 6,7, 7,4,
        0,4, 1,5, 2,6, 3,7
    };

    for (int i = 0; i < 24; i += 2)
        draw_line(c[edges[i]], c[edges[i + 1]], color);
}

void DebugRenderer::draw_sphere(const glm::vec3& center, f32 radius,
                                 const glm::vec4& color, i32 segments) {
    for (int ring = 0; ring < 3; ++ring) {
        for (int i = 0; i < segments; ++i) {
            f32 a0 = 6.2831853f * static_cast<f32>(i) / static_cast<f32>(segments);
            f32 a1 = 6.2831853f * static_cast<f32>(i + 1) / static_cast<f32>(segments);

            glm::vec3 p0, p1;
            switch (ring) {
                case 0: // XY ring
                    p0 = center + glm::vec3{std::cos(a0) * radius, std::sin(a0) * radius, 0};
                    p1 = center + glm::vec3{std::cos(a1) * radius, std::sin(a1) * radius, 0};
                    break;
                case 1: // XZ ring
                    p0 = center + glm::vec3{std::cos(a0) * radius, 0, std::sin(a0) * radius};
                    p1 = center + glm::vec3{std::cos(a1) * radius, 0, std::sin(a1) * radius};
                    break;
                default: // YZ ring
                    p0 = center + glm::vec3{0, std::cos(a0) * radius, std::sin(a0) * radius};
                    p1 = center + glm::vec3{0, std::cos(a1) * radius, std::sin(a1) * radius};
                    break;
            }
            draw_line(p0, p1, color);
        }
    }
}

void DebugRenderer::draw_axes(const glm::mat4& transform, f32 length) {
    glm::vec3 origin = glm::vec3(transform[3]);
    glm::vec3 right   = glm::normalize(glm::vec3(transform[0])) * length;
    glm::vec3 up      = glm::normalize(glm::vec3(transform[1])) * length;
    glm::vec3 forward = glm::normalize(glm::vec3(transform[2])) * length;

    draw_line(origin, origin + right,   {1, 0, 0, 1});
    draw_line(origin, origin + up,      {0, 1, 0, 1});
    draw_line(origin, origin + forward, {0, 0, 1, 1});
}

void DebugRenderer::flush_lines(const glm::mat4& view_proj) {
    if (m_lines.empty()) return;

    u32 vert_count = static_cast<u32>(m_lines.size()) * 2;
    std::vector<f32> verts;
    verts.reserve(static_cast<usize>(vert_count) * 7);

    for (const auto& line : m_lines) {
        verts.push_back(line.a.x); verts.push_back(line.a.y); verts.push_back(line.a.z);
        verts.push_back(line.color.r); verts.push_back(line.color.g);
        verts.push_back(line.color.b); verts.push_back(line.color.a);
        verts.push_back(line.b.x); verts.push_back(line.b.y); verts.push_back(line.b.z);
        verts.push_back(line.color.r); verts.push_back(line.color.g);
        verts.push_back(line.color.b); verts.push_back(line.color.a);
    }

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(verts.size() * sizeof(f32)),
                 verts.data(), GL_STREAM_DRAW);

    m_line_shader.bind();
    m_line_shader.set_mat4("u_mvp", view_proj);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vert_count));
    glBindVertexArray(0);

    RenderStats::instance().add_draw_call();
    RenderStats::instance().add_triangles(0);
}

void DebugRenderer::flush_points(const glm::mat4& view_proj) {
    if (m_points.empty()) return;

    u32 count = static_cast<u32>(m_points.size());
    std::vector<f32> verts;
    verts.reserve(static_cast<usize>(count) * 7);

    for (const auto& pt : m_points) {
        verts.push_back(pt.position.x); verts.push_back(pt.position.y); verts.push_back(pt.position.z);
        verts.push_back(pt.color.r); verts.push_back(pt.color.g);
        verts.push_back(pt.color.b); verts.push_back(pt.color.a);
    }

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(verts.size() * sizeof(f32)),
                 verts.data(), GL_STREAM_DRAW);

    m_point_shader.bind();
    m_point_shader.set_mat4("u_mvp", view_proj);
    m_point_shader.set_float("u_point_size", m_points[0].size);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(count));
    glBindVertexArray(0);

    RenderStats::instance().add_draw_call();
    RenderStats::instance().add_triangles(0);
}

void DebugRenderer::render(const glm::mat4& view_proj) {
    flush_lines(view_proj);
    flush_points(view_proj);
}

void DebugRenderer::end_frame() {
    m_lines.clear();
    m_points.clear();
}

void DebugRenderer::destroy() {
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    m_vao = m_vbo = 0;
    m_line_shader.destroy();
    m_point_shader.destroy();
}

} // namespace pino
