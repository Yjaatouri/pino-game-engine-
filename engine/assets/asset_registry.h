#pragma once

#include "engine/core/types.h"
#include "engine/assets/asset_manifest.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace pino {

// ── Dependency graph: forward + reverse lookups ──────────────────
class DependencyGraph {
public:
    void clear();
    void set_dependencies(u64 asset_key, const std::vector<u64>& deps);
    void remove(u64 asset_key);

    const std::vector<u64>& get_dependencies(u64 asset_key) const;
    std::vector<u64> get_dependents(u64 asset_key) const;

    u32 asset_count() const { return static_cast<u32>(m_forward.size()); }

private:
    std::unordered_map<u64, std::vector<u64>> m_forward;
    std::unordered_map<u64, std::vector<u64>> m_reverse;
};

// ── Runtime asset registry ───────────────────────────────────────
// Loads a manifest and provides lookup, dependency resolution, and
// integrity verification.
class AssetRegistry {
public:
    AssetRegistry() = default;

    // Load manifest from binary data (e.g., read from file or PAK)
    bool load(const std::vector<u8>& data);

    // Load manifest from file path
    bool load_from_path(const char* path);

    // Lookup by asset key string
    const AssetManifestEntry* find(const char* key) const;
    const AssetManifestEntry* find_by_hash(u64 key_hash) const;

    // Dependency queries
    std::vector<const AssetManifestEntry*> get_dependencies(const char* key) const;
    std::vector<const AssetManifestEntry*> get_dependents(const char* key) const;

    // Integrity: verify asset_hash of a cooked payload against expectation
    bool verify_integrity(const char* key, const void* payload, u32 payload_size) const;

    // Iterate all entries
    u32 entry_count() const { return static_cast<u32>(m_entries_by_hash.size()); }

    // Access underlying data (for debugging)
    const AssetManifestData& data() const { return m_data; }
    const DependencyGraph&   graph() const { return m_graph; }

    // Check if a specific entry exists
    bool contains(const char* key) const;

private:
    void rebuild_index();

    AssetManifestData m_data;
    DependencyGraph m_graph;

    // Lookup indices
    std::unordered_map<std::string, u64>     m_key_to_hash;
    std::unordered_map<u64, AssetManifestEntry> m_entries_by_hash;
    std::unordered_map<u64, std::string>     m_hash_to_key;
};

} // namespace pino
