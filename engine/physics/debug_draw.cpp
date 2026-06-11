#include "debug_draw.h"
#include "engine/core/log.h"
#include <glm/gtc/matrix_transform.hpp>

namespace pino {

static const char* debug_vert_src = R"(
#version 300 es
layout(location = 0) in vec3 a_pos;
uniform mat4 u_mvp;
void main() { gl_Position = u_mvp * vec4(a_pos, 1.0); }
)";

static const char* debug_frag_src = R"(
#version 300 es
precision mediump float;
uniform vec4 u_color;
out vec4 frag_color;
void main() { frag_color = u_color; }
)";

DebugDraw::DebugDraw() = default;
DebugDraw::~DebugDraw() { destroy(); }

bool DebugDraw::init() {
    if (!m_shader.load(debug_vert_src, debug_frag_src)) {
        PINO_ERROR("DebugDraw: failed to compile debug shader");
        return false;
    }

    // 12 line segments of a unit cube: 24 vertices (2 per line)
    static const f32 cube_wire[24 * 3] = {
        // front face
        -0.5f, -0.5f, -0.5f,   0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,   0.5f,  0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,  -0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,  -0.5f, -0.5f, -0.5f,
        // back face
        -0.5f, -0.5f,  0.5f,   0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,   0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,  -0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,  -0.5f, -0.5f,  0.5f,
        // connecting edges
        -0.5f, -0.5f, -0.5f,  -0.5f, -0.5f,  0.5f,
         0.5f, -0.5f, -0.5f,   0.5f, -0.5f,  0.5f,
         0.5f,  0.5f, -0.5f,   0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f, -0.5f,  -0.5f,  0.5f,  0.5f,
    };

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_wire), cube_wire, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(f32), nullptr);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    return true;
}

void DebugDraw::draw_aabbs(const std::vector<AABB>& aabbs, const Camera& camera) {
    if (aabbs.empty() || !m_vao) return;

    glDisable(GL_DEPTH_TEST);

    m_shader.bind();
    m_shader.set_vec4("u_color", {0.0f, 1.0f, 0.0f, 1.0f});

    glBindVertexArray(m_vao);

    glm::mat4 vp = camera.view_proj();

    for (const auto& aabb : aabbs) {
        glm::vec3 half  = aabb.extents();
        glm::vec3 center = aabb.center();

        glm::mat4 model = glm::translate(glm::mat4(1.0f), center);
        model = glm::scale(model, half * 2.0f);

        m_shader.set_mat4("u_mvp", vp * model);

        glDrawArrays(GL_LINES, 0, 24);
    }

    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
}

void DebugDraw::destroy() {
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    m_vao = m_vbo = 0;
    m_shader.destroy();
}

} // namespace pino