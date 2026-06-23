#include "asset_manager.h"
#include "engine/core/log.h"
#include "engine/core/math_utils.h"
#include "engine/serialization/cooked_asset.h"
#include "engine/assets/cooked_file_source.h"
#include "engine/assets/raw_file_source.h"
#include "engine/assets/asset_utils.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include <stb_image.h>

#include <vector>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <array>

namespace pino {

// ── AssetManager ────────────────────────────────────────────────
AssetManager::AssetManager(FileSystem& fs) {
    m_raw_source = std::make_unique<RawFileSource>(fs);
    init_fallback_assets();
}

AssetManager::~AssetManager() = default;

std::string AssetManager::shader_key(const char* vert_path, const char* frag_path) const {
    return normalize_asset_path(vert_path) + "|" + normalize_asset_path(frag_path);
}

// ═══════════════════════════════════════════════════════════════════
//  Cooked manifest support
// ═══════════════════════════════════════════════════════════════════

bool AssetManager::load_cooked_manifest(const char* manifest_path, const char* cooked_dir, FileSystem& fs) {
    if (!m_registry.load_from_path(manifest_path)) {
        PINO_ERROR("AssetManager: failed to load cooked manifest: %s", manifest_path);
        return false;
    }

    m_cooked_source = std::make_unique<CookedFileSource>(fs, m_registry, cooked_dir);

    PINO_INFO("AssetManager: cooked manifest loaded (%u entries, dir: %s)",
              m_registry.entry_count(), cooked_dir ? cooked_dir : "");
    return true;
}

// ═══════════════════════════════════════════════════════════════════
//  Cooked blob deserialization
// ═══════════════════════════════════════════════════════════════════

bool AssetManager::load_mesh_from_cooked_blob(const BinaryBlob& blob,
                                               Mesh*& out_mesh, std::shared_ptr<Mesh>& out_shared)
{
    CookedMeshData mesh_data;
    BinaryChunkReader reader(blob.data.data(), static_cast<u32>(blob.data.size()));
    if (!read_cooked_mesh(reader, mesh_data)) {
        PINO_ERROR("Failed to deserialize cooked mesh: %s", blob.debug_path.c_str());
        return false;
    }

    if (mesh_data.vertex_count == 0 || mesh_data.vertex_data.empty()) {
        PINO_ERROR("Cooked mesh has no vertices: %s", blob.debug_path.c_str());
        return false;
    }

    auto mesh = std::make_shared<Mesh>();
    const Vertex* verts = reinterpret_cast<const Vertex*>(mesh_data.vertex_data.data());
    const u32* idx = mesh_data.indices.data();
    mesh->upload(verts, mesh_data.vertex_count, idx, mesh_data.index_count);

    // Upload tangent/bitangent if present
    if (!mesh_data.tangent_data.empty() && !mesh_data.bitangent_data.empty()) {
        const glm::vec3* tangents = reinterpret_cast<const glm::vec3*>(mesh_data.tangent_data.data());
        const glm::vec3* bitangents = reinterpret_cast<const glm::vec3*>(mesh_data.bitangent_data.data());
        mesh->upload_tangents(tangents, bitangents, mesh_data.vertex_count);
    }

    PINO_INFO("Loaded cooked mesh (%u verts, %u indices%s)",
              mesh_data.vertex_count, mesh_data.index_count,
              mesh_data.tangent_data.empty() ? "" : ", with tangents");

    out_mesh = mesh.get();
    out_shared = std::move(mesh);
    return true;
}

bool AssetManager::load_texture_from_cooked_blob(const BinaryBlob& blob,
                                                  Texture*& out_tex, std::shared_ptr<Texture>& out_shared)
{
    CookedTextureData tex_data;
    BinaryChunkReader reader(blob.data.data(), static_cast<u32>(blob.data.size()));
    if (!read_cooked_texture(reader, tex_data)) {
        PINO_ERROR("Failed to deserialize cooked texture: %s", blob.debug_path.c_str());
        return false;
    }

    if (tex_data.width == 0 || tex_data.height == 0 || tex_data.mip_data.empty()) {
        PINO_ERROR("Cooked texture has no pixels: %s", blob.debug_path.c_str());
        return false;
    }

    auto tex = std::make_shared<Texture>();
    tex->upload_rgba(tex_data.mip_data.data(),
                     static_cast<i32>(tex_data.width),
                     static_cast<i32>(tex_data.height));

    PINO_INFO("Loaded cooked texture (%dx%d)", tex_data.width, tex_data.height);

    out_tex = tex.get();
    out_shared = std::move(tex);
    return true;
}

bool AssetManager::load_shader_from_cooked_blob(const BinaryBlob& blob,
                                                 Shader*& out_shader, std::shared_ptr<Shader>& out_shared)
{
    CookedShaderData shader_data;
    BinaryChunkReader reader(blob.data.data(), static_cast<u32>(blob.data.size()));
    if (!read_cooked_shader(reader, shader_data)) {
        PINO_ERROR("Failed to deserialize cooked shader: %s", blob.debug_path.c_str());
        return false;
    }

    if (shader_data.vert_stage.empty() || shader_data.frag_stage.empty()) {
        PINO_ERROR("Cooked shader has no source: %s", blob.debug_path.c_str());
        return false;
    }

    auto vec_to_str = [](const std::vector<u8>& vec) -> std::string {
        if (vec.empty()) return {};
        if (vec.back() == '\0') return reinterpret_cast<const char*>(vec.data());
        return std::string(reinterpret_cast<const char*>(vec.data()), vec.size());
    };

    std::string vert_src = vec_to_str(shader_data.vert_stage);
    std::string frag_src = vec_to_str(shader_data.frag_stage);

    auto shader = std::make_shared<Shader>();
    if (!shader->load(vert_src.c_str(), frag_src.c_str())) {
        PINO_ERROR("Failed to compile cooked shader: %s", blob.debug_path.c_str());
        return false;
    }

    out_shader = shader.get();
    out_shared = std::move(shader);
    return true;
}

// ═══════════════════════════════════════════════════════════════════
//  Raw blob parsing
// ═══════════════════════════════════════════════════════════════════

bool AssetManager::load_mesh_from_raw_blob(const BinaryBlob& blob,
                                            Mesh*& out_mesh, std::shared_ptr<Mesh>& out_shared)
{
    std::string src(reinterpret_cast<const char*>(blob.data.data()), blob.data.size());

    tinyobj::ObjReader reader;
    tinyobj::ObjReaderConfig cfg;
    cfg.triangulate = true;
    cfg.vertex_color = false;

    if (!reader.ParseFromString(src, "", cfg)) {
        PINO_ERROR("tinyobj error for %s: %s", blob.debug_path.c_str(), reader.Error().c_str());
        return false;
    }
    if (!reader.Warning().empty()) {
        PINO_WARN("tinyobj warning for %s: %s", blob.debug_path.c_str(), reader.Warning().c_str());
    }

    const auto& attrib = reader.GetAttrib();
    const auto& shapes = reader.GetShapes();

    if (shapes.empty()) {
        PINO_ERROR("No shapes in .obj: %s", blob.debug_path.c_str());
        return false;
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
        PINO_ERROR("No vertices from: %s", blob.debug_path.c_str());
        return false;
    }

    auto mesh = std::make_shared<Mesh>();
    mesh->upload(vertices.data(), static_cast<u32>(vertices.size()),
                 indices.data(),  static_cast<u32>(indices.size()));

    PINO_INFO("Loaded mesh from raw: %s (%u verts, %u indices)",
              blob.debug_path.c_str(),
              static_cast<u32>(vertices.size()),
              static_cast<u32>(indices.size()));

    out_mesh = mesh.get();
    out_shared = std::move(mesh);
    return true;
}

bool AssetManager::load_texture_from_raw_blob(const BinaryBlob& blob,
                                               Texture*& out_tex, std::shared_ptr<Texture>& out_shared)
{
    i32 w = 0, h = 0, channels = 0;
    stbi_set_flip_vertically_on_load(1);

    unsigned char* pixels = stbi_load_from_memory(
        blob.data.data(), static_cast<i32>(blob.data.size()),
        &w, &h, &channels, 4);

    if (!pixels) {
        PINO_ERROR("stb_image failed to decode: %s", blob.debug_path.c_str());
        return false;
    }

    auto tex = std::make_shared<Texture>();
    tex->upload_rgba(pixels, w, h);
    stbi_image_free(pixels);

    PINO_INFO("Loaded texture from raw: %s (%dx%d)", blob.debug_path.c_str(), w, h);

    out_tex = tex.get();
    out_shared = std::move(tex);
    return true;
}

bool AssetManager::load_shader_from_raw_blobs(const BinaryBlob& vert_blob,
                                               const BinaryBlob& frag_blob,
                                               Shader*& out_shader, std::shared_ptr<Shader>& out_shared)
{
    std::string vert_src(reinterpret_cast<const char*>(vert_blob.data.data()), vert_blob.data.size());
    std::string frag_src(reinterpret_cast<const char*>(frag_blob.data.data()), frag_blob.data.size());

    if (vert_src.empty() || frag_src.empty()) {
        PINO_ERROR("Failed to read shader files: %s / %s",
                   vert_blob.debug_path.c_str(), frag_blob.debug_path.c_str());
        return false;
    }

    auto shader = std::make_shared<Shader>();
    if (!shader->load(vert_src.c_str(), frag_src.c_str())) {
        PINO_ERROR("Failed to compile shader: %s / %s",
                   vert_blob.debug_path.c_str(), frag_blob.debug_path.c_str());
        return false;
    }

    PINO_INFO("Loaded shader from raw: %s + %s",
              vert_blob.debug_path.c_str(), frag_blob.debug_path.c_str());

    out_shader = shader.get();
    out_shared = std::move(shader);
    return true;
}

// ── Fallback assets ─────────────────────────────────────────────
void AssetManager::init_fallback_assets() {
    m_fallback_tex = std::make_unique<Texture>();
    {
        static const u32 fb_tex_data[8 * 8] = {
            0xFFFF00FF,0xFF000000,0xFFFF00FF,0xFF000000,0xFFFF00FF,0xFF000000,0xFFFF00FF,0xFF000000,
            0xFF000000,0xFFFF00FF,0xFF000000,0xFFFF00FF,0xFF000000,0xFFFF00FF,0xFF000000,0xFFFF00FF,
            0xFFFF00FF,0xFF000000,0xFFFF00FF,0xFF000000,0xFFFF00FF,0xFF000000,0xFFFF00FF,0xFF000000,
            0xFF000000,0xFFFF00FF,0xFF000000,0xFFFF00FF,0xFF000000,0xFFFF00FF,0xFF000000,0xFFFF00FF,
            0xFFFF00FF,0xFF000000,0xFFFF00FF,0xFF000000,0xFFFF00FF,0xFF000000,0xFFFF00FF,0xFF000000,
            0xFF000000,0xFFFF00FF,0xFF000000,0xFFFF00FF,0xFF000000,0xFFFF00FF,0xFF000000,0xFFFF00FF,
            0xFFFF00FF,0xFF000000,0xFFFF00FF,0xFF000000,0xFFFF00FF,0xFF000000,0xFFFF00FF,0xFF000000,
            0xFF000000,0xFFFF00FF,0xFF000000,0xFFFF00FF,0xFF000000,0xFFFF00FF,0xFF000000,0xFFFF00FF,
        };
        m_fallback_tex->upload_rgba(reinterpret_cast<const u8*>(fb_tex_data), 8, 8);
    }

    m_fallback_mesh = std::make_unique<Mesh>();
    {
        struct V { glm::vec3 p, n; glm::vec2 uv; };
        std::vector<V> verts;
        std::vector<u32> idx;

        auto emit = [&](const glm::vec3& p, const glm::vec3& n, const glm::vec2& uv) {
            verts.push_back({p, n, uv});
            idx.push_back(static_cast<u32>(idx.size()));
        };

        emit({ 1,-1,-1},{ 1,0,0},{1,1}); emit({ 1, 1,-1},{ 1,0,0},{1,0});
        emit({ 1, 1, 1},{ 1,0,0},{0,0}); emit({ 1,-1, 1},{ 1,0,0},{0,1});
        emit({-1,-1, 1},{-1,0,0},{1,1}); emit({-1, 1, 1},{-1,0,0},{1,0});
        emit({-1, 1,-1},{-1,0,0},{0,0}); emit({-1,-1,-1},{-1,0,0},{0,1});
        emit({-1, 1,-1},{0, 1,0},{1,1}); emit({ 1, 1,-1},{0, 1,0},{1,0});
        emit({ 1, 1, 1},{0, 1,0},{0,0}); emit({-1, 1, 1},{0, 1,0},{0,1});
        emit({-1,-1, 1},{0,-1,0},{1,1}); emit({ 1,-1, 1},{0,-1,0},{1,0});
        emit({ 1,-1,-1},{0,-1,0},{0,0}); emit({-1,-1,-1},{0,-1,0},{0,1});
        emit({-1,-1, 1},{0,0, 1},{1,1}); emit({ 1,-1, 1},{0,0, 1},{1,0});
        emit({ 1, 1, 1},{0,0, 1},{0,0}); emit({-1, 1, 1},{0,0, 1},{0,1});
        emit({ 1,-1,-1},{0,0,-1},{1,1}); emit({-1,-1,-1},{0,0,-1},{1,0});
        emit({-1, 1,-1},{0,0,-1},{0,0}); emit({ 1, 1,-1},{0,0,-1},{0,1});

        std::vector<u32> indices;
        for (u32 i = 0; i < 24; i += 4) {
            indices.push_back(i);   indices.push_back(i+1); indices.push_back(i+2);
            indices.push_back(i);   indices.push_back(i+2); indices.push_back(i+3);
        }

        m_fallback_mesh->upload(reinterpret_cast<const Vertex*>(verts.data()),
                                static_cast<u32>(verts.size()),
                                indices.data(), static_cast<u32>(indices.size()));
    }

    m_fallback_shader = std::make_unique<Shader>();
    {
        static const char* vert = R"(
#version 300 es
layout(location=0) in vec3 a_pos;
uniform mat4 u_mvp;
void main(){ gl_Position=u_mvp*vec4(a_pos,1.0); })";
        static const char* frag = R"(
#version 300 es
precision mediump float;
uniform vec4 u_color;
out vec4 fc;
void main(){ fc=u_color; })";
        if (!m_fallback_shader->load(vert, frag)) {
            PINO_ERROR("Fallback shader failed to compile");
        }
    }

    PINO_INFO("Fallback assets initialized");
}

