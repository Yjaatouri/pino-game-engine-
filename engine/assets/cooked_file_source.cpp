#include "cooked_file_source.h"
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

bool CookedFileSource::exists(const char* resolved_key) const {
    return m_registry.contains(resolved_key);
}

BinaryBlob CookedFileSource::load(const char* resolved_key) {
    BinaryBlob result;

    const auto* entry = m_registry.find(resolved_key);
    if (!entry) {
        PINO_WARN("CookedFileSource: unknown asset '%s'", resolved_key);
        return result;
    }

    std::string path = cooked_file_path(resolved_key);
    result.debug_path = path;

    result.data = m_fs.read_binary(path.c_str());
    if (result.data.empty()) {
        PINO_WARN("CookedFileSource: file missing: %s", path.c_str());
    }

    return result;
}

std::string CookedFileSource::cooked_file_path(const std::string& asset_key) const {
    std::string filename = asset_key;
    for (auto& ch : filename) if (ch == '/') ch = '_';
    return m_cooked_dir + filename + ".pino_cooked";
}

} // namespace pino
