#pragma once

#include "engine/core/types.h"
#include "engine/renderer/gl_es3.h"
#include <vector>

#include <glm/glm.hpp>

namespace pino {

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};

class Mesh {
public:
    Mesh() = default;
    ~Mesh();

    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    void upload(const Vertex* vertices, u32 vert_count,
                const u32* indices,  u32 index_count);
    void destroy();

    void draw() const;

    bool is_valid() const { return m_vao != 0; }

    u32 vertex_count()  const { return m_vertex_count; }
    u32 index_count()   const { return m_index_count; }

    // Local-space bounds (computed from vertex data on upload)
    const glm::vec3& local_min() const { return m_local_min; }
    const glm::vec3& local_max() const { return m_local_max; }
    glm::vec3 local_center() const { return (m_local_min + m_local_max) * 0.5f; }
    glm::vec3 local_extents() const { return (m_local_max - m_local_min) * 0.5f; }

    // Primitives
    static Mesh create_sphere(float radius = 0.5f, i32 sectors = 32, i32 stacks = 24);

private:
    GLuint m_vao  = 0;
    GLuint m_vbo  = 0;
    GLuint m_ebo  = 0;
    u32    m_vertex_count = 0;
    u32    m_index_count  = 0;

    glm::vec3 m_local_min = { -0.5f, -0.5f, -0.5f };
    glm::vec3 m_local_max = {  0.5f,  0.5f,  0.5f };
};

} // namespace pino
