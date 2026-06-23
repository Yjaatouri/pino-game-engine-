#include "engine/core/binary_chunk.h"
#include "engine/core/serializer.h"
#include "engine/renderer/mesh.h"
#include "engine/renderer/material.h"
#include "engine/serialization/cooked_asset.h"
#include <cstdio>
#include <cstring>
#include <vector>

using namespace pino;

static int s_pass = 0;
static int s_fail = 0;

#define TEST(name, expr) do { \
    if (!(expr)) { \
        printf("  FAIL: %s\n", name); \
        ++s_fail; \
    } else { \
        printf("  PASS: %s\n", name); \
        ++s_pass; \
    } \
} while(0)

// ── Helpers ──────────────────────────────────────────────────────

static bool roundtrip_mesh(const CookedMeshData& in) {
    BinaryChunkWriter writer;
    write_cooked_mesh(writer, CookedPlatform::Desktop, in);

    CookedMeshData out;
    BinaryChunkReader reader(writer.getBuffer().data(), writer.getBuffer().size());
    if (!read_cooked_mesh(reader, out)) return false;

    if (in.vertex_count  != out.vertex_count)  return false;
    if (in.index_count   != out.index_count)   return false;
    if (in.vertex_stride != out.vertex_stride) return false;
    if (in.vertex_data   != out.vertex_data)   return false;
    if (in.indices       != out.indices)       return false;
    if (in.tangent_data   != out.tangent_data)   return false;
    if (in.bitangent_data != out.bitangent_data) return false;
    // Vertex layout metadata (v4+)
    if (in.position_attrib.byte_offset     != out.position_attrib.byte_offset)     return false;
    if (in.position_attrib.byte_size       != out.position_attrib.byte_size)       return false;
    if (in.position_attrib.component_count != out.position_attrib.component_count) return false;
    if (in.normal_attrib.byte_offset       != out.normal_attrib.byte_offset)       return false;
    if (in.normal_attrib.byte_size         != out.normal_attrib.byte_size)         return false;
    if (in.normal_attrib.component_count   != out.normal_attrib.component_count)   return false;
    if (in.uv_attrib.byte_offset           != out.uv_attrib.byte_offset)           return false;
    if (in.uv_attrib.byte_size             != out.uv_attrib.byte_size)             return false;
    if (in.uv_attrib.component_count       != out.uv_attrib.component_count)       return false;
    if (in.tangent_per_vertex              != out.tangent_per_vertex)              return false;
    if (in.bitangent_per_vertex            != out.bitangent_per_vertex)            return false;

    if (in.bounds_min    != out.bounds_min)    return false;
    if (in.bounds_max    != out.bounds_max)    return false;
    if (in.bounds_center != out.bounds_center) return false;
    if (in.bounds_radius != out.bounds_radius) return false;
    return true;
}

static bool roundtrip_texture(const CookedTextureData& in) {
    BinaryChunkWriter writer;
    write_cooked_texture(writer, CookedPlatform::Desktop, in);

    CookedTextureData out;
    BinaryChunkReader reader(writer.getBuffer().data(), writer.getBuffer().size());
    if (!read_cooked_texture(reader, out)) return false;

    if (in.width     != out.width)     return false;
    if (in.height    != out.height)    return false;
    if (in.format    != out.format)    return false;
    if (in.mip_count != out.mip_count) return false;
    if (in.mip_sizes != out.mip_sizes) return false;
    if (in.mip_data  != out.mip_data)  return false;
    return true;
}

static bool roundtrip_shader(const CookedShaderData& in) {
    BinaryChunkWriter writer;
    write_cooked_shader(writer, CookedPlatform::Any, in);

    CookedShaderData out;
    BinaryChunkReader reader(writer.getBuffer().data(), writer.getBuffer().size());
    if (!read_cooked_shader(reader, out)) return false;

    if (in.identifier != out.identifier) return false;
    if (in.vert_stage != out.vert_stage) return false;
    if (in.frag_stage != out.frag_stage) return false;
    return true;
}

