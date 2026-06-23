#pragma once

#include "engine/core/types.h"
#include <string>
#include <vector>

namespace pino {

struct BinaryBlob {
    std::vector<u8> data;
    std::string     debug_path;
};

class IAssetSource {
public:
    virtual ~IAssetSource() = default;

    virtual bool     exists(const char* asset_key) const = 0;
    virtual BinaryBlob load(const char* asset_key) = 0;
};

} // namespace pino
