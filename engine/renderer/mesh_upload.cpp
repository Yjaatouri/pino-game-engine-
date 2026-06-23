#include "mesh_upload.h"
#include "engine/renderer/mesh.h"
#include "engine/core/log.h"
#include <glm/glm.hpp>

namespace pino {

std::shared_ptr<Mesh> upload_cooked_mesh(const CookedMeshData& data, const char* debug_name) {
    if (data.vertex_count == 0 || data.vertex_data.empty()) {
        PINO_ERROR("Cooked mesh upload: no vertices (%s)", debug_name ? debug_name : "");
        return nullptr;
    }

    auto mesh = std::make_shared<Mesh>();

    // Upload core attributes: position (loc 0), normal (loc 1), uv (loc 2)
    const Vertex* verts = reinterpret_cast<const Vertex*>(data.vertex_data.data());
    const u32* idx = data.indices.data();
    mesh->upload(verts, data.vertex_count, idx, data.index_count);

    // Upload optional tangent (loc 3) and bitangent (loc 4)
    if (!data.tangent_data.empty() && !data.bitangent_data.empty()) {
        const glm::vec3* tangents = reinterpret_cast<const glm::vec3*>(data.tangent_data.data());
        const glm::vec3* bitangents = reinterpret_cast<const glm::vec3*>(data.bitangent_data.data());
        mesh->upload_tangents(tangents, bitangents, data.vertex_count);
    }

    PINO_INFO("Uploaded cooked mesh '%s' (%u verts, %u indices%s)",
              debug_name ? debug_name : "",
              data.vertex_count, data.index_count,
              data.tangent_data.empty() ? "" : ", with tangents");

    return mesh;
}

} // namespace pino