static bool roundtrip_material(const CookedMaterialData& in) {
    BinaryChunkWriter writer;
    write_cooked_material(writer, CookedPlatform::Desktop, in);

    CookedMaterialData out;
    BinaryChunkReader reader(writer.getBuffer().data(), writer.getBuffer().size());
    if (!read_cooked_material(reader, out)) return false;

    if (in.shader_ref        != out.shader_ref)        return false;
    if (in.render_state_flags != out.render_state_flags) return false;
    if (in.texture_bindings.size() != out.texture_bindings.size()) return false;
    for (u32 i = 0; i < in.texture_bindings.size(); ++i) {
        if (in.texture_bindings[i].slot_name   != out.texture_bindings[i].slot_name)   return false;
        if (in.texture_bindings[i].texture_ref != out.texture_bindings[i].texture_ref) return false;
    }
    if (in.uniforms.size() != out.uniforms.size()) return false;
    for (u32 i = 0; i < in.uniforms.size(); ++i) {
        if (in.uniforms[i].name  != out.uniforms[i].name)  return false;
        if (in.uniforms[i].value.type != out.uniforms[i].value.type) return false;
    }
    return true;
}

// ── Tests: version + format rejection ─────────────────────────────
static bool test_version_rejection() {
    BinaryChunkWriter writer;
    CookedMeshData dummy;
    // Write with version 999
    auto payload = [&](Serializer& s) { write_cooked_mesh_payload(s, dummy); };
    BinaryChunkWriter tmp;
    Serializer s_tmp(tmp);
    payload(s_tmp);
    auto buf = tmp.getBuffer();
    u64 hash = cooked_hash_fnv1a(buf.data(), static_cast<u32>(buf.size()));

    writer.beginChunk(CookedType::Mesh, 999);
    {
        Serializer s_real(writer);
        CookedAssetHeader h;
        h.asset_hash = hash;
        h.platform_tag = 0;
        h.flags = CAF_None;
        write_cooked_header(s_real, h);
        s_real.writeBytes(buf.data(), static_cast<u32>(buf.size()));
    }
    writer.endChunk();

    CookedMeshData result;
    BinaryChunkReader reader(writer.getBuffer().data(), writer.getBuffer().size());
    return !read_cooked_mesh(reader, result);
}

// ── Test: corrupted hash detection ───────────────────────────────
static bool test_corrupted_hash() {
    BinaryChunkWriter writer;
    CookedMeshData mesh;
    mesh.vertex_count = 4;
    mesh.vertex_stride = sizeof(Vertex);
    mesh.vertex_data.resize(4 * sizeof(Vertex), 0xAB);
    mesh.bounds_max = {1,1,1};
    mesh.bounds_min = {-1,-1,-1};
    mesh.bounds_center = {0,0,0};
    mesh.bounds_radius = 1.732f;
    write_cooked_mesh(writer, CookedPlatform::Desktop, mesh);

    // Corrupt the buffer
    auto buf = writer.getBuffer();
    if (buf.size() > 32) {
        const_cast<u8*>(buf.data())[32] ^= 0xFF;
    }

    CookedMeshData result;
    BinaryChunkReader reader(buf.data(), buf.size());
    return !read_cooked_mesh(reader, result);
}

