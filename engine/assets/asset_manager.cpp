#include "asset_manager.h"
#include "engine/core/log.h"
#include "engine/core/math_utils.h"
#include "engine/serialization/cooked_asset.h"
#include "engine/assets/cooked_file_source.h"
#include "engine/assets/raw_file_source.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include <stb_image.h>

#include <vector>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <array>

namespace pino {

// ── Path normalization ──────────────────────────────────────────
std::string normalize_asset_path(const char* path) {
    if (!path || !path[0]) return {};

    std::string p = path;
    for (auto& ch : p) if (ch == '\\') ch = '/';

    std::vector<std::string> segments;
    std::istringstream ss(p);
    std::string seg;
    while (std::getline(ss, seg, '/')) {
        if (seg.empty() || seg == ".") continue;
        if (seg == ".." && !segments.empty()) {
            segments.pop_back();
        } else if (seg != "..") {
            segments.push_back(seg);
        }
    }

    std::string result;
    for (usize i = 0; i < segments.size(); ++i) {
        if (i > 0) result += '/';
        result += segments[i];
    }

#if defined(_WIN32)
    for (auto& ch : result) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
#endif

    return result;
}

// ── AssetManager ────────────────────────────────────────────────
AssetManager::AssetManager(FileSystem& fs) : m_fs(fs) {
    m_raw_source = std::make_unique<RawFileSource>(fs);
    init_fallback_assets();
}

AssetManager::~AssetManager() = default;

std::string AssetManager::shader_key(const char* vert_path, const char* frag_path) const {
    return normalize_asset_path(vert_path) + "|" + normalize_asset_path(frag_path);
}

std::string AssetManager::strip_extension(const std::string& path) {
    auto dot = path.rfind('.');
    if (dot == std::string::npos) return path;
    return path.substr(0, dot);
}

// ═══════════════════════════════════════════════════════════════════
//  Cooked manifest support
// ═══════════════════════════════════════════════════════════════════

bool AssetManager::load_cooked_manifest(const char* manifest_path, const char* cooked_dir) {
    if (!m_registry.load_from_path(manifest_path)) {
        PINO_ERROR("AssetManager: failed to load cooked manifest: %s", manifest_path);
        return false;
    }

    m_cooked_source = std::make_unique<CookedFileSource>(m_fs, m_registry, cooked_dir);

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

    PINO_INFO("Loaded cooked mesh (%u verts, %u indices)",
              mesh_data.vertex_count, mesh_data.index_count);

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

// ── Mesh loading (cooked first, then raw OBJ) ─────────────────
Mesh* AssetManager::load_mesh(const char* path) {
    std::string key = normalize_asset_path(path);
    auto it = m_mesh_cache.find(key);
    if (it != m_mesh_cache.end()) return it->second.get();

    // Try cooked source first
    if (m_cooked_source) {
        std::string asset_key = strip_extension(key);
        BinaryBlob blob = m_cooked_source->load(asset_key.c_str());
        if (!blob.data.empty()) {
            Mesh* mesh_ptr = nullptr;
            std::shared_ptr<Mesh> mesh_shared;
            if (load_mesh_from_cooked_blob(blob, mesh_ptr, mesh_shared)) {
                m_mesh_cache[key] = std::move(mesh_shared);
                return mesh_ptr;
            }
        }
        PINO_INFO("Cooked mesh not found for %s, falling back to raw", path);
    }

    // Raw OBJ loading
    BinaryBlob blob = m_raw_source->load(key.c_str());
    if (!blob.data.empty()) {
        Mesh* mesh_ptr = nullptr;
        std::shared_ptr<Mesh> mesh_shared;
        if (load_mesh_from_raw_blob(blob, mesh_ptr, mesh_shared)) {
            m_mesh_cache[key] = std::move(mesh_shared);
            return mesh_ptr;
        }
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

// ── Texture loading (cooked first, then raw) ───────────────────
Texture* AssetManager::load_texture(const char* path) {
    std::string key = normalize_asset_path(path);
    auto it = m_tex_cache.find(key);
    if (it != m_tex_cache.end()) return it->second.get();

    // Try cooked source first
    if (m_cooked_source) {
        std::string asset_key = strip_extension(key);
        BinaryBlob blob = m_cooked_source->load(asset_key.c_str());
        if (!blob.data.empty()) {
            Texture* tex_ptr = nullptr;
            std::shared_ptr<Texture> tex_shared;
            if (load_texture_from_cooked_blob(blob, tex_ptr, tex_shared)) {
                m_tex_cache[key] = std::move(tex_shared);
                return tex_ptr;
            }
        }
        PINO_INFO("Cooked texture not found for %s, falling back to raw", path);
    }

    // Raw texture loading
    BinaryBlob blob = m_raw_source->load(key.c_str());
    if (!blob.data.empty()) {
        Texture* tex_ptr = nullptr;
        std::shared_ptr<Texture> tex_shared;
        if (load_texture_from_raw_blob(blob, tex_ptr, tex_shared)) {
            m_tex_cache[key] = std::move(tex_shared);
            return tex_ptr;
        }
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

// ── Shader loading (cooked first, then raw) ─────────────────────
Shader* AssetManager::load_shader(const char* vert_path, const char* frag_path) {
    std::string key = shader_key(vert_path, frag_path);
    auto it = m_shader_cache.find(key);
    if (it != m_shader_cache.end()) return it->second.get();

    // Try cooked source first
    if (m_cooked_source) {
        std::string vert_norm = normalize_asset_path(vert_path);
        std::string asset_key = strip_extension(vert_norm);
        BinaryBlob blob = m_cooked_source->load(asset_key.c_str());
        if (!blob.data.empty()) {
            Shader* shader_ptr = nullptr;
            std::shared_ptr<Shader> shader_shared;
            if (load_shader_from_cooked_blob(blob, shader_ptr, shader_shared)) {
                m_shader_cache[key] = std::move(shader_shared);
                return shader_ptr;
            }
        }
        PINO_INFO("Cooked shader not found for %s, falling back to raw", asset_key.c_str());
    }

    // Raw shader loading
    std::string vert_norm = normalize_asset_path(vert_path);
    std::string frag_norm = normalize_asset_path(frag_path);
    BinaryBlob vert_blob = m_raw_source->load(vert_norm.c_str());
    BinaryBlob frag_blob = m_raw_source->load(frag_norm.c_str());
    if (!vert_blob.data.empty() && !frag_blob.data.empty()) {
        Shader* shader_ptr = nullptr;
        std::shared_ptr<Shader> shader_shared;
        if (load_shader_from_raw_blobs(vert_blob, frag_blob, shader_ptr, shader_shared)) {
            m_shader_cache[key] = std::move(shader_shared);
            return shader_ptr;
        }
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

// ── Preloading ──────────────────────────────────────────────────
void AssetManager::preload(const std::vector<std::string>& paths,
                           AssetProgressCallback progress) {
    u32 total = static_cast<u32>(paths.size());
    for (u32 i = 0; i < total; ++i) {
        const std::string& p = paths[i];

        auto dot = p.rfind('.');
        std::string ext;
        if (dot != std::string::npos) {
            ext = p.substr(dot);
            for (auto& ch : ext) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }

        if (ext == ".obj") {
            load_mesh(p.c_str());
        } else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
                   ext == ".bmp" || ext == ".tga") {
            load_texture(p.c_str());
        } else if (ext == ".vert") {
            std::string frag = p.substr(0, p.size() - 4) + "frag";
            load_shader(p.c_str(), frag.c_str());
        } else {
            PINO_WARN("preload: unknown asset type: %s", p.c_str());
        }

        if (progress)
            progress(i + 1, total, p.c_str());
    }
    PINO_INFO("Preload complete: %u/%u assets", total, total);
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

} // namespace pino
