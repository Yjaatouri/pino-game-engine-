#pragma once

#include "engine/core/binary_chunk.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace pino {

class StringTable {
public:
    static constexpr uint32_t kChunkType = 50;
    static constexpr uint32_t kChunkVersion = 1;

    static constexpr uint32_t kMaxStringCount  = 65536;
    static constexpr uint32_t kMaxStringLength = 65536;

    uint32_t addString(const std::string& str);
    const std::string& getString(uint32_t index) const;

    uint32_t findString(const std::string& str) const;
    bool exists(const std::string& str) const;

    void write(BinaryChunkWriter& writer) const;
    void read(BinaryChunkReader& reader);

    void clear();
    size_t size() const;

private:
    std::unordered_map<std::string, uint32_t> m_string_to_index;
    std::vector<std::string> m_index_to_string;
};

} // namespace pino
