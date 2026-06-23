#pragma once

#include "engine/core/types.h"
#include "engine/core/asset_handle.h"
#include "engine/platform/file_system.h"
#include "engine/renderer/shader.h"
#include "engine/renderer/mesh.h"
#include "engine/renderer/texture.h"
#include "engine/assets/asset_source.h"
#include "engine/assets/asset_registry.h"
#include "engine/assets/asset_utils.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <functional>

namespace pino {

// ── Progress callback type ──────────────────────────────────────
using AssetProgressCallback = std::function<void(u32 loaded, u32 total, const char* current)>;

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

    // ---- Unload unused assets (ref count == 1 means only cache owns it) ----
    void unload_unused();

    // ---- Invalidate all GPU resources (for context loss) ----
    // Clears all cached GPU assets and re-creates fallback assets.
    // Call from IGame::on_context_lost(), then re-load via load_*.
    void invalidate_all();

    // ---- Cache management ----
    void clear();

    // ---- Cooked asset manifest support ----
    // Load a cooked asset manifest. When loaded, get_* will try cooked assets
    // before falling back to raw source loading.
    // cooked_dir is the directory containing the .pino_cooked files.
    bool load_cooked_manifest(const char* manifest_path, const char* cooked_dir, FileSystem& fs);

    // Check if cooked manifest is loaded
    bool is_cooked_mode() const { return m_cooked_source != nullptr; }

    // ---- Cache size queries ----
    u32 mesh_cache_size()    const { return static_cast<u32>(m_mesh_cache.size()); }
    u32 texture_cache_size() const { return static_cast<u32>(m_tex_cache.size()); }
    u32 shader_cache_size()  const { return static_cast<u32>(m_shader_cache.size()); }

    // Access the asset registry (for tools / debugging)
    const AssetRegistry* registry() const { return &m_registry; }

    // ---- Debug API ----
    // Prints the source (cooked/raw/cached) for a given asset key.
    void print_loaded_asset_source(const char* path) const;
    void print_loaded_asset_source(const char* vert_path, const char* frag_path) const;

    // Prints the full resolution chain: normalize → cache → manifest → source → hash.
    void dump_asset_resolution_chain(const char* path) const;
    void dump_asset_resolution_chain(const char* vert_path, const char* frag_path) const;

private:
    Mesh*    load_mesh(const char* path);
    Texture* load_texture(const char* path);
    Shader*  load_shader(const char* vert_path, const char* frag_path);

    void init_fallback_assets();

    // Cooked blob deserialization helpers
    bool load_mesh_from_cooked_blob(const BinaryBlob& blob, Mesh*& out_mesh, std::shared_ptr<Mesh>& out_shared);
    bool load_texture_from_cooked_blob(const BinaryBlob& blob, Texture*& out_tex, std::shared_ptr<Texture>& out_shared);
    bool load_shader_from_cooked_blob(const BinaryBlob& blob, Shader*& out_shader, std::shared_ptr<Shader>& out_shared);

    // Raw blob parsing helpers
    bool load_mesh_from_raw_blob(const BinaryBlob& blob, Mesh*& out_mesh, std::shared_ptr<Mesh>& out_shared);
    bool load_texture_from_raw_blob(const BinaryBlob& blob, Texture*& out_tex, std::shared_ptr<Texture>& out_shared);
    bool load_shader_from_raw_blobs(const BinaryBlob& vert_blob, const BinaryBlob& frag_blob,
                                    Shader*& out_shader, std::shared_ptr<Shader>& out_shared);

    std::unordered_map<std::string, std::shared_ptr<Mesh>>    m_mesh_cache;
    std::unordered_map<std::string, std::shared_ptr<Texture>> m_tex_cache;
    std::unordered_map<std::string, std::shared_ptr<Shader>>  m_shader_cache;

    // Fallback assets (never null, always available)
    std::unique_ptr<Texture> m_fallback_tex;
    std::unique_ptr<Mesh>    m_fallback_mesh;
    std::unique_ptr<Shader>  m_fallback_shader;

    std::string shader_key(const char* vert_path, const char* frag_path) const;

    // Asset sources
    std::unique_ptr<IAssetSource> m_cooked_source; // set when cooked manifest loaded
    std::unique_ptr<IAssetSource> m_raw_source;     // always valid

    // Cooked asset registry (populated by load_cooked_manifest)
    AssetRegistry m_registry;
};

} // namespace pino