// ── Mesh loading pipeline ────────────────────────────────────────
// 1) Resolve asset key via registry (cooked)  2) Query source
// 3) Load binary blob   4) Validate hash/version
// 5) Deserialize        6) Upload to GPU       7) Cache result
// ─────────────────────────────────────────────────────────────────
Mesh* AssetManager::load_mesh(const char* path) {
    std::string key = normalize_asset_path(path);
    auto it = m_mesh_cache.find(key);
    if (it != m_mesh_cache.end()) return it->second.get();

    // ── 1. Resolve asset key via registry (if cooked mode) ────────
    std::string resolved;
    bool have_cooked = false;
    if (m_cooked_source) {
        resolved = m_registry.resolve(key.c_str());
        if (resolved.empty())
            PINO_WARN("Cooked mesh \"%s\" not in manifest, falling back to raw", path);
        else
            have_cooked = true;
    }

    // ── 2–3. Query active source → load blob ─────────────────────
    BinaryBlob blob;
    bool is_cooked = false;
    if (have_cooked) {
        blob = m_cooked_source->load(resolved.c_str());
        if (blob.data.empty()) {
            PINO_WARN("Cooked mesh \"%s\" file missing, falling back to raw", resolved.c_str());
        } else {
            is_cooked = true;
        }
    }
    if (!is_cooked)
        blob = m_raw_source->load(key.c_str());

    // ── 4. Validate hash/version ─────────────────────────────────
    if (is_cooked && !blob.data.empty()) {
        if (!m_registry.verify_integrity(resolved.c_str(),
                                         blob.data.data(),
                                         static_cast<u32>(blob.data.size()))) {
            PINO_WARN("Cooked mesh \"%s\" hash mismatch, falling back to raw", path);
            blob = m_raw_source->load(key.c_str());
            is_cooked = false;
        }
    }

    // ── 5–6. Deserialize → upload to GPU ─────────────────────────
    Mesh* mesh_ptr = nullptr;
    std::shared_ptr<Mesh> mesh_shared;
    bool ok = false;
    if (is_cooked) {
        ok = load_mesh_from_cooked_blob(blob, mesh_ptr, mesh_shared);
    } else if (!blob.data.empty()) {
        ok = load_mesh_from_raw_blob(blob, mesh_ptr, mesh_shared);
    }

    // ── 7. Cache result ───────────────────────────────────────────
    if (ok) {
        m_mesh_cache[key] = std::move(mesh_shared);
        return mesh_ptr;
    }

    PINO_ERROR("Failed to load mesh: %s  (using fallback mesh)", path);
    return fallback_mesh();
}

