#include "sdl2_file_system.h"
#include "engine/core/log.h"

#include <fstream>
#include <iterator>
#include <algorithm>
#include <cctype>

namespace pino {

Sdl2FileSystem::Sdl2FileSystem(std::string base_path)
    : m_base(std::move(base_path))
{
    for (auto& ch : m_base) if (ch == '\\') ch = '/';
    if (!m_base.empty() && m_base.back() != '/')
        m_base.push_back('/');
    PINO_INFO("FileSystem base: %s", m_base.c_str());
}

std::vector<u8> Sdl2FileSystem::read_binary(const char* path) {
    std::string full = resolve(path);
    std::ifstream file(full, std::ios::binary | std::ios::ate);
    if (!file) { PINO_ERROR("Cannot open: %s", full.c_str()); return {}; }

    auto size = static_cast<usize>(file.tellg());
    file.seekg(0, std::ios::beg);
    std::vector<u8> data(size);
    file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
    if (!file) { PINO_ERROR("Failed to read: %s", full.c_str()); return {}; }
    return data;
}

std::string Sdl2FileSystem::read_text(const char* path) {
    std::string full = resolve(path);
    std::ifstream file(full);
    if (!file) { PINO_ERROR("Cannot open: %s", full.c_str()); return {}; }
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

bool Sdl2FileSystem::exists(const char* path) const {
    return std::ifstream(resolve(path)).good();
}

std::string Sdl2FileSystem::resolve(const char* path) const {
    // If path is absolute, return as-is (Windows drive letter or Unix /)
    if (path[0] == '/' || path[0] == '\\') return path;
    if (std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':')
        return path;
    return m_base + path;
}

} // namespace pino
