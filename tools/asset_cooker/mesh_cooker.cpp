#include "cooker.h"
#include "engine/renderer/mesh.h"
#include <tiny_obj_loader.h>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <cstring>
#include <unordered_map>
#include <cmath>
#include <glm/glm.hpp>

namespace pino {
namespace {

// ── Hash for vertex deduplication ─────────────────────────────
struct VertexHasher {
    size_t operator()(const Vertex& v) const {
        size_t h = 0;
        auto mix = [&](uint32_t bits) {
            h ^= bits + 0x9e3779b9u + (h << 6) + (h >> 2);
        };
        uint32_t tmp;
        std::memcpy(&tmp, &v.position.x, 4); mix(tmp);
        std::memcpy(&tmp, &v.position.y, 4); mix(tmp);
        std::memcpy(&tmp, &v.position.z, 4); mix(tmp);
        std::memcpy(&tmp, &v.normal.x, 4); mix(tmp);
        std::memcpy(&tmp, &v.normal.y, 4); mix(tmp);
        std::memcpy(&tmp, &v.normal.z, 4); mix(tmp);
        std::memcpy(&tmp, &v.uv.x, 4); mix(tmp);
        std::memcpy(&tmp, &v.uv.y, 4); mix(tmp);
        return h;
    }
};

// ── Triangle scoring for vertex cache optimization ────────────
// Implements a FIFO cache model (Tip&Wu style) with a fixed cache size.
// Scores unprocessed triangles by how many of their vertices are in cache.
static void optimize_indices(const std::vector<u32>& src_indices, u32 vertex_count,
                             std::vector<u32>& dst_indices) {
    static const u32 CACHE_SIZE = 32;
    u32 tri_count = static_cast<u32>(src_indices.size() / 3);
    if (tri_count == 0) { dst_indices = src_indices; return; }

    std::vector<int> vertex_age(vertex_count, -1);
    std::vector<bool> processed(tri_count, false);
    int timestamp = 0;

    dst_indices.clear();
    dst_indices.reserve(src_indices.size());

    for (u32 emitted = 0; emitted < tri_count; ++emitted) {
        int best_tri = -1;
        int best_score = -1;

        for (u32 t = 0; t < tri_count; ++t) {
            if (processed[t]) continue;

            u32 i0 = src_indices[t * 3 + 0];
            u32 i1 = src_indices[t * 3 + 1];
            u32 i2 = src_indices[t * 3 + 2];

            auto cache_score = [&](u32 idx) -> int {
                int age = vertex_age[idx];
                if (age < 0) return 0;
                int dist = timestamp - age;
                if (dist < static_cast<int>(CACHE_SIZE)) return CACHE_SIZE - dist;
                return 0;
            };

            int score = cache_score(i0) + cache_score(i1) + cache_score(i2);
            if (score > best_score) { best_score = score; best_tri = static_cast<int>(t); }
        }

        u32 to = static_cast<u32>(best_tri) * 3;
        u32 i0 = src_indices[to];
        u32 i1 = src_indices[to + 1];
        u32 i2 = src_indices[to + 2];

        dst_indices.push_back(i0);
        dst_indices.push_back(i1);
        dst_indices.push_back(i2);

        vertex_age[i0] = timestamp++;
        vertex_age[i1] = timestamp++;
        vertex_age[i2] = timestamp++;

        processed[static_cast<u32>(best_tri)] = true;
    }
}

// ── Tangent/bitangent generation ──────────────────────────────
static void generate_tangents(const std::vector<Vertex>& verts, const std::vector<u32>& indices,
                              std::vector<glm::vec3>& tangents, std::vector<glm::vec3>& bitangents) {
    u32 vc = static_cast<u32>(verts.size());
    std::vector<glm::vec3> tan1(vc, glm::vec3(0.0f));
    std::vector<glm::vec3> tan2(vc, glm::vec3(0.0f));

    for (u32 i = 0; i + 2 < static_cast<u32>(indices.size()); i += 3) {
        u32 i0 = indices[i], i1 = indices[i + 1], i2 = indices[i + 2];

        const glm::vec3& v0 = verts[i0].position;
        const glm::vec3& v1 = verts[i1].position;
        const glm::vec3& v2 = verts[i2].position;

        const glm::vec2& uv0 = verts[i0].uv;
        const glm::vec2& uv1 = verts[i1].uv;
        const glm::vec2& uv2 = verts[i2].uv;

        glm::vec3 e1 = v1 - v0;
        glm::vec3 e2 = v2 - v0;
        glm::vec2 duv1 = uv1 - uv0;
        glm::vec2 duv2 = uv2 - uv0;

        float r = duv1.x * duv2.y - duv2.x * duv1.y;
        if (std::fabs(r) < 1e-8f) continue;
        r = 1.0f / r;

        glm::vec3 t = (e1 * duv2.y - e2 * duv1.y) * r;
        glm::vec3 b = (e2 * duv1.x - e1 * duv2.x) * r;

        tan1[i0] += t; tan1[i1] += t; tan1[i2] += t;
        tan2[i0] += b; tan2[i1] += b; tan2[i2] += b;
    }

    tangents.resize(vc);
    bitangents.resize(vc);
    for (u32 i = 0; i < vc; ++i) {
        const glm::vec3& n = verts[i].normal;
        const glm::vec3& t = tan1[i];

        if (glm::length(t) < 1e-8f) {
            tangents[i] = glm::vec3(1.0f, 0.0f, 0.0f);
            bitangents[i] = glm::cross(n, tangents[i]);
            continue;
        }

        tangents[i] = glm::normalize(t - n * glm::dot(n, t));
        float w = (glm::dot(glm::cross(n, t), tan2[i]) < 0.0f) ? -1.0f : 1.0f;
        bitangents[i] = glm::normalize(glm::cross(n, tangents[i]) * w);
    }
}

// ── Cooker implementation ─────────────────────────────────────
class MeshCooker : public ICooker {
public:
    std::string extension() const override { return "obj"; }
    u32 asset_type() const override { return CookedType::Mesh; }