AssetHandle<Mesh> AssetManager::get_mesh(const char* path) {
    std::string key = normalize_asset_path(path);
    auto it = m_mesh_cache.find(key);
    if (it != m_mesh_cache.end())
        return AssetHandle<Mesh>(it->second);

    load_mesh(path);
    it = m_mesh_cache.find(key);
    if (it != m_mesh_cache.end())
        return AssetHandle<Mesh>(it->second);
    return AssetHandle<Mesh>();
}

// ── Texture loading pipeline ─────────────────────────────────────
Texture* AssetManager::load_texture(const char* path) {
    std::string key = normalize_asset_path(path);
    auto it = m_tex_cache.find(key);
    if (it != m_tex_cache.end()) return it->second.get();

    // 1. Resolve asset key via registry (if cooked)
    std::string resolved;
    bool have_cooked = false;
    if (m_cooked_source) {
        resolved = m_registry.resolve(key.c_str());
        if (resolved.empty())
            PINO_WARN("Cooked texture \"%s\" not in manifest, falling back to raw", path);
        else
            have_cooked = true;
    }

    // 2–3. Query active source → load blob
    BinaryBlob blob;
    bool is_cooked = false;
    if (have_cooked) {
        blob = m_cooked_source->load(resolved.c_str());
        if (blob.data.empty()) {
            PINO_WARN("Cooked texture \"%s\" file missing, falling back to raw", resolved.c_str());
        } else {
            is_cooked = true;
        }
    }
    if (!is_cooked)
        blob = m_raw_source->load(key.c_str());

    // 4. Validate hash
    if (is_cooked && !blob.data.empty()) {
        if (!m_registry.verify_integrity(resolved.c_str(),
                                         blob.data.data(),
                                         static_cast<u32>(blob.data.size()))) {
            PINO_WARN("Cooked texture \"%s\" hash mismatch, falling back to raw", path);
            blob = m_raw_source->load(key.c_str());
            is_cooked = false;
        }
    }

    // 5–6. Deserialize → upload
    Texture* tex_ptr = nullptr;
    std::shared_ptr<Texture> tex_shared;
    bool ok = false;
    if (is_cooked) {
        ok = load_texture_from_cooked_blob(blob, tex_ptr, tex_shared);
    } else if (!blob.data.empty()) {
        ok = load_texture_from_raw_blob(blob, tex_ptr, tex_shared);
    }

    // 7. Cache
    if (ok) {
        m_tex_cache[key] = std::move(tex_shared);
        return tex_ptr;
    }

    PINO_ERROR("Failed to load texture: %s  (using fallback)", path);
    return fallback_texture();
}

