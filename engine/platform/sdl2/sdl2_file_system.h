#pragma once

#include "engine/platform/file_system.h"
#include <string>

namespace pino {

class Sdl2FileSystem final : public FileSystem {
public:
    explicit Sdl2FileSystem(std::string base_path);
    ~Sdl2FileSystem() override = default;

    std::vector<u8> read_binary(const char* path)       override;
    std::string     read_text(const char* path)         override;
    bool            exists(const char* path)      const override;
    std::string     resolve(const char* path)     const override;

    const std::string& base_path() const override { return m_base; }

private:
    std::string m_base;
};

} // namespace pino
