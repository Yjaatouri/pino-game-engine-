#pragma once

#include "engine/core/types.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace pino {

// ── Asset pack format ───────────────────────────────────────────
// Binary format (.pino_pack):
//   [Header]    magic=4 bytes "PINO", version=u32, entry_count=u32
//   [Index]     entry_count × { name_hash=u64, offset=u64, size=u64 }
//   [Data]      raw file data at each offset
//
// name_hash is a simple hash of the normalized asset path.
// Offsets are relative to the start of the Data section.

struct AssetPackEntry {
    u64 name_hash;
    u64 offset;
    u64 size;
};

class AssetPackReader {
public:
    AssetPackReader() = default;

    // Load the pack file into memory
    bool load(const std::vector<u8>& data);
    bool load_from_path(const char* path);

    // Find an asset by normalized path. Returns true + data if found.
    bool read(const char* name, std::vector<u8>& out_data) const;

    // Check if a specific asset exists in the pack
    bool contains(const char* name) const;

    // Number of entries loaded
    u32 entry_count() const { return m_entry_count; }

    // Access raw data
    const std::vector<u8>& raw_data() const { return m_raw; }

private:
    static u64 hash_name(const std::string& name);

    std::vector<u8> m_raw;
    const AssetPackEntry* m_entries = nullptr;
    u32 m_entry_count = 0;
    u64 m_data_offset = 0;
};

// ── Offline packer (used at build time) ─────────────────────────
// Writes a .pino_pack file from a list of { name, data } pairs.
struct AssetPackFile {
    std::string name;
    std::vector<u8> data;
};

bool write_asset_pack(const char* output_path,
                      const std::vector<AssetPackFile>& files);

} // namespace pino