AssetHandle<Texture> AssetManager::get_texture(const char* path) {
    std::string key = normalize_asset_path(path);
    auto it = m_tex_cache.find(key);
    if (it != m_tex_cache.end())
        return AssetHandle<Texture>(it->second);
    load_texture(path);
    it = m_tex_cache.find(key);
    if (it != m_tex_cache.end())
        return AssetHandle<Texture>(it->second);
    return AssetHandle<Texture>();
}

// ── Shader loading pipeline ──────────────────────────────────────
Shader* AssetManager::load_shader(const char* vert_path, const char* frag_path) {
    std::string key = shader_key(vert_path, frag_path);
    auto it = m_shader_cache.find(key);
    if (it != m_shader_cache.end()) return it->second.get();

    std::string vert_norm = normalize_asset_path(vert_path);

    // 1. Resolve asset key via registry (if cooked)
    std::string resolved;
    bool have_cooked = false;
    if (m_cooked_source) {
        resolved = m_registry.resolve(vert_norm.c_str());
        if (resolved.empty())
            PINO_WARN("Cooked shader \"%s\" not in manifest, falling back to raw", vert_path);
        else
            have_cooked = true;
    }

    // 2–3. Query active source → load blob
    BinaryBlob blob;
    bool is_cooked = false;
    if (have_cooked) {
        blob = m_cooked_source->load(resolved.c_str());
        if (blob.data.empty()) {
            PINO_WARN("Cooked shader \"%s\" file missing, falling back to raw", resolved.c_str());
        } else {
            is_cooked = true;
        }
    }

    // 4. Validate hash (cooked)
    if (is_cooked && !blob.data.empty()) {
        if (!m_registry.verify_integrity(resolved.c_str(),
                                         blob.data.data(),
                                         static_cast<u32>(blob.data.size()))) {
            PINO_WARN("Cooked shader \"%s\" hash mismatch, falling back to raw", vert_path);
            is_cooked = false;
            blob.data.clear();
        }
    }

    // 5–6. Deserialize → upload
    Shader* shader_ptr = nullptr;
    std::shared_ptr<Shader> shader_shared;
    bool ok = false;
    if (is_cooked) {
        ok = load_shader_from_cooked_blob(blob, shader_ptr, shader_shared);
    } else {
        // Raw shader loading (needs both vert and frag files)
        std::string frag_norm = normalize_asset_path(frag_path);
        BinaryBlob vert_blob = m_raw_source->load(vert_norm.c_str());
        BinaryBlob frag_blob = m_raw_source->load(frag_norm.c_str());
        if (!vert_blob.data.empty() && !frag_blob.data.empty())
            ok = load_shader_from_raw_blobs(vert_blob, frag_blob, shader_ptr, shader_shared);
    }

    // 7. Cache
    if (ok) {
        m_shader_cache[key] = std::move(shader_shared);
        return shader_ptr;
    }

    PINO_ERROR("Failed to read shader files: %s / %s  (using fallback)", vert_path, frag_path);
    return fallback_shader();
}