// ═══════════════════════════════════════════════════════════════
int main() {
    printf("=== Cooked Asset Format Test ===\n\n");

    // ── CookedMesh round-trip ──
    {
        printf("-- CookedMesh --\n");
        CookedMeshData mesh;
        mesh.vertex_count  = 24;
        mesh.index_count   = 36;
        mesh.vertex_stride = sizeof(Vertex);
        mesh.vertex_data.resize(24 * sizeof(Vertex));
        mesh.indices.resize(36);
        for (u32 i = 0; i < 36; ++i) mesh.indices[i] = i;
        // Vertex layout metadata (v4+)
        mesh.position_attrib   = { 0, 12, 3 };
        mesh.normal_attrib     = { 12, 12, 3 };
        mesh.uv_attrib         = { 24, 8, 2 };
        // Explicit attribute flags
        mesh.has_positions   = true;
        mesh.has_normals     = true;
        mesh.has_uvs         = true;
        mesh.has_tangents    = true;
        mesh.has_bitangents  = true;
        mesh.tangent_per_vertex   = sizeof(glm::vec3);
        mesh.bitangent_per_vertex = sizeof(glm::vec3);
        mesh.tangent_data.resize(24 * sizeof(glm::vec3), 0xCD);
        mesh.bitangent_data.resize(24 * sizeof(glm::vec3), 0xDC);
        mesh.bounds_min    = {-1,-1,-1};
        mesh.bounds_max    = { 1, 1, 1};
        mesh.bounds_center = { 0, 0, 0};
        mesh.bounds_radius = 1.732f;

        // Mesh without optional attributes
        CookedMeshData no_tan_mesh;
        no_tan_mesh.vertex_count = 4;
        no_tan_mesh.vertex_stride = sizeof(Vertex);
        no_tan_mesh.vertex_data.resize(4 * sizeof(Vertex));
        no_tan_mesh.position_attrib = { 0, 12, 3 };
        no_tan_mesh.normal_attrib   = { 12, 12, 3 };
        no_tan_mesh.uv_attrib       = { 24, 8, 2 };
        no_tan_mesh.has_positions   = true;
        no_tan_mesh.has_normals     = true;
        no_tan_mesh.has_uvs         = true;
        no_tan_mesh.bounds_max = {1,1,1};

        TEST("empty mesh round-trips",              roundtrip_mesh(CookedMeshData{}));
        TEST("mesh with tangents round-trips",      roundtrip_mesh(mesh));
        TEST("mesh without tangents round-trips",   roundtrip_mesh(no_tan_mesh));

        // Verify flags are correctly serialized
        {
            BinaryChunkWriter writer;
            write_cooked_mesh(writer, CookedPlatform::Desktop, mesh);
            CookedMeshData out;
            BinaryChunkReader reader(writer.getBuffer().data(), writer.getBuffer().size());
            TEST("flags round-trip correctly",
                 read_cooked_mesh(reader, out) &&
                 out.has_positions  == mesh.has_positions &&
                 out.has_normals    == mesh.has_normals &&
                 out.has_uvs        == mesh.has_uvs &&
                 out.has_tangents   == mesh.has_tangents &&
                 out.has_bitangents == mesh.has_bitangents);
        }
    }

    // ── CookedTexture round-trip ──
    {
        printf("\n-- CookedTexture --\n");
        CookedTextureData tex;
        tex.width  = 256;
        tex.height = 256;
        tex.format = static_cast<u32>(CookedTextureFormat::RGBA8);
        tex.mip_count = 8;
        tex.mip_sizes = {65536, 16384, 4096, 1024, 256, 64, 16, 4};
        tex.mip_data.resize(65536 + 16384 + 4096 + 1024 + 256 + 64 + 16 + 4, 0x80);

        CookedTextureData empty_tex;
        empty_tex.width = 64;
        empty_tex.height = 64;
        empty_tex.format = static_cast<u32>(CookedTextureFormat::ETC2_RGB);
        empty_tex.mip_count = 1;
        empty_tex.mip_sizes = {0};

        TEST("texture with mipmaps round-trips",  roundtrip_texture(tex));
        TEST("texture with no mip data round-trips", roundtrip_texture(empty_tex));
    }

    // ── CookedShader round-trip ──
    {
        printf("\n-- CookedShader --\n");
        CookedShaderData shader;
        shader.identifier = "shaders/lit";
        const char* vs = "#version 300 es\nvoid main(){}";
        const char* fs = "#version 300 es\nvoid main(){}";
        shader.vert_stage.assign(vs, vs + std::strlen(vs) + 1);
        shader.frag_stage.assign(fs, fs + std::strlen(fs) + 1);

        CookedShaderData empty_shader;
        empty_shader.identifier = "shaders/empty";

        TEST("shader with source round-trips",  roundtrip_shader(shader));
        TEST("empty shader round-trips",        roundtrip_shader(empty_shader));
    }

    // ── CookedMaterial round-trip ──
    {
        printf("\n-- CookedMaterial --\n");
        CookedMaterialData mat;
        mat.shader_ref = "shaders/lit";
        mat.texture_bindings.push_back({"Diffuse", "textures/diffuse_01"});
        mat.texture_bindings.push_back({"Normal",  "textures/normal_01"});
        mat.uniforms.push_back({"u_mat_shininess",
                                []{ CookedUniformValue uv; uv.type = CookedUniformValue::Float; uv.f_val = 32.0f; return uv; }()});
        mat.uniforms.push_back({"u_mat_diffuse",
                                []{ CookedUniformValue uv; uv.type = CookedUniformValue::Vec3; uv.v3 = {0.8f, 0.2f, 0.2f}; return uv; }()});
        mat.render_state_flags = RS_DepthTest | RS_CullBack;

        CookedMaterialData empty_mat;
        empty_mat.shader_ref = "shaders/unlit";

        TEST("material with bindings round-trips", roundtrip_material(mat));
        TEST("empty material round-trips",         roundtrip_material(empty_mat));
    }

    // ── Edge cases ──
    {
        printf("\n-- Edge cases --\n");

        // Version rejection
        TEST("unknown version is rejected",  test_version_rejection());

        // Old v1 format rejected (current is v4)
        {
            BinaryChunkWriter writer;
            CookedMeshData dummy;
            auto write_v1 = [&](Serializer& s) {
                // v1 format: count + count + stride (no tangent/bitangent at all)
                s.writeUInt32(0); // vertex_count
                s.writeUInt32(0); // index_count
                s.writeUInt32(0); // vertex_stride
            };
            BinaryChunkWriter tmp;
            Serializer s_tmp(tmp);
            write_v1(s_tmp);
            auto buf = tmp.getBuffer();
            u64 hash = cooked_hash_fnv1a(buf.data(), static_cast<u32>(buf.size()));

            writer.beginChunk(CookedType::Mesh, 1); // v1
            {
                Serializer s_real(writer);
                CookedAssetHeader h;
                h.asset_hash = hash; h.platform_tag = 0; h.flags = CAF_None;
                write_cooked_header(s_real, h);
                s_real.writeBytes(buf.data(), static_cast<u32>(buf.size()));
            }
            writer.endChunk();

            CookedMeshData result;
            BinaryChunkReader reader(writer.getBuffer().data(), writer.getBuffer().size());
            TEST("old v1 format is rejected", !read_cooked_mesh(reader, result));
        }

        // Old v2 format rejected (missing explicit attribute flags)
        {
            BinaryChunkWriter writer;
            CookedMeshData dummy;
            auto write_v2 = [&](Serializer& s) {
                // v2 format: count + count + stride + data fields + has_tangents(bool) + data
                s.writeUInt32(0); // vertex_count
                s.writeUInt32(0); // index_count
                s.writeUInt32(0); // vertex_stride
                s.writeBool(false); // has_tangents (v2 bool, no explicit flags)
            };
            BinaryChunkWriter tmp;
            Serializer s_tmp(tmp);
            write_v2(s_tmp);
            auto buf = tmp.getBuffer();
            u64 hash = cooked_hash_fnv1a(buf.data(), static_cast<u32>(buf.size()));

            writer.beginChunk(CookedType::Mesh, 2); // v2
            {
                Serializer s_real(writer);
                CookedAssetHeader h;
                h.asset_hash = hash; h.platform_tag = 0; h.flags = CAF_None;
                write_cooked_header(s_real, h);
                s_real.writeBytes(buf.data(), static_cast<u32>(buf.size()));
            }
            writer.endChunk();

            CookedMeshData result;
            BinaryChunkReader reader(writer.getBuffer().data(), writer.getBuffer().size());
            TEST("old v2 format is rejected", !read_cooked_mesh(reader, result));
        }

        // Old v3 format rejected (missing vertex layout metadata)
        {
            BinaryChunkWriter writer;
            CookedMeshData dummy;
            auto write_v3 = [&](Serializer& s) {
                // v3 format: count + count + stride + data + flags(5×bool) + optional data + bounds
                s.writeUInt32(0); // vertex_count
                s.writeUInt32(0); // index_count
                s.writeUInt32(0); // vertex_stride
                s.writeBool(true);  // has_positions
                s.writeBool(true);  // has_normals
                s.writeBool(true);  // has_uvs
                s.writeBool(false); // has_tangents
                s.writeBool(false); // has_bitangents
                s.writeVec3({0,0,0}); // bounds_min
                s.writeVec3({0,0,0}); // bounds_max
                s.writeVec3({0,0,0}); // bounds_center
                s.writeFloat(0);      // bounds_radius
            };
            BinaryChunkWriter tmp;
            Serializer s_tmp(tmp);
            write_v3(s_tmp);
            auto buf = tmp.getBuffer();
            u64 hash = cooked_hash_fnv1a(buf.data(), static_cast<u32>(buf.size()));

            writer.beginChunk(CookedType::Mesh, 3); // v3
            {
                Serializer s_real(writer);
                CookedAssetHeader h;
                h.asset_hash = hash; h.platform_tag = 0; h.flags = CAF_None;
                write_cooked_header(s_real, h);
                s_real.writeBytes(buf.data(), static_cast<u32>(buf.size()));
            }
            writer.endChunk();

            CookedMeshData result;
            BinaryChunkReader reader(writer.getBuffer().data(), writer.getBuffer().size());
            TEST("old v3 format is rejected", !read_cooked_mesh(reader, result));
        }

        // Corrupted hash
        TEST("corrupted hash is detected",   test_corrupted_hash());

        // Header size verification: CookedAssetHeader = 4 × u32 = 16 bytes
        {
            BinaryChunkWriter writer;
            CookedMeshData dummy;
            write_cooked_mesh(writer, CookedPlatform::Desktop, dummy);
            const auto& buf = writer.getBuffer();

            // Chunk header: 16 bytes
            // Asset header: 16 bytes (hash_lo + hash_hi + platform + flags)
            // Nested chunk: remaining
            bool header_size_ok = buf.size() >= 32;
            bool offset_ok = false;
            if (header_size_ok) {
                // Verify the chunk header magic
                u32 magic = 0;
                std::memcpy(&magic, buf.data(), 4);
                offset_ok = (magic == kChunkMagic);
            }
            TEST("header layout: chunk(16) + asset_header(16) + nested_chunk", header_size_ok && offset_ok);
        }

        // Flags field round-trip via byte inspection
        {
            BinaryChunkWriter writer;
            CookedMeshData dummy;
            write_cooked_mesh(writer, CookedPlatform::Desktop, dummy);
            const auto& buf = writer.getBuffer();

            // Chunk header (bytes 0-15). Payload starts at byte 16.
            // Payload layout:
            //   hash_lo  = bytes 16-19  (offset 0 from payload start)
            //   hash_hi  = bytes 20-23  (offset 4)
            //   platform = bytes 24-27  (offset 8)
            //   flags    = bytes 28-31  (offset 12)
            //   nested chunk = bytes 32+
            u32 flags_raw = 0;
            std::memcpy(&flags_raw, buf.data() + 28, 4);
            TEST("flags field position and default value", flags_raw == CAF_None);
        }

        // Flag bits compose correctly
        {
            u32 composed = CAF_Compressed | CAF_Streamable | CAF_EditorOnly;
            TEST("flag bit composition",
                 (composed & CAF_Compressed)  != 0 &&
                 (composed & CAF_Streamable)  != 0 &&
                 (composed & CAF_EditorOnly)  != 0 &&
                 (composed & CAF_Encrypted)   == 0 &&
                 (composed & CAF_HasDependencies) == 0);
        }

        // Truncated data rejected (incomplete chunk header)
        {
            BinaryChunkWriter writer;
            CookedMeshData dummy;
            write_cooked_mesh(writer, CookedPlatform::Desktop, dummy);
            const auto& buf = writer.getBuffer();
            std::vector<u8> truncated(buf.begin(), buf.begin() + 15);  // only 15 bytes

            CookedMeshData result;
            BinaryChunkReader reader(truncated.data(), static_cast<u32>(truncated.size()));
            TEST("truncated data rejected", !read_cooked_mesh(reader, result));
        }

        // Wrong type_id rejected
        {
            BinaryChunkWriter writer;
            CookedTextureData tex;
            tex.width = 64; tex.height = 64;
            tex.format = static_cast<u32>(CookedTextureFormat::RGBA8);
            tex.mip_count = 1;
            tex.mip_sizes = {0};
            write_cooked_texture(writer, CookedPlatform::Desktop, tex);

            CookedMeshData result;
            BinaryChunkReader reader(writer.getBuffer().data(), writer.getBuffer().size());
            TEST("wrong type ID rejected", !read_cooked_mesh(reader, result));
        }
    }

    // ── Summary ──
    printf("\n=== Results: %d passed, %d failed ===\n", s_pass, s_fail);
    return s_fail > 0 ? 1 : 0;
}
