#pragma once

#include "engine/assets/asset_source.h"
#include "engine/platform/file_system.h"
#include "engine/assets/asset_registry.h"

namespace pino {

class CookedFileSource : public IAssetSource {
public:
    CookedFileSource(FileSystem& fs, const AssetRegistry& registry, const char* cooked_dir);

    bool       exists(const char* asset_key) const override;
    BinaryBlob load(const char* asset_key) override;

    const AssetRegistry& registry() const { return m_registry; }

private:
    // Strips file extension from a path: "models/cube.obj" -> "models/cube"
    static std::string strip_extension(const std::string& path);
    std::string cooked_file_path(const std::string& asset_key) const;

    FileSystem&          m_fs;
    const AssetRegistry& m_registry;
    std::string          m_cooked_dir;
};

} // namespace pino