AssetHandle<Shader> AssetManager::get_shader(const char* vert_path, const char* frag_path) {
    std::string key = shader_key(vert_path, frag_path);
    auto it = m_shader_cache.find(key);
    if (it != m_shader_cache.end())
        return AssetHandle<Shader>(it->second);
    load_shader(vert_path, frag_path);
    it = m_shader_cache.find(key);
    if (it != m_shader_cache.end())
        return AssetHandle<Shader>(it->second);
    return AssetHandle<Shader>();
}

// ── Unload unused ───────────────────────────────────────────────
void AssetManager::unload_unused() {
    u32 freed = 0;

    auto purge = [&](auto& cache) {
        for (auto it = cache.begin(); it != cache.end(); ) {
            if (it->second.use_count() == 1) {
                it = cache.erase(it);
                ++freed;
            } else {
                ++it;
            }
        }
    };

    purge(m_mesh_cache);
    purge(m_tex_cache);
    purge(m_shader_cache);

    if (freed > 0)
        PINO_INFO("Unload unused: %u assets freed", freed);
}

// ── Clear all ───────────────────────────────────────────────────
void AssetManager::clear() {
    m_mesh_cache.clear();
    m_tex_cache.clear();
    m_shader_cache.clear();
    PINO_INFO("Asset manager cache cleared");
}

