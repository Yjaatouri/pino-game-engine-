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

    bool operator==(const Vertex& other) const {
        return position == other.position &&
               normal   == other.normal   &&
               uv       == other.uv;
    }
};

class MeshUploader; // forward decl for friendship

class Mesh {
public:
    friend class MeshUploader;
    Mesh() = default;
    ~Mesh();

    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    void upload(const Vertex* vertices, u32 vert_count,
                const u32* indices,  u32 index_count);

    // Upload tangent/bitangent data as additional vertex attributes
    // (location 3 = tangent, location 4 = bitangent).
    // Must be called AFTER upload().
    void upload_tangents(const glm::vec3* tangents, const glm::vec3* bitangents, u32 count);

    void destroy();

    void draw() const;

    // Instancing
    void set_instance_data(const glm::mat4* transforms, u32 count);
    void draw_instanced(u32 count) const;
    u32  instance_capacity() const { return m_instance_capacity; }

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
    GLuint m_tangent_vbo = 0;   // optional tangent buffer (location 3)
    GLuint m_bitangent_vbo = 0; // optional bitangent buffer (location 4)
    u32    m_vertex_count = 0;
    u32    m_index_count  = 0;

    GLuint m_instance_vbo = 0;
    u32    m_instance_capacity = 0;

    glm::vec3 m_local_min = { -0.5f, -0.5f, -0.5f };
    glm::vec3 m_local_max = {  0.5f,  0.5f,  0.5f };
};

} // namespace pino
