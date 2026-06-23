#include "raw_file_source.h"
#include "engine/core/log.h"

namespace pino {

RawFileSource::RawFileSource(FileSystem& fs) : m_fs(fs) {}

bool RawFileSource::exists(const char* path) const {
    return m_fs.exists(path);
}

BinaryBlob RawFileSource::load(const char* path) {
    BinaryBlob result;
    result.debug_path = path;
    result.data = m_fs.read_binary(path);
    if (result.data.empty()) {
        PINO_WARN("RawFileSource: failed to read '%s'", path);
    }
    return result;
}

} // namespace pino