// ── Invalidate all GPU resources (for context loss) ────────────
void AssetManager::invalidate_all() {
    m_mesh_cache.clear();
    m_tex_cache.clear();
    m_shader_cache.clear();

    m_fallback_tex.reset();
    m_fallback_mesh.reset();
    m_fallback_shader.reset();
    init_fallback_assets();

    PINO_INFO("All GPU resources invalidated (context loss)");
}

// ═══════════════════════════════════════════════════════════════════
//  Debug API
// ═══════════════════════════════════════════════════════════════════

void AssetManager::print_loaded_asset_source(const char* path) const {
    std::string key = normalize_asset_path(path);

    // Check caches
    if (m_mesh_cache.count(key))
        { PINO_INFO("[debug] mesh '%s' → CACHED", key.c_str()); return; }
    if (m_tex_cache.count(key))
        { PINO_INFO("[debug] texture '%s' → CACHED", key.c_str()); return; }

    // Not cached — check source
    if (m_cooked_source) {
        std::string resolved = m_registry.resolve(key.c_str());
        if (!resolved.empty()) {
            if (m_cooked_source->exists(resolved.c_str()))
                { PINO_INFO("[debug] '%s' → not cached, source: COOKED", key.c_str()); return; }
            else
                { PINO_INFO("[debug] '%s' → not cached, source: RAW (cooked file missing)", key.c_str()); return; }
        }
    }
    // Fallthrough: raw source
    PINO_INFO("[debug] '%s' → not cached, source: RAW", key.c_str());
}

