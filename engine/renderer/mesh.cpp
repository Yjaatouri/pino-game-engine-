#include "mesh.h"
#include "engine/renderer/render_stats.h"
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <cfloat>

namespace pino {

Mesh::~Mesh() { destroy(); }

Mesh::Mesh(Mesh&& other) noexcept
    : m_vao(other.m_vao), m_vbo(other.m_vbo), m_ebo(other.m_ebo),
      m_tangent_vbo(other.m_tangent_vbo), m_bitangent_vbo(other.m_bitangent_vbo),
      m_vertex_count(other.m_vertex_count), m_index_count(other.m_index_count),
      m_instance_vbo(other.m_instance_vbo),
      m_instance_capacity(other.m_instance_capacity),
      m_local_min(other.m_local_min), m_local_max(other.m_local_max)
{
    other.m_vao = other.m_vbo = other.m_ebo = 0;
    other.m_tangent_vbo = other.m_bitangent_vbo = 0;
    other.m_vertex_count = other.m_index_count = 0;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        destroy();
        m_vao  = other.m_vao;  other.m_vao  = 0;
        m_vbo  = other.m_vbo;  other.m_vbo  = 0;
        m_ebo  = other.m_ebo;  other.m_ebo  = 0;
        m_tangent_vbo = other.m_tangent_vbo; other.m_tangent_vbo = 0;
        m_bitangent_vbo = other.m_bitangent_vbo; other.m_bitangent_vbo = 0;
        m_vertex_count = other.m_vertex_count; other.m_vertex_count = 0;
        m_index_count  = other.m_index_count;  other.m_index_count  = 0;
        m_instance_vbo = other.m_instance_vbo; other.m_instance_vbo = 0;
        m_instance_capacity = other.m_instance_capacity; other.m_instance_capacity = 0;
        m_local_min = other.m_local_min;
        m_local_max = other.m_local_max;
    }
    return *this;
}

void Mesh::upload(const Vertex* vertices, u32 vert_count,
                  const u32* indices,  u32 index_count)
{
    destroy();

    m_vertex_count = vert_count;
    m_index_count  = index_count;

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vert_count * sizeof(Vertex)),
                 vertices, GL_STATIC_DRAW);

    // Position (location 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(0);

    // Normal (location 1)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, normal)));
    glEnableVertexAttribArray(1);

    // UV (location 2)
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, uv)));
    glEnableVertexAttribArray(2);

    if (indices && index_count > 0) {
        glGenBuffers(1, &m_ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(index_count * sizeof(u32)),
                     indices, GL_STATIC_DRAW);
    }

    glBindVertexArray(0);

    // Compute local-space bounds from vertex positions
    m_local_min = { FLT_MAX, FLT_MAX, FLT_MAX };
    m_local_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
    for (u32 i = 0; i < vert_count; ++i) {
        m_local_min = glm::min(m_local_min, vertices[i].position);
        m_local_max = glm::max(m_local_max, vertices[i].position);
    }
}

void Mesh::upload_tangents(const glm::vec3* tangents, const glm::vec3* bitangents, u32 count) {
    if (!m_vao || count != m_vertex_count) return;

    glBindVertexArray(m_vao);

    // Tangent VBO (location 3)
    glGenBuffers(1, &m_tangent_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_tangent_vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(count * sizeof(glm::vec3)),
                 tangents, GL_STATIC_DRAW);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
    glEnableVertexAttribArray(3);

    // Bitangent VBO (location 4)
    glGenBuffers(1, &m_bitangent_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_bitangent_vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(count * sizeof(glm::vec3)),
                 bitangents, GL_STATIC_DRAW);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
    glEnableVertexAttribArray(4);

    glBindVertexArray(0);
}

void Mesh::destroy() {
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_ebo) glDeleteBuffers(1, &m_ebo);
    if (m_tangent_vbo) glDeleteBuffers(1, &m_tangent_vbo);
    if (m_bitangent_vbo) glDeleteBuffers(1, &m_bitangent_vbo);
    if (m_instance_vbo) glDeleteBuffers(1, &m_instance_vbo);
    m_vao = m_vbo = m_ebo = m_tangent_vbo = m_bitangent_vbo = m_instance_vbo = 0;
    m_vertex_count = m_index_count = m_instance_capacity = 0;
}

