#include "asset_manifest.h"
#include "engine/core/log.h"
#include <algorithm>
#include <cctype>

namespace pino {

// ── Key normalization ───────────────────────────────────────────
std::string normalize_manifest_key(const std::string& key) {
    std::string k = key;
    for (auto& ch : k)
        if (ch == '\\') ch = '/';
    for (auto& ch : k)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return k;
}

// ── Key hash (FNV-1a 64-bit) ────────────────────────────────────
u64 asset_key_hash(const std::string& key) {
    std::string norm = normalize_manifest_key(key);
    u64 h = 0xCBF29CE484222325ull;
    for (unsigned char c : norm) {
        h ^= c;
        h *= 0x100000001B3ull;
    }
    return h;
}

// ── Serialize ────────────────────────────────────────────────────
void write_asset_manifest(BinaryChunkWriter& writer, const AssetManifestData& manifest) {
    // Build nested chunk containing the payload
    BinaryChunkWriter tmp;
    {
        Serializer stmp(tmp);
        stmp.beginChunk(0, 0);

        u32 count = static_cast<u32>(manifest.entries.size());
        stmp.writeUInt32(count);

        for (u32 i = 0; i < count; ++i) {
            const auto& e = manifest.entries[i];
            stmp.writeUInt64(e.key_hash);
            stmp.writeUInt32(e.type_id);
            stmp.writeUInt64(e.file_offset);
            stmp.writeUInt32(e.file_size);
            stmp.writeUInt64(e.asset_hash);
            stmp.writeUInt32(e.platform_tag);
            stmp.writeUInt32(e.flags);
            stmp.writeUInt32(e.dep_count);
            stmp.writeString(manifest.keys[i]);

            if (i < manifest.dependencies.size()) {
                for (u64 dep_hash : manifest.dependencies[i]) {
                    stmp.writeUInt64(dep_hash);
                }
            }
        }
        stmp.endChunk();
    }

    const auto& nested = tmp.getBuffer();
    u64 hash = cooked_hash_fnv1a(nested.data(), static_cast<u32>(nested.size()));

    writer.beginChunk(MANIFEST_TYPE, MANIFEST_VERSION);
    {
        Serializer s(writer);
        CookedAssetHeader h;
        h.asset_hash   = hash;
        h.platform_tag = static_cast<u32>(CookedPlatform::Any);
        h.flags        = CAF_None;
        write_cooked_header(s, h);
        s.writeBytes(nested.data(), static_cast<u32>(nested.size()));
    }
    writer.endChunk();
}

// ── Deserialize ──────────────────────────────────────────────────
bool read_asset_manifest(BinaryChunkReader& reader, AssetManifestData& manifest) {
    if (!reader.nextChunk()) {
        PINO_ERROR("Asset manifest: no chunk");
        return false;
    }
    const ChunkHeader& hdr = reader.getHeader();
    if (hdr.type_id != MANIFEST_TYPE) {
        PINO_ERROR("Asset manifest: bad type %u", hdr.type_id);
        return false;
    }
    if (hdr.version != MANIFEST_VERSION) {
        PINO_ERROR("Asset manifest: bad version %u", hdr.version);
        return false;
    }

    // Read common header
    u32 hash_lo = reader.readUInt32();
    u32 hash_hi = reader.readUInt32();
    u32 platform = reader.readUInt32(); (void)platform;
    u32 flags    = reader.readUInt32(); (void)flags;
    u64 expected_hash = (static_cast<u64>(hash_hi) << 32) | hash_lo;

    // Read nested chunk
    if (hdr.size < 16) { PINO_ERROR("Asset manifest: chunk too small (%u bytes)", hdr.size); return false; }
    u32 nested_size = hdr.size - 16;
    std::vector<u8> nested(nested_size);
    if (nested_size > 0)
        reader.readBytes(nested.data(), nested_size);

    // Verify integrity
    u64 actual_hash = cooked_hash_fnv1a(nested.data(), nested_size);
    if (actual_hash != expected_hash) {
        PINO_ERROR("Asset manifest: hash mismatch");
        return false;
    }

    // Deserialize nested chunk
    BinaryChunkReader nested_reader(nested.data(), nested_size);
    Deserializer d(nested_reader);
    if (!d.nextChunk()) {
        PINO_ERROR("Asset manifest: nested chunk missing");
        return false;
    }

    u32 count = d.readUInt32();
    if (!d.isValid()) {
        PINO_ERROR("Asset manifest: failed to read entry count");
        return false;
    }

    manifest.entries.clear();
    manifest.keys.clear();
    manifest.dependencies.clear();
    manifest.entries.reserve(count);
    manifest.keys.reserve(count);
    manifest.dependencies.reserve(count);

    for (u32 i = 0; i < count; ++i) {
        AssetManifestEntry e;
        e.key_hash     = d.readUInt64();
        e.type_id      = d.readUInt32();
        e.file_offset  = d.readUInt64();
        e.file_size    = d.readUInt32();
        e.asset_hash   = d.readUInt64();
        e.platform_tag = d.readUInt32();
        e.flags        = d.readUInt32();
        e.dep_count    = d.readUInt32();

        std::string key = d.readString();
        std::vector<u64> deps;
        deps.reserve(e.dep_count);
        for (u32 j = 0; j < e.dep_count; ++j) {
            deps.push_back(d.readUInt64());
        }

        if (!d.isValid()) {
            PINO_ERROR("Asset manifest: deserialization failed at entry %u", i);
            return false;
        }

        manifest.entries.push_back(e);
        manifest.keys.push_back(key);
        manifest.dependencies.push_back(std::move(deps));
    }

    return true;
}

// ── Calculate manifest size ──────────────────────────────────────
u32 calc_manifest_size(const AssetManifestData& manifest) {
    u32 size = 4; // entry count
    for (u32 i = 0; i < manifest.entries.size(); ++i) {
        size += 8 + 4 + 8 + 4 + 8 + 4 + 4 + 4; // fixed fields
        size += static_cast<u32>(4 + manifest.keys[i].size()); // string
        size += static_cast<u32>(manifest.dependencies[i].size() * 8); // dep hashes
    }
    return size;
}

} // namespace pino
