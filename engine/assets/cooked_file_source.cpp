#include "cooked_file_source.h"
#include "engine/serialization/cooked_asset.h"
#include "engine/core/log.h"

namespace pino {

CookedFileSource::CookedFileSource(FileSystem& fs, const AssetRegistry& registry, const char* cooked_dir)
    : m_fs(fs)
    , m_registry(registry)
{
    if (cooked_dir) m_cooked_dir = cooked_dir;
    for (auto& ch : m_cooked_dir) if (ch == '\\') ch = '/';
    if (!m_cooked_dir.empty() && m_cooked_dir.back() != '/')
        m_cooked_dir.push_back('/');
}

bool CookedFileSource::exists(const char* asset_key) const {
    return m_registry.contains(asset_key);
}

BinaryBlob CookedFileSource::load(const char* asset_key) {
    BinaryBlob result;

    const auto* entry = m_registry.find(asset_key);
    if (!entry) {
        PINO_WARN("CookedFileSource: unknown asset '%s'", asset_key);
        return result;
    }

    std::string path = cooked_file_path(asset_key);
    result.debug_path = path;

    result.data = m_fs.read_binary(path.c_str());
    if (result.data.empty()) {
        PINO_WARN("CookedFileSource: file missing: %s", path.c_str());
        return result;
    }

    u64 actual = cooked_hash_fnv1a(result.data.data(), static_cast<u32>(result.data.size()));
    if (actual != entry->asset_hash) {
        PINO_ERROR("CookedFileSource: hash mismatch for '%s'"
                   " (expected 0x%llx, got 0x%llx)",
                   asset_key,
                   static_cast<unsigned long long>(entry->asset_hash),
                   static_cast<unsigned long long>(actual));
        result.data.clear();
        return result;
    }

    return result;
}

std::string CookedFileSource::cooked_file_path(const std::string& asset_key) const {
    std::string filename = asset_key;
    for (auto& ch : filename) if (ch == '/') ch = '_';
    return m_cooked_dir + filename + ".pino_cooked";
}

} // namespace pino
