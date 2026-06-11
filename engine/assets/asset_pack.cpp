#include "asset_pack.h"
#include "engine/core/log.h"
#include <fstream>
#include <cstring>
#include <algorithm>
#include <cctype>

namespace pino {

static constexpr u32 PACK_MAGIC = 0x4F4E4950u; // "PINO" little-endian
static constexpr u32 PACK_VERSION = 1;

// FNV-1a 64-bit hash
u64 AssetPackReader::hash_name(const std::string& name) {
    u64 h = 0xCBF29CE484222325ull;
    for (unsigned char c : name) {
        h ^= c;
        h *= 0x100000001B3ull;
    }
    return h;
}

bool AssetPackReader::load(const std::vector<u8>& data) {
    m_raw = data;
    m_entries = nullptr;
    m_entry_count = 0;
    m_data_offset = 0;

    if (data.size() < 12) {
        PINO_ERROR("Asset pack: file too small");
        return false;
    }

    // Read header
    u32 magic = 0;
    u32 version = 0;
    std::memcpy(&magic, data.data(), 4);
    std::memcpy(&version, data.data() + 4, 4);

    if (magic != PACK_MAGIC) {
        PINO_ERROR("Asset pack: invalid magic (got 0x%08x)", magic);
        return false;
    }
    if (version != PACK_VERSION) {
        PINO_ERROR("Asset pack: unsupported version %u", version);
        return false;
    }

    std::memcpy(&m_entry_count, data.data() + 8, 4);
    usize index_size = static_cast<usize>(m_entry_count) * sizeof(AssetPackEntry);
    usize header_size = 12;

    if (data.size() < header_size + index_size) {
        PINO_ERROR("Asset pack: truncated index");
        return false;
    }

    m_entries = reinterpret_cast<const AssetPackEntry*>(data.data() + header_size);
    m_data_offset = header_size + index_size;

    PINO_INFO("Asset pack loaded: %u entries, %zu bytes", m_entry_count, data.size());
    return true;
}

bool AssetPackReader::load_from_path(const char* path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        PINO_ERROR("Cannot open asset pack: %s", path);
        return false;
    }

    auto size = static_cast<usize>(file.tellg());
    file.seekg(0, std::ios::beg);
    std::vector<u8> data(size);
    file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
    if (!file) {
        PINO_ERROR("Failed to read asset pack: %s", path);
        return false;
    }

    return load(data);
}

bool AssetPackReader::read(const char* name, std::vector<u8>& out_data) const {
    if (!m_entries || !name) return false;

    // Normalize the name for lookup
    std::string norm = name;
    for (auto& ch : norm) if (ch == '\\') ch = '/';
#if defined(_WIN32)
    for (auto& ch : norm) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
#endif

    u64 h = hash_name(norm);

    for (u32 i = 0; i < m_entry_count; ++i) {
        if (m_entries[i].name_hash == h) {
            u64 offset = m_data_offset + m_entries[i].offset;
            u64 size = m_entries[i].size;
            if (offset + size > m_raw.size()) {
                PINO_ERROR("Asset pack: entry %s extends past file end", name);
                return false;
            }
            out_data.assign(m_raw.data() + offset, m_raw.data() + offset + size);
            return true;
        }
    }
    return false;
}

bool AssetPackReader::contains(const char* name) const {
    if (!m_entries || !name) return false;

    std::string norm = name;
    for (auto& ch : norm) if (ch == '\\') ch = '/';
#if defined(_WIN32)
    for (auto& ch : norm) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
#endif

    u64 h = hash_name(norm);
    for (u32 i = 0; i < m_entry_count; ++i) {
        if (m_entries[i].name_hash == h) return true;
    }
    return false;
}

// ── Offline packer ──────────────────────────────────────────────
bool write_asset_pack(const char* output_path,
                      const std::vector<AssetPackFile>& files) {
    std::ofstream out(output_path, std::ios::binary);
    if (!out) {
        PINO_ERROR("Cannot write asset pack: %s", output_path);
        return false;
    }

    u32 count = static_cast<u32>(files.size());

    // Write header
    out.write(reinterpret_cast<const char*>(&PACK_MAGIC), 4);
    out.write(reinterpret_cast<const char*>(&PACK_VERSION), 4);
    out.write(reinterpret_cast<const char*>(&count), 4);

    // Compute offsets and write index entries
    u64 data_offset = 12 + static_cast<u64>(count) * sizeof(AssetPackEntry);
    u64 current_offset = 0;

    for (u32 i = 0; i < count; ++i) {
        std::string norm = files[i].name;
        for (auto& ch : norm) if (ch == '\\') ch = '/';
#if defined(_WIN32)
        for (auto& ch : norm) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
#endif
        u64 h = 0xCBF29CE484222325ull;
        for (unsigned char c : norm) { h ^= c; h *= 0x100000001B3ull; }

        AssetPackEntry entry;
        entry.name_hash = h;
        entry.offset = current_offset;
        entry.size = files[i].data.size();

        out.write(reinterpret_cast<const char*>(&entry), sizeof(entry));
        current_offset += entry.size;
    }

    // Write data
    for (u32 i = 0; i < count; ++i) {
        out.write(reinterpret_cast<const char*>(files[i].data.data()),
                  static_cast<std::streamsize>(files[i].data.size()));
    }

    out.close();
    PINO_INFO("Asset pack written: %s (%u files, data starts at %llu)",
              output_path, count, static_cast<unsigned long long>(data_offset));
    return true;
}

} // namespace pino