    CookResult cook(const CookInput& input, BinaryChunkWriter& writer) override {
        // ─── 1. Parse OBJ ────────────────────────────────────────
        std::ifstream file(input.source_path, std::ios::binary);
        if (!file) return {false, "Cannot open " + input.source_path};

        std::stringstream ss;
        ss << file.rdbuf();
        std::string src = ss.str();

        tinyobj::ObjReader reader;
        tinyobj::ObjReaderConfig cfg;
        cfg.triangulate = true;
        cfg.vertex_color = false;

        if (!reader.ParseFromString(src, "", cfg))
            return {false, "tinyobj: " + reader.Error()};

        const auto& attrib = reader.GetAttrib();
        const auto& shapes = reader.GetShapes();

        if (shapes.empty())
            return {false, "No shapes in " + input.source_path};

        // ─── 2. Collect face vertices with deduplication ─────────
        std::vector<Vertex> unique_verts;
        std::vector<u32>    src_indices;
        std::unordered_map<Vertex, u32, VertexHasher> vert_map;

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

                    auto it = vert_map.find(vert);
                    if (it != vert_map.end()) {
                        src_indices.push_back(it->second);
                    } else {
                        u32 new_idx = static_cast<u32>(unique_verts.size());
                        vert_map[vert] = new_idx;
                        unique_verts.push_back(vert);
                        src_indices.push_back(new_idx);
                    }
                }
                index_offset += fv;
            }
        }

        if (unique_verts.empty())
            return {false, "No vertices generated from " + input.source_path};

        // ─── 3. Vertex cache optimization ────────────────────────
        std::vector<u32> opt_indices;
        optimize_indices(src_indices, static_cast<u32>(unique_verts.size()), opt_indices);

        // ─── 4. Bounding box + sphere ────────────────────────────
        glm::vec3 bmin = unique_verts[0].position;
        glm::vec3 bmax = unique_verts[0].position;
        for (const auto& v : unique_verts) {
            bmin = glm::min(bmin, v.position);
            bmax = glm::max(bmax, v.position);
        }
        glm::vec3 center = (bmin + bmax) * 0.5f;
        float radius = 0.0f;
        for (const auto& v : unique_verts)
            radius = glm::max(radius, glm::distance(v.position, center));

        // ─── 5. Tangent + bitangent generation ───────────────────
        std::vector<glm::vec3> tangents, bitangents;
        generate_tangents(unique_verts, opt_indices, tangents, bitangents);

        // ─── 6. Serialize to CookedMeshData ──────────────────────
        CookedMeshData mesh;
        mesh.vertex_count  = static_cast<u32>(unique_verts.size());
        mesh.index_count   = static_cast<u32>(opt_indices.size());
        mesh.vertex_stride = sizeof(Vertex);

        // Explicit vertex layout metadata — fully self-descriptive
        mesh.position_attrib   = { 0, 12, 3 };  // offset=0,  sizeof(vec3), 3 floats
        mesh.normal_attrib     = { 12, 12, 3 }; // offset=12, sizeof(vec3), 3 floats
        mesh.uv_attrib         = { 24, 8, 2 };  // offset=24, sizeof(vec2), 2 floats

        // Explicit attribute presence — no implicit assumptions at runtime
        mesh.has_positions   = true;
        mesh.has_normals     = true;
        mesh.has_uvs         = true;
        mesh.has_tangents    = !tangents.empty();
        mesh.has_bitangents  = !bitangents.empty();

        mesh.tangent_per_vertex   = sizeof(glm::vec3);
        mesh.bitangent_per_vertex = sizeof(glm::vec3);

        mesh.vertex_data.resize(unique_verts.size() * sizeof(Vertex));
        std::memcpy(mesh.vertex_data.data(), unique_verts.data(), mesh.vertex_data.size());

        mesh.indices = std::move(opt_indices);

        mesh.tangent_data.resize(tangents.size() * sizeof(glm::vec3));
        mesh.bitangent_data.resize(bitangents.size() * sizeof(glm::vec3));
        if (!tangents.empty()) {
            std::memcpy(mesh.tangent_data.data(), tangents.data(), mesh.tangent_data.size());
            std::memcpy(mesh.bitangent_data.data(), bitangents.data(), mesh.bitangent_data.size());
        }

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
