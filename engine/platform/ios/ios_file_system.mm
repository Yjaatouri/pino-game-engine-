#include "ios_file_system.h"
#include "engine/core/log.h"
#import <Foundation/Foundation.h>

namespace pino {

IOSFileSystem::IOSFileSystem() {
    // Assets live in the app bundle's resource directory
    NSString* path = [[NSBundle mainBundle] resourcePath];
    m_base = [path UTF8String];
    if (!m_base.empty() && m_base.back() != '/')
        m_base.push_back('/');
    PINO_INFO("iOS FileSystem base: %s", m_base.c_str());
}

std::vector<u8> IOSFileSystem::read_binary(const char* path) {
    std::string full = resolve(path);
    NSString* nsPath = [NSString stringWithUTF8String:full.c_str()];
    NSData* data = [NSData dataWithContentsOfFile:nsPath];
    if (!data) {
        PINO_ERROR("IOSFileSystem: cannot open %s", full.c_str());
        return {};
    }
    const u8* bytes = static_cast<const u8*>([data bytes]);
    return std::vector<u8>(bytes, bytes + [data length]);
}

std::string IOSFileSystem::read_text(const char* path) {
    auto data = read_binary(path);
    if (data.empty()) return {};
    return std::string(reinterpret_cast<const char*>(data.data()), data.size());
}

bool IOSFileSystem::exists(const char* path) const {
    std::string full = resolve(path);
    NSString* nsPath = [NSString stringWithUTF8String:full.c_str()];
    return [[NSFileManager defaultManager] fileExistsAtPath:nsPath];
}

std::string IOSFileSystem::resolve(const char* path) const {
    if (path[0] == '/') return path;
    return m_base + path;
}

} // namespace pino