void Mesh::draw() const {
    if (!m_vao) return;
    RenderStats::instance().add_draw_call();
    RenderStats::instance().add_triangles(m_ebo ? m_index_count / 3 : m_vertex_count / 3);
    glBindVertexArray(m_vao);
    if (m_ebo) {
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_index_count),
                       GL_UNSIGNED_INT, nullptr);
    } else {
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_vertex_count));
    }
    glBindVertexArray(0);
}

// ── Instancing ────────────────────────────────────────

void Mesh::set_instance_data(const glm::mat4* transforms, u32 count) {
    if (!m_vao || count == 0) return;

    u32 needed = count * sizeof(glm::mat4);

    if (!m_instance_vbo || count > m_instance_capacity) {
        if (m_instance_vbo) glDeleteBuffers(1, &m_instance_vbo);

        glGenBuffers(1, &m_instance_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, m_instance_vbo);
        glBufferData(GL_ARRAY_BUFFER, needed, nullptr, GL_DYNAMIC_DRAW);
        m_instance_capacity = count;

        // Bind instance attributes to the VAO (locations 3-6 = 4 rows of mat4)
        glBindVertexArray(m_vao);
        for (u32 i = 0; i < 4; ++i) {
            GLint loc = 3 + static_cast<GLint>(i);
            glEnableVertexAttribArray(static_cast<GLuint>(loc));
            glVertexAttribPointer(static_cast<GLuint>(loc), 4, GL_FLOAT, GL_FALSE,
                                  sizeof(glm::mat4),
                                  reinterpret_cast<const void*>(i * sizeof(glm::vec4)));
            glVertexAttribDivisor(static_cast<GLuint>(loc), 1);
        }
        glBindVertexArray(0);
    } else {
        glBindBuffer(GL_ARRAY_BUFFER, m_instance_vbo);
    }

    glBufferSubData(GL_ARRAY_BUFFER, 0, needed, transforms);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Mesh::draw_instanced(u32 count) const {
    if (!m_vao || !m_instance_vbo || count == 0) return;

    RenderStats::instance().add_draw_call();
    RenderStats::instance().add_triangles(
        (m_ebo ? m_index_count / 3 : m_vertex_count / 3) * count);

    glBindVertexArray(m_vao);
    if (m_ebo) {
        glDrawElementsInstanced(GL_TRIANGLES, static_cast<GLsizei>(m_index_count),
                                GL_UNSIGNED_INT, nullptr, static_cast<GLsizei>(count));
    } else {
        glDrawArraysInstanced(GL_TRIANGLES, 0, static_cast<GLsizei>(m_vertex_count),
                              static_cast<GLsizei>(count));
    }
    glBindVertexArray(0);
}

Mesh Mesh::create_sphere(float radius, i32 sectors, i32 stacks) {
    struct TempVert { glm::vec3 pos; glm::vec3 nrm; glm::vec2 uv; };
    std::vector<TempVert> verts;
    std::vector<u32> idx;

    for (i32 i = 0; i <= stacks; ++i) {
        float phi = glm::pi<float>() * static_cast<float>(i) / static_cast<float>(stacks);
        for (i32 j = 0; j <= sectors; ++j) {
            float theta = 2.0f * glm::pi<float>() * static_cast<float>(j) / static_cast<float>(sectors);
            float x = radius * std::sin(phi) * std::cos(theta);
            float y = radius * std::cos(phi);
            float z = radius * std::sin(phi) * std::sin(theta);
            verts.push_back({{x, y, z},
                             glm::normalize(glm::vec3{x, y, z}),
                             {static_cast<float>(j) / static_cast<float>(sectors),
                              static_cast<float>(i) / static_cast<float>(stacks)}});
        }
    }

    for (i32 i = 0; i < stacks; ++i) {
        for (i32 j = 0; j < sectors; ++j) {
            u32 a = i * (sectors + 1) + j;
            u32 b = a + sectors + 1;
            idx.push_back(a); idx.push_back(b); idx.push_back(a + 1);
            idx.push_back(a + 1); idx.push_back(b); idx.push_back(b + 1);
        }
    }

    Mesh m;
    // Convert TempVert → Vertex layout is identical, safe to reinterpret
    m.upload(reinterpret_cast<const Vertex*>(verts.data()),
             static_cast<u32>(verts.size()),
             idx.data(), static_cast<u32>(idx.size()));
    return m;
}

} // namespace pino
