#include "mesh_uploader.h"
#include "engine/renderer/mesh.h"
#include "engine/renderer/gl_es3.h"
#include "engine/core/log.h"
#include <glm/glm.hpp>
#include <cfloat>

namespace pino {

// ── Layout inference ─────────────────────────────────────────────
MeshUploadLayout MeshUploader::infer_layout(const CookedMeshData& data) {
    MeshUploadLayout layout;
    layout.vertex_stride = data.vertex_stride > 0 ? data.vertex_stride : sizeof(Vertex);
    layout.has_tangents   = !data.tangent_data.empty();
    layout.has_bitangents = !data.bitangent_data.empty();
    return layout;
}

// ── Core vertex upload (interleaved VBO) ─────────────────────────
bool MeshUploader::upload_vertex_data(Mesh& mesh, const CookedMeshData& data,
                                      const MeshUploadLayout& layout)
{
    glGenVertexArrays(1, &mesh.m_vao);
    glBindVertexArray(mesh.m_vao);

    // VBO — interleaved vertex data (position + normal + uv)
    glGenBuffers(1, &mesh.m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.m_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(data.vertex_data.size()),
                 data.vertex_data.data(),
                 GL_STATIC_DRAW);

    // Position (location 0)
    if (layout.has_positions) {
        glVertexAttribPointer(layout.position_location, 3, GL_FLOAT, GL_FALSE,
                              static_cast<GLsizei>(layout.vertex_stride),
                              reinterpret_cast<const void*>(offsetof(Vertex, position)));
        glEnableVertexAttribArray(layout.position_location);
    }

    // Normal (location 1)
    if (layout.has_normals) {
        glVertexAttribPointer(layout.normal_location, 3, GL_FLOAT, GL_FALSE,
                              static_cast<GLsizei>(layout.vertex_stride),
                              reinterpret_cast<const void*>(offsetof(Vertex, normal)));
        glEnableVertexAttribArray(layout.normal_location);
    }

    // UV (location 2)
    if (layout.has_uvs) {
        glVertexAttribPointer(layout.uv_location, 2, GL_FLOAT, GL_FALSE,
                              static_cast<GLsizei>(layout.vertex_stride),
                              reinterpret_cast<const void*>(offsetof(Vertex, uv)));
        glEnableVertexAttribArray(layout.uv_location);
    }

    // IBO — index data
    if (data.index_count > 0) {
        glGenBuffers(1, &mesh.m_ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.m_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(data.index_count * sizeof(u32)),
                     data.indices.data(),
                     GL_STATIC_DRAW);
    }

    mesh.m_vertex_count = data.vertex_count;
    mesh.m_index_count  = data.index_count;
    return true;
}

// ── Optional attributes (tangent / bitangent) ───────────────────
void MeshUploader::upload_optional_attribs(Mesh& mesh, const CookedMeshData& data,
                                            const MeshUploadLayout& layout)
{
    if (!layout.has_tangents || !layout.has_bitangents)
        return;

    u32 count = data.vertex_count;
    u32 attrib_size = static_cast<u32>(sizeof(glm::vec3));

    // Tangent VBO (location 3)
    glGenBuffers(1, &mesh.m_tangent_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.m_tangent_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(count * attrib_size),
                 data.tangent_data.data(),
                 GL_STATIC_DRAW);
    glVertexAttribPointer(layout.tangent_location, 3, GL_FLOAT, GL_FALSE,
                          static_cast<GLsizei>(attrib_size), nullptr);
    glEnableVertexAttribArray(layout.tangent_location);

    // Bitangent VBO (location 4)
    glGenBuffers(1, &mesh.m_bitangent_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.m_bitangent_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(count * attrib_size),
                 data.bitangent_data.data(),
                 GL_STATIC_DRAW);
    glVertexAttribPointer(layout.bitangent_location, 3, GL_FLOAT, GL_FALSE,
                          static_cast<GLsizei>(attrib_size), nullptr);
    glEnableVertexAttribArray(layout.bitangent_location);
}

// ── Bounds computation ───────────────────────────────────────────
void MeshUploader::compute_bounds(Mesh& mesh, const CookedMeshData& data) {
    if (data.vertex_count == 0 || data.vertex_data.empty())
        return;
    const Vertex* verts = reinterpret_cast<const Vertex*>(data.vertex_data.data());
    mesh.m_local_min = { FLT_MAX, FLT_MAX, FLT_MAX };
    mesh.m_local_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
    for (u32 i = 0; i < data.vertex_count; ++i) {
        mesh.m_local_min = glm::min(mesh.m_local_min, verts[i].position);
        mesh.m_local_max = glm::max(mesh.m_local_max, verts[i].position);
    }
}

// ── Public upload entry point ────────────────────────────────────
std::shared_ptr<Mesh> MeshUploader::upload(const CookedMeshData& data, const char* debug_name) {
    if (data.vertex_count == 0 || data.vertex_data.empty()) {
        PINO_ERROR("MeshUploader: no vertex data (%s)", debug_name ? debug_name : "");
        return nullptr;
    }

    MeshUploadLayout layout = infer_layout(data);

    auto mesh = std::make_shared<Mesh>();

    // Create GL buffers and upload interleaved vertex data
    if (!upload_vertex_data(*mesh, data, layout)) {
        PINO_ERROR("MeshUploader: vertex upload failed (%s)", debug_name ? debug_name : "");
        return nullptr;
    }

    // Handle optional tangent/bitangent attributes
    upload_optional_attribs(*mesh, data, layout);

    // Compute local-space bounds from vertex positions
    compute_bounds(*mesh, data);

    glBindVertexArray(0);

    PINO_INFO("MeshUploader: '%s' (%u verts, %u indices%s)",
              debug_name ? debug_name : "",
              data.vertex_count, data.index_count,
              (layout.has_tangents ? ", with tangents" : ""));

    return mesh;
}

} // namespace pino
