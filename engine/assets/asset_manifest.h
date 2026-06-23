#pragma once

#include "engine/core/types.h"
#include "engine/serialization/cooked_asset.h"
#include "engine/core/binary_chunk.h"
#include <string>
#include <vector>
#include <cstring>

namespace pino {

// ── FNV-1a hash for asset key strings ──────────────────────────
u64 asset_key_hash(const std::string& key);

// ── Manifest file type/version ──────────────────────────────────
static constexpr u32 MANIFEST_TYPE    = CookedType::Manifest;
static constexpr u32 MANIFEST_VERSION = CookedVersion::Manifest;

// ── A single manifest entry ─────────────────────────────────────
struct AssetManifestEntry {
    u64      key_hash       = 0;   // FNV-1a of normalized key
    u32      type_id        = 0;   // CookedType::Mesh, Texture, etc.
    u64      file_offset    = 0;   // offset in PAK (0 = packed at start, loose = 0)
    u32      file_size      = 0;   // byte size of the cooked chunk
    u64      asset_hash     = 0;   // integrity hash of cooked payload
    u32      platform_tag   = 0;   // CookedPlatform
    u32      flags          = 0;   // CookedAssetFlag
    u32      dep_count      = 0;   // number of dependency key hashes following

    // dep_count × u64 dependency key_hashes written after the fixed fields
};

// ── In-memory manifest with dependency lists ────────────────────
struct AssetManifestData {
    std::vector<AssetManifestEntry> entries;
    std::vector<std::string>        keys;           // original key strings, order matches entries
    std::vector<std::vector<u64>>   dependencies;   // per-entry dep hashes, order matches entries
};

// ── Serialization ───────────────────────────────────────────────
void write_asset_manifest(BinaryChunkWriter& writer, const AssetManifestData& manifest);
bool read_asset_manifest(BinaryChunkReader& reader, AssetManifestData& manifest);

// ── Helpers ─────────────────────────────────────────────────────
u32 calc_manifest_size(const AssetManifestData& manifest);

// Normalize a key for hashing: lowercase + forward slashes
std::string normalize_manifest_key(const std::string& key);

} // namespace pino
