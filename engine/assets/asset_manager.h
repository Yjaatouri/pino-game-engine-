#pragma once

#include "engine/core/types.h"
#include "engine/platform/file_system.h"
#include "engine/renderer/shader.h"
#include "engine/renderer/mesh.h"
#include "engine/renderer/texture.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <functional>

namespace pino {

// ── AssetHandle<T> — safe wrapper around shared asset pointers ──
template <typename T>
class AssetHandle {
public:
    AssetHandle() = default;
    explicit AssetHandle(std::shared_ptr<T> ptr) : m_ptr(std::move(ptr)) {}

    bool is_loaded() const { return m_ptr != nullptr; }
    T*   get()       const { return m_ptr.get(); }
    T*   operator->() const { return m_ptr.get(); }
    T&   operator*()  const { return *m_ptr; }
    explicit operator bool() const { return is_loaded(); }

    // Fallback: returns a default-constructed value if not loaded
    const T& value_or(const T& fallback) const {
        return m_ptr ? *m_ptr : fallback;
    }

private:
    std::shared_ptr<T> m_ptr;
};

// ── Progress callback type ──────────────────────────────────────
using AssetProgressCallback = std::function<void(u32 loaded, u32 total, const char* current)>;

// ── Path normalization ──────────────────────────────────────────
// Normalizes a file path for use as a consistent cache key:
//   - converts backslashes to forward slashes
//   - collapses ".", ".." segments
//   - lowercases on case-insensitive platforms (Win32)
std::string normalize_asset_path(const char* path);

// ── AssetManager ─────────────────────────────────────────────────
class AssetManager {
public:
    explicit AssetManager(FileSystem& fs);
    ~AssetManager();

    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;

    // ---- Mesh loading (via tinyobjloader) ----
    AssetHandle<Mesh> get_mesh(const char* path);

    // ---- Texture loading (via stb_image) ----
    AssetHandle<Texture> get_texture(const char* path);

    // ---- Shader loading ----
    AssetHandle<Shader> get_shader(const char* vert_path, const char* frag_path);

    // ---- Fallback assets (always valid) ----
    Texture* fallback_texture() { return m_fallback_tex.get(); }
    Mesh*    fallback_mesh()    { return m_fallback_mesh.get(); }
    Shader*  fallback_shader()  { return m_fallback_shader.get(); }

    // ---- Preloading ----
    // Load a batch of assets. Progress callback is invoked after each asset.
    // Supported extensions: .obj (mesh), .png/.jpg/.bmp/.tga (texture),
    // .vert+.frag (shader pair).
    void preload(const std::vector<std::string>& paths,
                 AssetProgressCallback progress = nullptr);

    // ---- Unload unused assets (ref count == 1 means only cache owns it) ----
    void unload_unused();

    // ---- Invalidate all GPU resources (for context loss) ----
    // Clears all cached GPU assets and re-creates fallback assets.
    // Call from IGame::on_context_lost(), then re-load via load_*.
    void invalidate_all();

    // ---- Cache management ----
    void clear();

    FileSystem& filesystem() const { return m_fs; }

private:
    Mesh*    load_mesh(const char* path);
    Texture* load_texture(const char* path);
    Shader*  load_shader(const char* vert_path, const char* frag_path);

    void init_fallback_assets();

    FileSystem& m_fs;

    std::unordered_map<std::string, std::shared_ptr<Mesh>>    m_mesh_cache;
    std::unordered_map<std::string, std::shared_ptr<Texture>> m_tex_cache;
    std::unordered_map<std::string, std::shared_ptr<Shader>>  m_shader_cache;

    // Fallback assets (never null, always available)
    std::unique_ptr<Texture> m_fallback_tex;
    std::unique_ptr<Mesh>    m_fallback_mesh;
    std::unique_ptr<Shader>  m_fallback_shader;

    std::string shader_key(const char* vert_path, const char* frag_path) const;
};

} // namespace pino
