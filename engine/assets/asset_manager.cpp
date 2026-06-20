#include "asset_manager.h"
#include "engine/core/log.h"
#include "engine/core/math_utils.h"

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
    // Convert backslashes to forward slashes
    for (auto& ch : p) if (ch == '\\') ch = '/';

    // Collapse "." and ".." segments
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

    // Rejoin
    std::string result;
    for (usize i = 0; i < segments.size(); ++i) {
        if (i > 0) result += '/';
        result += segments[i];
    }

    // Lowercase on Windows (case-insensitive FS)
#if defined(_WIN32)
    for (auto& ch : result) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
#endif

    return result;
}

// ── AssetManager ────────────────────────────────────────────────
AssetManager::AssetManager(FileSystem& fs) : m_fs(fs) {
    init_fallback_assets();
}

AssetManager::~AssetManager() = default;

std::string AssetManager::shader_key(const char* vert_path, const char* frag_path) const {
    return normalize_asset_path(vert_path) + "|" + normalize_asset_path(frag_path);
}

// ── Fallback assets ─────────────────────────────────────────────
void AssetManager::init_fallback_assets() {
    // Fallback texture: 8x8 magenta/black checkerboard
    m_fallback_tex = std::make_unique<Texture>();
    {
        // RGBA bytes stored as u32 (little-endian: 0xAABBGGRR)
        // Magenta = 0xFFFF00FF, Black (opaque) = 0xFF000000
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

    // Fallback mesh: unit cube
    m_fallback_mesh = std::make_unique<Mesh>();
    {
        // Unit cube vertices (pos, normal, uv)
        struct V { glm::vec3 p, n; glm::vec2 uv; };
        std::vector<V> verts;
        std::vector<u32> idx;

        auto emit = [&](const glm::vec3& p, const glm::vec3& n, const glm::vec2& uv) {
            verts.push_back({p, n, uv});
            idx.push_back(static_cast<u32>(idx.size()));
        };

        // Each face: 4 verts, 6 indices (triangulated)
        // +X face
        glm::vec3 px(1,1,1), nx(1,1,1);
        emit({ 1,-1,-1},{ 1,0,0},{1,1}); emit({ 1, 1,-1},{ 1,0,0},{1,0});
        emit({ 1, 1, 1},{ 1,0,0},{0,0}); emit({ 1,-1, 1},{ 1,0,0},{0,1});
        // -X face
        emit({-1,-1, 1},{-1,0,0},{1,1}); emit({-1, 1, 1},{-1,0,0},{1,0});
        emit({-1, 1,-1},{-1,0,0},{0,0}); emit({-1,-1,-1},{-1,0,0},{0,1});
        // +Y face
        emit({-1, 1,-1},{0, 1,0},{1,1}); emit({ 1, 1,-1},{0, 1,0},{1,0});
        emit({ 1, 1, 1},{0, 1,0},{0,0}); emit({-1, 1, 1},{0, 1,0},{0,1});
        // -Y face
        emit({-1,-1, 1},{0,-1,0},{1,1}); emit({ 1,-1, 1},{0,-1,0},{1,0});
        emit({ 1,-1,-1},{0,-1,0},{0,0}); emit({-1,-1,-1},{0,-1,0},{0,1});
        // +Z face
        emit({-1,-1, 1},{0,0, 1},{1,1}); emit({ 1,-1, 1},{0,0, 1},{1,0});
        emit({ 1, 1, 1},{0,0, 1},{0,0}); emit({-1, 1, 1},{0,0, 1},{0,1});
        // -Z face
        emit({ 1,-1,-1},{0,0,-1},{1,1}); emit({-1,-1,-1},{0,0,-1},{1,0});
        emit({-1, 1,-1},{0,0,-1},{0,0}); emit({ 1, 1,-1},{0,0,-1},{0,1});

        // Generate proper index buffer for 6 faces (4 verts each = 24 verts, 36 indices)
        std::vector<u32> indices;
        for (u32 i = 0; i < 24; i += 4) {
            indices.push_back(i);   indices.push_back(i+1); indices.push_back(i+2);
            indices.push_back(i);   indices.push_back(i+2); indices.push_back(i+3);
        }

        m_fallback_mesh->upload(reinterpret_cast<const Vertex*>(verts.data()),
                                static_cast<u32>(verts.size()),
                                indices.data(), static_cast<u32>(indices.size()));
    }

    // Fallback shader: simple vertex-lit color shader
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

// ── Mesh loading — tinyobjloader ────────────────────────────────
Mesh* AssetManager::load_mesh(const char* path) {
    std::string key = normalize_asset_path(path);
    auto it = m_mesh_cache.find(key);
    if (it != m_mesh_cache.end()) return it->second.get();

    std::string src = m_fs.read_text(path);
    if (src.empty()) {
        PINO_ERROR("Failed to read .obj: %s  (using fallback mesh)", path);
        return fallback_mesh();
    }

    tinyobj::ObjReader reader;
    tinyobj::ObjReaderConfig cfg;
    cfg.triangulate = true;
    cfg.vertex_color = false;

    if (!reader.ParseFromString(src, "", cfg)) {
        PINO_ERROR("tinyobj error for %s: %s  (using fallback mesh)", path,
                   reader.Error().c_str());
        return fallback_mesh();
    }
    if (!reader.Warning().empty()) {
        PINO_WARN("tinyobj warning for %s: %s", path, reader.Warning().c_str());
    }

    const auto& attrib = reader.GetAttrib();
    const auto& shapes = reader.GetShapes();

    if (shapes.empty()) {
        PINO_ERROR("No shapes in .obj: %s  (using fallback mesh)", path);
        return fallback_mesh();
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
        PINO_ERROR("No vertices from: %s  (using fallback mesh)", path);
        return fallback_mesh();
    }

    auto mesh = std::make_shared<Mesh>();
    mesh->upload(vertices.data(), static_cast<u32>(vertices.size()),
                 indices.data(),  static_cast<u32>(indices.size()));

    PINO_INFO("Loaded mesh: %s (%u verts, %u indices)", path,
              static_cast<u32>(vertices.size()),
              static_cast<u32>(indices.size()));

    Mesh* ptr = mesh.get();
    m_mesh_cache[key] = std::move(mesh);
    return ptr;
}

AssetHandle<Mesh> AssetManager::get_mesh(const char* path) {
    std::string key = normalize_asset_path(path);
    auto it = m_mesh_cache.find(key);
    if (it != m_mesh_cache.end())
        return AssetHandle<Mesh>(it->second);

    // Load via the standard path; returns raw ptr but we need the shared_ptr
    load_mesh(path);
    it = m_mesh_cache.find(key);
    if (it != m_mesh_cache.end())
        return AssetHandle<Mesh>(it->second);
    return AssetHandle<Mesh>();
}

// ── Texture loading — stb_image ─────────────────────────────────
Texture* AssetManager::load_texture(const char* path) {
    std::string key = normalize_asset_path(path);
    auto it = m_tex_cache.find(key);
    if (it != m_tex_cache.end()) return it->second.get();

    std::vector<u8> file_data = m_fs.read_binary(path);
    if (file_data.empty()) {
        PINO_ERROR("Failed to read texture: %s  (using fallback)", path);
        return fallback_texture();
    }

    i32 w = 0, h = 0, channels = 0;
    stbi_set_flip_vertically_on_load(1);

    unsigned char* pixels = stbi_load_from_memory(
        file_data.data(), static_cast<i32>(file_data.size()),
        &w, &h, &channels, 4);

    if (!pixels) {
        PINO_ERROR("stb_image failed to decode: %s  (using fallback)", path);
        return fallback_texture();
    }

    auto tex = std::make_shared<Texture>();
    tex->upload_rgba(pixels, w, h);
    stbi_image_free(pixels);

    PINO_INFO("Loaded texture: %s (%dx%d)", path, w, h);

    Texture* ptr = tex.get();
    m_tex_cache[key] = std::move(tex);
    return ptr;
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

// ── Shader loading ──────────────────────────────────────────────
Shader* AssetManager::load_shader(const char* vert_path, const char* frag_path) {
    std::string key = shader_key(vert_path, frag_path);
    auto it = m_shader_cache.find(key);
    if (it != m_shader_cache.end()) return it->second.get();

    std::string vert_src = m_fs.read_text(vert_path);
    std::string frag_src = m_fs.read_text(frag_path);
    if (vert_src.empty() || frag_src.empty()) {
        PINO_ERROR("Failed to read shader files: %s / %s  (using fallback)", vert_path, frag_path);
        return fallback_shader();
    }

    auto shader = std::make_shared<Shader>();
    if (!shader->load(vert_src.c_str(), frag_src.c_str())) {
        PINO_ERROR("Failed to compile shader: %s / %s  (using fallback)", vert_path, frag_path);
        return fallback_shader();
    }

    PINO_INFO("Loaded shader: %s + %s", vert_path, frag_path);

    Shader* ptr = shader.get();
    m_shader_cache[key] = std::move(shader);
    return ptr;
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

        // Determine type by extension
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
            // Assume paired .frag file with same name
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
            // use_count == 1 means only the cache holds a reference
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
    // Release cached GPU resources (old GL handles from lost context)
    m_mesh_cache.clear();
    m_tex_cache.clear();
    m_shader_cache.clear();

    // Release and re-create fallback assets
    m_fallback_tex.reset();
    m_fallback_mesh.reset();
    m_fallback_shader.reset();
    init_fallback_assets();

    PINO_INFO("All GPU resources invalidated (context loss)");
}

} // namespace pino