void AssetManager::print_loaded_asset_source(const char* vert_path, const char* frag_path) const {
    std::string key = shader_key(vert_path, frag_path);

    if (m_shader_cache.count(key))
        { PINO_INFO("[debug] shader '%s|%s' → CACHED", normalize_asset_path(vert_path).c_str(), normalize_asset_path(frag_path).c_str()); return; }

    std::string vert_norm = normalize_asset_path(vert_path);
    if (m_cooked_source) {
        std::string resolved = m_registry.resolve(vert_norm.c_str());
        if (!resolved.empty()) {
            if (m_cooked_source->exists(resolved.c_str()))
                { PINO_INFO("[debug] shader '%s|%s' → not cached, source: COOKED",
                            vert_norm.c_str(), normalize_asset_path(frag_path).c_str()); return; }
        }
    }
    PINO_INFO("[debug] shader '%s|%s' → not cached, source: RAW",
              vert_norm.c_str(), normalize_asset_path(frag_path).c_str());
}

void AssetManager::dump_asset_resolution_chain(const char* path) const {
    std::string key = normalize_asset_path(path);
    PINO_INFO("── Resolution chain: '%s' ──", path);
    PINO_INFO("  1. Normalize → '%s'", key.c_str());

    // Step 2: cache check
    bool in_mesh_cache = m_mesh_cache.count(key) > 0;
    bool in_tex_cache  = m_tex_cache.count(key) > 0;
    if (in_mesh_cache)
        { PINO_INFO("  2. Cache → HIT (mesh cache)"); return; }
    if (in_tex_cache)
        { PINO_INFO("  2. Cache → HIT (texture cache)"); return; }
    PINO_INFO("  2. Cache → MISS");

    // Step 3: manifest resolve
    std::string resolved;
    bool in_manifest = false;
    if (m_cooked_source) {
        resolved = m_registry.resolve(key.c_str());
        in_manifest = !resolved.empty();
        if (in_manifest)
            PINO_INFO("  3. Manifest → FOUND (canonical key: '%s')", resolved.c_str());
        else
            PINO_INFO("  3. Manifest → NOT FOUND");
    } else {
        PINO_INFO("  3. Manifest → no manifest loaded (cooked mode inactive)");
    }

    // Step 4: source query
    if (in_manifest) {
        bool cooked_exists = m_cooked_source->exists(resolved.c_str());
        if (cooked_exists)
            PINO_INFO("  4. Cooked source → EXISTS ('%s')", resolved.c_str());
        else
            PINO_INFO("  4. Cooked source → MISSING ('%s')", resolved.c_str());

        PINO_INFO("  5. Raw source → EXISTS ('%s') (will fall back if cooked fails)", key.c_str());
    } else {
        bool raw_exists = m_raw_source->exists(key.c_str());
        if (raw_exists)
            PINO_INFO("  4. Raw source → EXISTS ('%s')", key.c_str());
        else
            PINO_INFO("  4. Raw source → MISSING ('%s')", key.c_str());
    }
    PINO_INFO("── End resolution chain ──");
}

