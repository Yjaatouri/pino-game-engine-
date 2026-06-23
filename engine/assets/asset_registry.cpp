#include "asset_registry.h"
#include "engine/core/log.h"
#include <fstream>
#include <algorithm>
#include <cstring>

namespace pino {

// ═══════════════════════════════════════════════════════════════════
//  DependencyGraph
// ═══════════════════════════════════════════════════════════════════

void DependencyGraph::clear() {
    m_forward.clear();
    m_reverse.clear();
}

void DependencyGraph::set_dependencies(u64 asset_key, const std::vector<u64>& deps) {
    // Remove old reverse entries for this asset
    auto old_it = m_forward.find(asset_key);
    if (old_it != m_forward.end()) {
        for (u64 old_dep : old_it->second) {
            auto& rev = m_reverse[old_dep];
            rev.erase(std::remove(rev.begin(), rev.end(), asset_key), rev.end());
            if (rev.empty()) m_reverse.erase(old_dep);
        }
    }

    // Set new forward deps
    m_forward[asset_key] = deps;

    // Register reverse deps
    for (u64 dep : deps) {
        auto& rev = m_reverse[dep];
        if (std::find(rev.begin(), rev.end(), asset_key) == rev.end()) {
            rev.push_back(asset_key);
        }
    }
}

void DependencyGraph::remove(u64 asset_key) {
    auto it = m_forward.find(asset_key);
    if (it != m_forward.end()) {
        for (u64 dep : it->second) {
            auto& rev = m_reverse[dep];
            rev.erase(std::remove(rev.begin(), rev.end(), asset_key), rev.end());
            if (rev.empty()) m_reverse.erase(dep);
        }
        m_forward.erase(it);
    }
    m_reverse.erase(asset_key);
}

const std::vector<u64>& DependencyGraph::get_dependencies(u64 asset_key) const {
    static const std::vector<u64> kEmpty;
    auto it = m_forward.find(asset_key);
    return it != m_forward.end() ? it->second : kEmpty;
}

std::vector<u64> DependencyGraph::get_dependents(u64 asset_key) const {
    auto it = m_reverse.find(asset_key);
    return it != m_reverse.end() ? it->second : std::vector<u64>();
}

// ═══════════════════════════════════════════════════════════════════
//  AssetRegistry
// ═══════════════════════════════════════════════════════════════════

bool AssetRegistry::load(const std::vector<u8>& data) {
    BinaryChunkReader reader(data.data(), static_cast<u32>(data.size()));
    if (!read_asset_manifest(reader, m_data)) {
        PINO_ERROR("AssetRegistry: failed to load manifest");
        return false;
    }
    rebuild_index();
    PINO_INFO("AssetRegistry: loaded %u entries", entry_count());
    return true;
}

bool AssetRegistry::load_from_path(const char* path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        PINO_ERROR("AssetRegistry: cannot open %s", path);
        return false;
    }
    usize size = static_cast<usize>(file.tellg());
    file.seekg(0);
    std::vector<u8> data(size);
    if (!file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size))) {
        PINO_ERROR("AssetRegistry: failed to read %s", path);
        return false;
    }
    return load(data);
}

void AssetRegistry::rebuild_index() {
    m_key_to_hash.clear();
    m_entries_by_hash.clear();
    m_hash_to_key.clear();
    m_graph.clear();

    for (u32 i = 0; i < m_data.entries.size(); ++i) {
        const auto& entry = m_data.entries[i];
        const auto& key   = m_data.keys[i];
        const auto& deps  = m_data.dependencies[i];

        m_key_to_hash[key] = entry.key_hash;
        m_entries_by_hash[entry.key_hash] = entry;
        m_hash_to_key[entry.key_hash] = key;

        m_graph.set_dependencies(entry.key_hash, deps);
    }
}

const AssetManifestEntry* AssetRegistry::find(const char* key) const {
    if (!key) return nullptr;
    u64 h = asset_key_hash(key);
    return find_by_hash(h);
}

const AssetManifestEntry* AssetRegistry::find_by_hash(u64 key_hash) const {
    auto it = m_entries_by_hash.find(key_hash);
    return it != m_entries_by_hash.end() ? &it->second : nullptr;
}

bool AssetRegistry::contains(const char* key) const {
    return find(key) != nullptr;
}

std::vector<const AssetManifestEntry*> AssetRegistry::get_dependencies(const char* key) const {
    std::vector<const AssetManifestEntry*> result;
    if (!key) return result;
    u64 h = asset_key_hash(key);
    auto deps = m_graph.get_dependencies(h);
    result.reserve(deps.size());
    for (u64 dep_hash : deps) {
        auto* entry = find_by_hash(dep_hash);
        if (entry) result.push_back(entry);
    }
    return result;
}

std::vector<const AssetManifestEntry*> AssetRegistry::get_dependents(const char* key) const {
    std::vector<const AssetManifestEntry*> result;
    if (!key) return result;
    u64 h = asset_key_hash(key);
    auto deps = m_graph.get_dependents(h);
    result.reserve(deps.size());
    for (u64 dep_hash : deps) {
        auto* entry = find_by_hash(dep_hash);
        if (entry) result.push_back(entry);
    }
    return result;
}

bool AssetRegistry::verify_integrity(const char* key, const void* payload, u32 payload_size) const {
    auto* entry = find(key);
    if (!entry) {
        PINO_WARN("Integrity check: unknown asset '%s'", key);
        return false;
    }
    u64 actual = cooked_hash_fnv1a(payload, payload_size);
    if (actual != entry->asset_hash) {
        PINO_ERROR("Integrity check FAILED for '%s': expected 0x%llx, got 0x%llx",
                   key,
                   static_cast<unsigned long long>(entry->asset_hash),
                   static_cast<unsigned long long>(actual));
        return false;
    }
    return true;
}

} // namespace pino
