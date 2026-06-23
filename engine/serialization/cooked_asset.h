#pragma once

#include "engine/core/types.h"
#include "engine/core/serializer.h"
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace pino {

// ── Platform target tags ─────────────────────────────────────────
enum class CookedPlatform : u32 {
    Any     = 0,
    Desktop = 1,
    Android = 2,
    iOS     = 3,
};

// ── Cooked asset type identifiers ────────────────────────────────
namespace CookedType {
    static constexpr u32 Mesh     = 400;
    static constexpr u32 Texture  = 401;
    static constexpr u32 Shader   = 402;
    static constexpr u32 Material = 403;
}

// ── Version constants ────────────────────────────────────────────
namespace CookedVersion {
    static constexpr u32 Mesh     = 1;
    static constexpr u32 Texture  = 1;
    static constexpr u32 Shader   = 1;
    static constexpr u32 Material = 1;
}

// ── Texture pixel format ─────────────────────────────────────────
enum class CookedTextureFormat : u32 {
    RGBA8      = 0,
    BC1        = 1,
    BC3        = 2,
    BC5        = 3,
    ETC2_RGB   = 4,
    ETC2_RGBA  = 5,
    PVRTC_RGB  = 6,
    PVRTC_RGBA = 7,
};

// ── Render state flags ───────────────────────────────────────────
enum CookedRenderState : u32 {
    RS_BlendEnable    = 1u << 0,
    RS_DepthTest      = 1u << 1,
    RS_DepthWrite     = 1u << 2,
    RS_CullBack       = 1u << 3,
    RS_CullFront      = 1u << 4,
};

// ── Asset header flags ───────────────────────────────────────────
enum CookedAssetFlag : u32 {
    CAF_None          = 0u,
    CAF_Compressed    = 1u << 0,
    CAF_Encrypted     = 1u << 1,
    CAF_Streamable    = 1u << 2,
    CAF_EditorOnly    = 1u << 3,
    CAF_HasDependencies = 1u << 4,
};

// ── Common header at the start of every cooked asset payload ────
struct CookedAssetHeader {
    u64    asset_hash;
    u32    platform_tag;
    u32    flags;
};

// ── CookedMesh ───────────────────────────────────────────────────
struct CookedMeshData {
    u32             vertex_count  = 0;
    u32             index_count   = 0;
    u32             vertex_stride = 0;
    std::vector<u8> vertex_data;
    std::vector<u32> indices;
    glm::vec3       bounds_min    = glm::vec3(0.0f);
    glm::vec3       bounds_max    = glm::vec3(0.0f);
    glm::vec3       bounds_center = glm::vec3(0.0f);
    float           bounds_radius = 0.0f;
};

// ── CookedTexture ────────────────────────────────────────────────
struct CookedTextureData {
    u32              width    = 0;
    u32              height   = 0;
    u32              format   = 0;
    u32              mip_count = 1;
    std::vector<u32> mip_sizes;
    std::vector<u8>  mip_data;
};

// ── CookedShader ─────────────────────────────────────────────────
struct CookedShaderData {
    std::string     identifier;
    std::vector<u8> vert_stage;
    std::vector<u8> frag_stage;
};

// ── CookedMaterial ───────────────────────────────────────────────
struct CookedTextureBinding {
    std::string slot_name;
    std::string texture_ref;
};

struct CookedUniformValue {
    enum Type : u8 { None, Int, Float, Vec3, Vec4, Mat3, Mat4 };
    Type      type = None;
    i32       i_val = 0;
    float     f_val = 0.0f;
    glm::vec3 v3{0.0f};
    glm::vec4 v4{0.0f};
    glm::mat3 m3{1.0f};
    glm::mat4 m4{1.0f};
};

struct CookedUniformDefault {
    std::string        name;
    CookedUniformValue value;
};

struct CookedMaterialData {
    std::string                        shader_ref;
    std::vector<CookedTextureBinding>  texture_bindings;
    std::vector<CookedUniformDefault>  uniforms;
    u32                                render_state_flags = RS_DepthTest | RS_CullBack;
};

// ── FNV-1a 64-bit hash for integrity validation ─────────────────
u64 cooked_hash_fnv1a(const void* data, u32 size);

// ── Low-level serialization helpers ──────────────────────────────
void write_cooked_header(Serializer& s, const CookedAssetHeader& header);
CookedAssetHeader read_cooked_header(Deserializer& d);

// ── Per-type serialization ───────────────────────────────────────
// Writes/reads the type-specific payload (no chunk header, no common header).
void write_cooked_mesh_payload(Serializer& s, const CookedMeshData& mesh);
void read_cooked_mesh_payload(Deserializer& d, CookedMeshData& mesh);

void write_cooked_texture_payload(Serializer& s, const CookedTextureData& tex);
void read_cooked_texture_payload(Deserializer& d, CookedTextureData& tex);

void write_cooked_shader_payload(Serializer& s, const CookedShaderData& shader);
void read_cooked_shader_payload(Deserializer& d, CookedShaderData& shader);

void write_cooked_material_payload(Serializer& s, const CookedMaterialData& mat);
void read_cooked_material_payload(Deserializer& d, CookedMaterialData& mat);

// ── High-level chunk-level serialization ─────────────────────────
// Opens a chunk, writes common header + payload, closes chunk.
void write_cooked_mesh(BinaryChunkWriter& writer, CookedPlatform platform,
                       const CookedMeshData& mesh);

// Reads a chunk, validates version + type, reads common header + payload,
// verifies integrity hash. Returns false on any failure.
bool read_cooked_mesh(BinaryChunkReader& reader, CookedMeshData& mesh);

void write_cooked_texture(BinaryChunkWriter& writer, CookedPlatform platform,
                          const CookedTextureData& tex);
bool read_cooked_texture(BinaryChunkReader& reader, CookedTextureData& tex);

void write_cooked_shader(BinaryChunkWriter& writer, CookedPlatform platform,
                         const CookedShaderData& shader);
bool read_cooked_shader(BinaryChunkReader& reader, CookedShaderData& shader);

void write_cooked_material(BinaryChunkWriter& writer, CookedPlatform platform,
                           const CookedMaterialData& mat);
bool read_cooked_material(BinaryChunkReader& reader, CookedMaterialData& mat);

} // namespace pino