void AssetManager::dump_asset_resolution_chain(const char* vert_path, const char* frag_path) const {
    std::string vert_norm = normalize_asset_path(vert_path);
    std::string frag_norm = normalize_asset_path(frag_path);
    std::string key = shader_key(vert_path, frag_path);
    PINO_INFO("── Resolution chain: shader '%s | %s' ──", vert_norm.c_str(), frag_norm.c_str());
    PINO_INFO("  1. Normalize → '%s', '%s'", vert_norm.c_str(), frag_norm.c_str());

    if (m_shader_cache.count(key))
        { PINO_INFO("  2. Cache → HIT (shader cache)"); return; }
    PINO_INFO("  2. Cache → MISS");

    if (m_cooked_source) {
        std::string resolved = m_registry.resolve(vert_norm.c_str());
        bool in_manifest = !resolved.empty();
        if (in_manifest) {
            PINO_INFO("  3. Manifest → FOUND (canonical key: '%s')", resolved.c_str());
            bool cooked_exists = m_cooked_source->exists(resolved.c_str());
            if (cooked_exists)
                PINO_INFO("  4. Cooked source → EXISTS ('%s')", resolved.c_str());
            else
                PINO_INFO("  4. Cooked source → MISSING ('%s')", resolved.c_str());
            PINO_INFO("  5. Resolution → will try COOKED, fall back to RAW");
        } else {
            PINO_INFO("  3. Manifest → NOT FOUND");
            PINO_INFO("  4. Resolution → will use RAW (vert='%s', frag='%s')",
                      vert_norm.c_str(), frag_norm.c_str());
        }
    } else {
        PINO_INFO("  3. Manifest → no manifest loaded");
        PINO_INFO("  4. Resolution → will use RAW (vert='%s', frag='%s')",
                  vert_norm.c_str(), frag_norm.c_str());
    }
    PINO_INFO("── End resolution chain ──");
}

} // namespace pino
