#include "engine/serialization/cooked_asset.h"
#include "engine/core/log.h"
#include <cstring>

namespace pino {

// ── FNV-1a 64-bit ───────────────────────────────────────────────
u64 cooked_hash_fnv1a(const void* data, u32 size) {
    u64 h = 0xCBF29CE484222325ull;
    const u8* p = static_cast<const u8*>(data);
    for (u32 i = 0; i < size; ++i) {
        h ^= p[i];
        h *= 0x100000001B3ull;
    }
    return h;
}

// ── Common header helpers ────────────────────────────────────────
void write_cooked_header(Serializer& s, const CookedAssetHeader& header) {
    s.writeUInt32(static_cast<u32>(header.asset_hash & 0xFFFFFFFFu));
    s.writeUInt32(static_cast<u32>(header.asset_hash >> 32));
    s.writeUInt32(header.platform_tag);
    s.writeUInt32(header.flags);
}

CookedAssetHeader read_cooked_header(Deserializer& d) {
    CookedAssetHeader h;
    u32 lo = d.readUInt32();
    u32 hi = d.readUInt32();
    h.asset_hash   = (static_cast<u64>(hi) << 32) | lo;
    h.platform_tag = d.readUInt32();
    h.flags        = d.readUInt32();
    return h;
}

// ═══════════════════════════════════════════════════════════════
//  CookedMesh payload
// ═══════════════════════════════════════════════════════════════

void write_cooked_mesh_payload(Serializer& s, const CookedMeshData& mesh) {
    s.writeUInt32(mesh.vertex_count);
    s.writeUInt32(mesh.index_count);
    s.writeUInt32(mesh.vertex_stride);

    // Vertex layout metadata (v4+) — before vertex_data so consumers can interpret it
    s.writeUInt32(mesh.position_attrib.byte_offset);
    s.writeUInt32(mesh.position_attrib.byte_size);
    s.writeUInt32(mesh.position_attrib.component_count);
    s.writeUInt32(mesh.normal_attrib.byte_offset);
    s.writeUInt32(mesh.normal_attrib.byte_size);
    s.writeUInt32(mesh.normal_attrib.component_count);
    s.writeUInt32(mesh.uv_attrib.byte_offset);
    s.writeUInt32(mesh.uv_attrib.byte_size);
    s.writeUInt32(mesh.uv_attrib.component_count);

    u32 vert_bytes = mesh.vertex_count * mesh.vertex_stride;
    if (vert_bytes > 0)
        s.writeBytes(mesh.vertex_data.data(), vert_bytes);

    if (mesh.index_count > 0)
        s.writeBytes(mesh.indices.data(), mesh.index_count * sizeof(u32));

    // Explicit attribute presence flags (v3+)
    s.writeBool(mesh.has_positions);
    s.writeBool(mesh.has_normals);
    s.writeBool(mesh.has_uvs);
    s.writeBool(mesh.has_tangents);
    s.writeBool(mesh.has_bitangents);

    // Per-vertex sizes for optional attributes (v4+)
    s.writeUInt32(mesh.tangent_per_vertex);
    s.writeUInt32(mesh.bitangent_per_vertex);

    // Tangent/bitangent data (present only when flags are set)
    if (mesh.has_tangents && !mesh.tangent_data.empty())
        s.writeBytes(mesh.tangent_data.data(), static_cast<u32>(mesh.tangent_data.size()));
    if (mesh.has_bitangents && !mesh.bitangent_data.empty())
        s.writeBytes(mesh.bitangent_data.data(), static_cast<u32>(mesh.bitangent_data.size()));

    s.writeVec3(mesh.bounds_min);
    s.writeVec3(mesh.bounds_max);
    s.writeVec3(mesh.bounds_center);
    s.writeFloat(mesh.bounds_radius);
}

void read_cooked_mesh_payload(Deserializer& d, CookedMeshData& mesh) {
    mesh.vertex_count   = d.readUInt32();
    mesh.index_count    = d.readUInt32();
    mesh.vertex_stride  = d.readUInt32();

    // Vertex layout metadata (v4+)
    mesh.position_attrib.byte_offset     = d.readUInt32();
    mesh.position_attrib.byte_size       = d.readUInt32();
    mesh.position_attrib.component_count = d.readUInt32();
    mesh.normal_attrib.byte_offset       = d.readUInt32();
    mesh.normal_attrib.byte_size         = d.readUInt32();
    mesh.normal_attrib.component_count   = d.readUInt32();
    mesh.uv_attrib.byte_offset           = d.readUInt32();
    mesh.uv_attrib.byte_size             = d.readUInt32();
    mesh.uv_attrib.component_count       = d.readUInt32();

    u32 vert_bytes = mesh.vertex_count * mesh.vertex_stride;
    mesh.vertex_data.resize(vert_bytes);
    if (vert_bytes > 0)
        d.readBytes(mesh.vertex_data.data(), vert_bytes);

    mesh.indices.resize(mesh.index_count);
    if (mesh.index_count > 0)
        d.readBytes(mesh.indices.data(), mesh.index_count * sizeof(u32));

    // Read explicit attribute flags (v3+)
    mesh.has_positions   = d.readBool();
    mesh.has_normals     = d.readBool();
    mesh.has_uvs         = d.readBool();
    mesh.has_tangents    = d.readBool();
    mesh.has_bitangents  = d.readBool();

    // Per-vertex sizes for optional attributes (v4+)
    mesh.tangent_per_vertex   = d.readUInt32();
    mesh.bitangent_per_vertex = d.readUInt32();

    // Tangent/bitangent data (sized using explicit per-vertex sizes)
    if (mesh.has_tangents) {
        u32 tangent_bytes = mesh.vertex_count * mesh.tangent_per_vertex;
        mesh.tangent_data.resize(tangent_bytes);
        if (tangent_bytes > 0)
            d.readBytes(mesh.tangent_data.data(), tangent_bytes);
    }
    if (mesh.has_bitangents) {
        u32 bitangent_bytes = mesh.vertex_count * mesh.bitangent_per_vertex;
        mesh.bitangent_data.resize(bitangent_bytes);
        if (bitangent_bytes > 0)
            d.readBytes(mesh.bitangent_data.data(), bitangent_bytes);
    }

    mesh.bounds_min    = d.readVec3();
    mesh.bounds_max    = d.readVec3();
    mesh.bounds_center = d.readVec3();
    mesh.bounds_radius = d.readFloat();
}

// ── v1 backward-compatible reader (no flags, no tangents) ─────────
// Upgrades to v4 in-memory with standard layout defaults.
static void read_cooked_mesh_payload_v1(Deserializer& d, CookedMeshData& mesh) {
    mesh.vertex_count   = d.readUInt32();
    mesh.index_count    = d.readUInt32();
    mesh.vertex_stride  = d.readUInt32();

    u32 vert_bytes = mesh.vertex_count * mesh.vertex_stride;
    mesh.vertex_data.resize(vert_bytes);
    if (vert_bytes > 0)
        d.readBytes(mesh.vertex_data.data(), vert_bytes);

    mesh.indices.resize(mesh.index_count);
    if (mesh.index_count > 0)
        d.readBytes(mesh.indices.data(), mesh.index_count * sizeof(u32));

    // v1 assumed standard interleaved Vertex layout
    mesh.position_attrib   = { 0, 12, 3 };
    mesh.normal_attrib     = { 12, 12, 3 };
    mesh.uv_attrib         = { 24, 8, 2 };
    mesh.has_positions     = true;
    mesh.has_normals       = true;
    mesh.has_uvs           = true;
    mesh.has_tangents      = false;
    mesh.has_bitangents    = false;
    mesh.tangent_per_vertex   = 12;
    mesh.bitangent_per_vertex = 12;

    mesh.bounds_min    = d.readVec3();
    mesh.bounds_max    = d.readVec3();
    mesh.bounds_center = d.readVec3();
    mesh.bounds_radius = d.readFloat();
}

// ── v2 backward-compatible reader (single has_tangents bool, no explicit flags) ──
// Upgrades to v4 in-memory with standard layout defaults.
static void read_cooked_mesh_payload_v2(Deserializer& d, CookedMeshData& mesh) {
    mesh.vertex_count   = d.readUInt32();
    mesh.index_count    = d.readUInt32();
    mesh.vertex_stride  = d.readUInt32();

    u32 vert_bytes = mesh.vertex_count * mesh.vertex_stride;
    mesh.vertex_data.resize(vert_bytes);
    if (vert_bytes > 0)
        d.readBytes(mesh.vertex_data.data(), vert_bytes);

    mesh.indices.resize(mesh.index_count);
    if (mesh.index_count > 0)
        d.readBytes(mesh.indices.data(), mesh.index_count * sizeof(u32));

    // v2 assumed standard interleaved Vertex layout
    mesh.position_attrib   = { 0, 12, 3 };
    mesh.normal_attrib     = { 12, 12, 3 };
    mesh.uv_attrib         = { 24, 8, 2 };
    mesh.has_positions     = true;
    mesh.has_normals       = true;
    mesh.has_uvs           = true;
    mesh.tangent_per_vertex   = 12;
    mesh.bitangent_per_vertex = 12;

    // v2: single has_tangents bool (paired tangents+bitangents)
    bool has_tangents = d.readBool();
    mesh.has_tangents    = has_tangents;
    mesh.has_bitangents  = has_tangents;

    if (has_tangents) {
        u32 attr_bytes = mesh.vertex_count * sizeof(glm::vec3);
        mesh.tangent_data.resize(attr_bytes);
        mesh.bitangent_data.resize(attr_bytes);
        if (attr_bytes > 0) {
            d.readBytes(mesh.tangent_data.data(), attr_bytes);
            d.readBytes(mesh.bitangent_data.data(), attr_bytes);
        }
    }

    mesh.bounds_min    = d.readVec3();
    mesh.bounds_max    = d.readVec3();
    mesh.bounds_center = d.readVec3();
    mesh.bounds_radius = d.readFloat();
}

// ═══════════════════════════════════════════════════════════════
//  CookedTexture payload
// ═══════════════════════════════════════════════════════════════

void write_cooked_texture_payload(Serializer& s, const CookedTextureData& tex) {
    s.writeUInt32(tex.width);
    s.writeUInt32(tex.height);
    s.writeUInt32(tex.format);
    s.writeUInt32(tex.mip_count);

    for (u32 i = 0; i < tex.mip_count; ++i)
        s.writeUInt32(tex.mip_sizes[i]);

    if (!tex.mip_data.empty())
        s.writeBytes(tex.mip_data.data(), static_cast<u32>(tex.mip_data.size()));
}

void read_cooked_texture_payload(Deserializer& d, CookedTextureData& tex) {
    tex.width      = d.readUInt32();
    tex.height     = d.readUInt32();
    tex.format     = d.readUInt32();
    tex.mip_count  = d.readUInt32();

    tex.mip_sizes.resize(tex.mip_count);
    u32 total_mip_bytes = 0;
    for (u32 i = 0; i < tex.mip_count; ++i) {
        tex.mip_sizes[i] = d.readUInt32();
        total_mip_bytes += tex.mip_sizes[i];
    }

    tex.mip_data.resize(total_mip_bytes);
    if (total_mip_bytes > 0)
        d.readBytes(tex.mip_data.data(), total_mip_bytes);
}

// ═══════════════════════════════════════════════════════════════
//  CookedShader payload
// ═══════════════════════════════════════════════════════════════

void write_cooked_shader_payload(Serializer& s, const CookedShaderData& shader) {
    s.writeString(shader.identifier);

    u32 vs_size = static_cast<u32>(shader.vert_stage.size());
    u32 fs_size = static_cast<u32>(shader.frag_stage.size());
    s.writeUInt32(vs_size);
    if (vs_size > 0)
        s.writeBytes(shader.vert_stage.data(), vs_size);
    s.writeUInt32(fs_size);
    if (fs_size > 0)
        s.writeBytes(shader.frag_stage.data(), fs_size);
}

void read_cooked_shader_payload(Deserializer& d, CookedShaderData& shader) {
    shader.identifier = d.readString();

    u32 vs_size = d.readUInt32();
    shader.vert_stage.resize(vs_size);
    if (vs_size > 0)
        d.readBytes(shader.vert_stage.data(), vs_size);

    u32 fs_size = d.readUInt32();
    shader.frag_stage.resize(fs_size);
    if (fs_size > 0)
        d.readBytes(shader.frag_stage.data(), fs_size);
}

// ═══════════════════════════════════════════════════════════════
//  CookedMaterial payload
// ═══════════════════════════════════════════════════════════════

static void write_cooked_uniform_value(Serializer& s, const CookedUniformValue& uv) {
    s.writeUInt32(static_cast<u32>(uv.type));
    switch (uv.type) {
        case CookedUniformValue::Int:   s.writeInt32(uv.i_val); break;
        case CookedUniformValue::Float: s.writeFloat(uv.f_val); break;
        case CookedUniformValue::Vec3:  s.writeVec3(uv.v3); break;
        case CookedUniformValue::Vec4:  s.writeVec4(uv.v4); break;
        case CookedUniformValue::Mat3:  s.writeBytes(&uv.m3, sizeof(glm::mat3)); break;
        case CookedUniformValue::Mat4:  s.writeBytes(&uv.m4, sizeof(glm::mat4)); break;
        default: break;
    }
}

static CookedUniformValue read_cooked_uniform_value(Deserializer& d) {
    CookedUniformValue uv;
    uv.type = static_cast<CookedUniformValue::Type>(d.readUInt32());
    switch (uv.type) {
        case CookedUniformValue::Int:   uv.i_val = d.readInt32(); break;
        case CookedUniformValue::Float: uv.f_val = d.readFloat(); break;
        case CookedUniformValue::Vec3:  uv.v3 = d.readVec3(); break;
        case CookedUniformValue::Vec4:  uv.v4 = d.readVec4(); break;
        case CookedUniformValue::Mat3:  d.readBytes(&uv.m3, sizeof(glm::mat3)); break;
        case CookedUniformValue::Mat4:  d.readBytes(&uv.m4, sizeof(glm::mat4)); break;
        default: break;
    }
    return uv;
}

void write_cooked_material_payload(Serializer& s, const CookedMaterialData& mat) {
    s.writeString(mat.shader_ref);

    s.writeUInt32(static_cast<u32>(mat.texture_bindings.size()));
    for (const auto& tb : mat.texture_bindings) {
        s.writeString(tb.slot_name);
        s.writeString(tb.texture_ref);
    }

    s.writeUInt32(static_cast<u32>(mat.uniforms.size()));
    for (const auto& u : mat.uniforms) {
        s.writeString(u.name);
        write_cooked_uniform_value(s, u.value);
    }

    s.writeUInt32(mat.render_state_flags);
}

void read_cooked_material_payload(Deserializer& d, CookedMaterialData& mat) {
    mat.shader_ref = d.readString();

    u32 tex_count = d.readUInt32();
    mat.texture_bindings.resize(tex_count);
    for (u32 i = 0; i < tex_count; ++i) {
        mat.texture_bindings[i].slot_name   = d.readString();
        mat.texture_bindings[i].texture_ref = d.readString();
    }

    u32 uniform_count = d.readUInt32();
    mat.uniforms.resize(uniform_count);
    for (u32 i = 0; i < uniform_count; ++i) {
        mat.uniforms[i].name  = d.readString();
        mat.uniforms[i].value = read_cooked_uniform_value(d);
    }

    mat.render_state_flags = d.readUInt32();
}

// ═══════════════════════════════════════════════════════════════
//  High-level chunk serialization
// ═══════════════════════════════════════════════════════════════
//
// Each cooked asset is written as a BinaryChunk containing:
//   1. CookedAssetHeader (3 × u32 = 12 bytes)
//   2. A nested mini-chunk wrapping the type-specific payload
//
// The integrity hash covers the entire nested mini-chunk (header + fields).
// The nested chunk allows Deserializer to navigate the payload naturally.

void write_cooked_mesh(BinaryChunkWriter& writer, CookedPlatform platform,
                       const CookedMeshData& mesh)
{
    // Build nested mini-chunk containing the payload
    BinaryChunkWriter tmp;
    {
        Serializer s(tmp);
        s.beginChunk(0, 0); // dummy type/version — framing only
        write_cooked_mesh_payload(s, mesh);
        s.endChunk();
    }
    const auto& nested = tmp.getBuffer();
    u64 hash = cooked_hash_fnv1a(nested.data(), static_cast<u32>(nested.size()));

    writer.beginChunk(CookedType::Mesh, CookedVersion::Mesh);
    {
        Serializer s(writer);
        CookedAssetHeader h;
        h.asset_hash   = hash;
        h.platform_tag = static_cast<u32>(platform);
        h.flags        = CAF_None;
        write_cooked_header(s, h);
        s.writeBytes(nested.data(), static_cast<u32>(nested.size()));
    }
    writer.endChunk();
}

void write_cooked_texture(BinaryChunkWriter& writer, CookedPlatform platform,
                          const CookedTextureData& tex)
{
    BinaryChunkWriter tmp;
    {
        Serializer s(tmp);
        s.beginChunk(0, 0);
        write_cooked_texture_payload(s, tex);
        s.endChunk();
    }
    const auto& nested = tmp.getBuffer();
    u64 hash = cooked_hash_fnv1a(nested.data(), static_cast<u32>(nested.size()));

    writer.beginChunk(CookedType::Texture, CookedVersion::Texture);
    {
        Serializer s(writer);
        CookedAssetHeader h;
        h.asset_hash   = hash;
        h.platform_tag = static_cast<u32>(platform);
        h.flags        = CAF_None;
        write_cooked_header(s, h);
        s.writeBytes(nested.data(), static_cast<u32>(nested.size()));
    }
    writer.endChunk();
}

void write_cooked_shader(BinaryChunkWriter& writer, CookedPlatform platform,
                         const CookedShaderData& shader)
{
    BinaryChunkWriter tmp;
    {
        Serializer s(tmp);
        s.beginChunk(0, 0);
        write_cooked_shader_payload(s, shader);
        s.endChunk();
    }
    const auto& nested = tmp.getBuffer();
    u64 hash = cooked_hash_fnv1a(nested.data(), static_cast<u32>(nested.size()));

    writer.beginChunk(CookedType::Shader, CookedVersion::Shader);
    {
        Serializer s(writer);
        CookedAssetHeader h;
        h.asset_hash   = hash;
        h.platform_tag = static_cast<u32>(platform);
        h.flags        = CAF_None;
        write_cooked_header(s, h);
        s.writeBytes(nested.data(), static_cast<u32>(nested.size()));
    }
    writer.endChunk();
}

void write_cooked_material(BinaryChunkWriter& writer, CookedPlatform platform,
                           const CookedMaterialData& mat)
{
    BinaryChunkWriter tmp;
    {
        Serializer s(tmp);
        s.beginChunk(0, 0);
        write_cooked_material_payload(s, mat);
        s.endChunk();
    }
    const auto& nested = tmp.getBuffer();
    u64 hash = cooked_hash_fnv1a(nested.data(), static_cast<u32>(nested.size()));

    writer.beginChunk(CookedType::Material, CookedVersion::Material);
    {
        Serializer s(writer);
        CookedAssetHeader h;
        h.asset_hash   = hash;
        h.platform_tag = static_cast<u32>(platform);
        write_cooked_header(s, h);
        s.writeBytes(nested.data(), static_cast<u32>(nested.size()));
    }
    writer.endChunk();
}

// ── Read helpers ─────────────────────────────────────────────────
//
// Reads the outer chunk, validates type/version, reads the common
// header, extracts and verifies the nested mini-chunk, then
// deserializes the payload fields through Deserializer.

bool read_cooked_mesh(BinaryChunkReader& reader, CookedMeshData& mesh) {
    if (!reader.nextChunk()) { PINO_ERROR("Cooked mesh: no chunk"); return false; }
    const ChunkHeader& hdr = reader.getHeader();
    if (hdr.type_id != CookedType::Mesh) { PINO_ERROR("Cooked mesh: bad type %u", hdr.type_id); return false; }
    if (hdr.version != 1 && hdr.version != 2 && hdr.version != CookedVersion::Mesh) {
        PINO_ERROR("Cooked mesh: unsupported version %u", hdr.version); return false;
    }

    u32 hash_lo = reader.readUInt32();
    u32 hash_hi = reader.readUInt32();
    u32 platform = reader.readUInt32(); (void)platform;
    u32 flags    = reader.readUInt32(); (void)flags;
    u64 expected_hash = (static_cast<u64>(hash_hi) << 32) | hash_lo;

    if (hdr.size < 16) { PINO_ERROR("Cooked mesh: chunk too small (%u bytes)", hdr.size); return false; }
    u32 nested_size = hdr.size - 16;
    std::vector<u8> nested(nested_size);
    if (nested_size > 0) reader.readBytes(nested.data(), nested_size);

    u64 actual_hash = cooked_hash_fnv1a(nested.data(), nested_size);
    if (actual_hash != expected_hash) { PINO_ERROR("Cooked mesh: hash mismatch"); return false; }

    BinaryChunkReader nested_reader(nested.data(), nested_size);
    Deserializer d(nested_reader);
    if (!d.nextChunk()) { PINO_ERROR("Cooked mesh: nested chunk missing"); return false; }

    switch (hdr.version) {
        case 1: read_cooked_mesh_payload_v1(d, mesh); break;
        case 2: read_cooked_mesh_payload_v2(d, mesh); break;
        default: read_cooked_mesh_payload(d, mesh); break;
    }
    return d.isValid();
}

bool read_cooked_texture(BinaryChunkReader& reader, CookedTextureData& tex) {
    if (!reader.nextChunk()) { PINO_ERROR("Cooked tex: no chunk"); return false; }
    const ChunkHeader& hdr = reader.getHeader();
    if (hdr.type_id != CookedType::Texture) { PINO_ERROR("Cooked tex: bad type %u", hdr.type_id); return false; }
    if (hdr.version != CookedVersion::Texture) { PINO_ERROR("Cooked tex: bad version %u", hdr.version); return false; }

    u32 hash_lo = reader.readUInt32();
    u32 hash_hi = reader.readUInt32();
    u32 platform = reader.readUInt32(); (void)platform;
    u32 flags    = reader.readUInt32(); (void)flags;
    u64 expected_hash = (static_cast<u64>(hash_hi) << 32) | hash_lo;

    if (hdr.size < 16) { PINO_ERROR("Cooked tex: chunk too small (%u bytes)", hdr.size); return false; }
    u32 nested_size = hdr.size - 16;
    std::vector<u8> nested(nested_size);
    if (nested_size > 0) reader.readBytes(nested.data(), nested_size);

    u64 actual_hash = cooked_hash_fnv1a(nested.data(), nested_size);
    if (actual_hash != expected_hash) { PINO_ERROR("Cooked tex: hash mismatch"); return false; }

    BinaryChunkReader nested_reader(nested.data(), nested_size);
    Deserializer d(nested_reader);
    if (!d.nextChunk()) { PINO_ERROR("Cooked tex: nested chunk missing"); return false; }
    read_cooked_texture_payload(d, tex);
    return d.isValid();
}

bool read_cooked_shader(BinaryChunkReader& reader, CookedShaderData& shader) {
    if (!reader.nextChunk()) { PINO_ERROR("Cooked shader: no chunk"); return false; }
    const ChunkHeader& hdr = reader.getHeader();
    if (hdr.type_id != CookedType::Shader) { PINO_ERROR("Cooked shader: bad type %u", hdr.type_id); return false; }
    if (hdr.version != CookedVersion::Shader) { PINO_ERROR("Cooked shader: bad version %u", hdr.version); return false; }

    u32 hash_lo = reader.readUInt32();
    u32 hash_hi = reader.readUInt32();
    u32 platform = reader.readUInt32(); (void)platform;
    u32 flags    = reader.readUInt32(); (void)flags;
    u64 expected_hash = (static_cast<u64>(hash_hi) << 32) | hash_lo;

    if (hdr.size < 16) { PINO_ERROR("Cooked shader: chunk too small (%u bytes)", hdr.size); return false; }
    u32 nested_size = hdr.size - 16;
    std::vector<u8> nested(nested_size);
    if (nested_size > 0) reader.readBytes(nested.data(), nested_size);

    u64 actual_hash = cooked_hash_fnv1a(nested.data(), nested_size);
    if (actual_hash != expected_hash) { PINO_ERROR("Cooked shader: hash mismatch"); return false; }

    BinaryChunkReader nested_reader(nested.data(), nested_size);
    Deserializer d(nested_reader);
    if (!d.nextChunk()) { PINO_ERROR("Cooked shader: nested chunk missing"); return false; }
    read_cooked_shader_payload(d, shader);
    return d.isValid();
}

bool read_cooked_material(BinaryChunkReader& reader, CookedMaterialData& mat) {
    if (!reader.nextChunk()) { PINO_ERROR("Cooked mat: no chunk"); return false; }
    const ChunkHeader& hdr = reader.getHeader();
    if (hdr.type_id != CookedType::Material) { PINO_ERROR("Cooked mat: bad type %u", hdr.type_id); return false; }
    if (hdr.version != CookedVersion::Material) { PINO_ERROR("Cooked mat: bad version %u", hdr.version); return false; }

    u32 hash_lo = reader.readUInt32();
    u32 hash_hi = reader.readUInt32();
    u32 platform = reader.readUInt32(); (void)platform;
    u32 flags    = reader.readUInt32(); (void)flags;
    u64 expected_hash = (static_cast<u64>(hash_hi) << 32) | hash_lo;

    if (hdr.size < 16) { PINO_ERROR("Cooked mat: chunk too small (%u bytes)", hdr.size); return false; }
    u32 nested_size = hdr.size - 16;
    std::vector<u8> nested(nested_size);
    if (nested_size > 0) reader.readBytes(nested.data(), nested_size);

    u64 actual_hash = cooked_hash_fnv1a(nested.data(), nested_size);
    if (actual_hash != expected_hash) { PINO_ERROR("Cooked mat: hash mismatch"); return false; }

    BinaryChunkReader nested_reader(nested.data(), nested_size);
    Deserializer d(nested_reader);
    if (!d.nextChunk()) { PINO_ERROR("Cooked mat: nested chunk missing"); return false; }
    read_cooked_material_payload(d, mat);
    return d.isValid();
}

} // namespace pino
