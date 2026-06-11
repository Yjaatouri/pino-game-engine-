#pragma once

#include "engine/core/types.h"
#include <string>
#include <vector>

namespace pino {

class FileSystem {
public:
    virtual ~FileSystem() = default;

    virtual std::vector<u8> read_binary(const char* path)       = 0;
    virtual std::string     read_text(const char* path)         = 0;
    virtual bool            exists(const char* path)      const = 0;
    virtual std::string     resolve(const char* path)     const = 0;

    virtual const std::string& base_path() const = 0;
};

} // namespace pino
