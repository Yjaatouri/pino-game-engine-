#include "android_file_system.h"
#include "engine/core/log.h"
#include <algorithm>

namespace pino {

AndroidFileSystem::AndroidFileSystem(AAssetManager* mgr)
    : m_mgr(mgr)
    , m_base("")
{
    PINO_INFO("AndroidFileSystem using AAssetManager");
}

std::vector<u8> AndroidFileSystem::read_binary(const char* path) {
    std::string full = resolve(path);
    AAsset* asset = AAssetManager_open(m_mgr, full.c_str(), AASSET_MODE_STREAMING);
    if (!asset) {
        PINO_ERROR("AAssetManager_open failed: %s", full.c_str());
        return {};
    }

    off_t size = AAsset_getLength(asset);
    std::vector<u8> data(static_cast<usize>(size));
    int read = AAsset_read(asset, data.data(), static_cast<size_t>(size));
    AAsset_close(asset);

    if (read < 0) {
        PINO_ERROR("AAsset_read failed: %s", full.c_str());
        return {};
    }

    return data;
}

std::string AndroidFileSystem::read_text(const char* path) {
    auto data = read_binary(path);
    if (data.empty()) return {};
    return std::string(reinterpret_cast<const char*>(data.data()), data.size());
}

bool AndroidFileSystem::exists(const char* path) const {
    std::string full = resolve(path);
    AAsset* asset = AAssetManager_open(m_mgr, full.c_str(), AASSET_MODE_UNKNOWN);
    if (!asset) return false;
    AAsset_close(asset);
    return true;
}

std::string AndroidFileSystem::resolve(const char* path) const {
    return path;
}

} // namespace pino
