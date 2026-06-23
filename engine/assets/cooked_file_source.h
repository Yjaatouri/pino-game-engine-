#pragma once

#include "engine/assets/asset_source.h"
#include "engine/platform/file_system.h"
#include "engine/assets/asset_registry.h"

namespace pino {

// CookedFileSource reads .pino_cooked files from a cooked directory.
// The caller is responsible for resolving asset keys to canonical form
// via AssetRegistry::resolve() before calling load/exists.
class CookedFileSource : public IAssetSource {
public:
    CookedFileSource(FileSystem& fs, const AssetRegistry& registry, const char* cooked_dir);

    bool       exists(const char* resolved_key) const override;
    BinaryBlob load(const char* resolved_key) override;

    const AssetRegistry& registry() const { return m_registry; }

private:
    std::string cooked_file_path(const std::string& asset_key) const;

    FileSystem&          m_fs;
    const AssetRegistry& m_registry;
    std::string          m_cooked_dir;
};

} // namespace pino
