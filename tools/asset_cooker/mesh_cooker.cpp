#include "cooker.h"
#include "engine/renderer/mesh.h"
#include <tiny_obj_loader.h>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <cstring>

namespace pino {
namespace {

class MeshCooker : public ICooker {
public:
    std::string extension() const override { return "obj"; }
    u32 asset_type() const override { return CookedType::Mesh; }

    CookResult cook(const CookInput& input, BinaryChunkWriter& writer) override {
        std::ifstream file(input.source_path, std::ios::binary);
        if (!file) {
            return {false, "Cannot open " + input.source_path};
        }
        std::stringstream ss;
        ss << file.rdbuf();
        std::string src = ss.str();

        tinyobj::ObjReader reader;
        tinyobj::ObjReaderConfig cfg;
        cfg.triangulate = true;
        cfg.vertex_color = false;

        if (!reader.ParseFromString(src, "", cfg)) {
            return {false, "tinyobj: " + reader.Error()};
        }

        const auto& attrib = reader.GetAttrib();
        const auto& shapes = reader.GetShapes();

        if (shapes.empty()) {
            return {false, "No shapes in " + input.source_path};
        }

        std::vector<Vertex> vertices;
        std::vector<u32>    indices;

        for (const auto& shape : shapes) {
            u32 index_offset = 0;
            for (usize f = 0; f < shape.mesh.num_face_vertices.size(); ++f) {
                u32 fv = shape.mesh.num_face_vertices[f];
                for (u32 v = 0; v < fv; ++v) {
                    tinyobj::index_t idx = shape.mesh.indices[index_offset + v];

                    Vertex vert;
                    vert.position.x = attrib.vertices[3 * idx.vertex_index + 0];
                    vert.position.y = attrib.vertices[3 * idx.vertex_index + 1];
                    vert.position.z = attrib.vertices[3 * idx.vertex_index + 2];

                    if (idx.normal_index >= 0) {
                        vert.normal.x = attrib.normals[3 * idx.normal_index + 0];
                        vert.normal.y = attrib.normals[3 * idx.normal_index + 1];
                        vert.normal.z = attrib.normals[3 * idx.normal_index + 2];
                    } else {
                        vert.normal = {0, 1, 0};
                    }

                    if (idx.texcoord_index >= 0) {
                        vert.uv.x = attrib.texcoords[2 * idx.texcoord_index + 0];
                        vert.uv.y = 1.0f - attrib.texcoords[2 * idx.texcoord_index + 1];
                    } else {
                        vert.uv = {0, 0};
                    }

                    vertices.push_back(vert);
                    indices.push_back(static_cast<u32>(indices.size()));
                }
                index_offset += fv;
            }
        }

        if (vertices.empty()) {
            return {false, "No vertices generated from " + input.source_path};
        }

        glm::vec3 bmin = vertices[0].position;
        glm::vec3 bmax = vertices[0].position;
        for (const auto& v : vertices) {
            bmin = glm::min(bmin, v.position);
            bmax = glm::max(bmax, v.position);
        }
        glm::vec3 center = (bmin + bmax) * 0.5f;
        float radius = 0.0f;
        for (const auto& v : vertices) {
            radius = glm::max(radius, glm::distance(v.position, center));
        }

        CookedMeshData mesh;
        mesh.vertex_count  = static_cast<u32>(vertices.size());
        mesh.index_count   = static_cast<u32>(indices.size());
        mesh.vertex_stride = sizeof(Vertex);
        mesh.vertex_data.resize(vertices.size() * sizeof(Vertex));
        std::memcpy(mesh.vertex_data.data(), vertices.data(), mesh.vertex_data.size());
        mesh.indices = indices;
        mesh.bounds_min    = bmin;
        mesh.bounds_max    = bmax;
        mesh.bounds_center = center;
        mesh.bounds_radius = radius;

        write_cooked_mesh(writer, CookedPlatform::Desktop, mesh);
        return {true, "", CookedType::Mesh, {}};
    }
};

} // namespace

void register_mesh_cooker(CookerRegistry& reg) {
    static MeshCooker cooker;
    reg.register_cooker(&cooker);
}

} // namespace pino
