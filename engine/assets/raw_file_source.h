#pragma once

#include "engine/assets/asset_source.h"
#include "engine/platform/file_system.h"

namespace pino {

class RawFileSource : public IAssetSource {
public:
    explicit RawFileSource(FileSystem& fs);

    bool       exists(const char* path) const override;
    BinaryBlob load(const char* path) override;

private:
    FileSystem& m_fs;
};

} // namespace pino
