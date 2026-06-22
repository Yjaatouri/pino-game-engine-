#include "engine/core/string_table.h"

namespace pino {

uint32_t StringTable::addString(const std::string& str) {
    auto it = m_string_to_index.find(str);
    if (it != m_string_to_index.end()) {
        return it->second;
    }
    uint32_t index = static_cast<uint32_t>(m_index_to_string.size());
    m_index_to_string.push_back(str);
    m_string_to_index[str] = index;
    return index;
}

const std::string& StringTable::getString(uint32_t index) const {
    static const std::string s_empty;
    if (index >= m_index_to_string.size()) {
        return s_empty;
    }
    return m_index_to_string[index];
}

uint32_t StringTable::findString(const std::string& str) const {
    auto it = m_string_to_index.find(str);
    if (it == m_string_to_index.end()) {
        return UINT32_MAX;
    }
    return it->second;
}

bool StringTable::exists(const std::string& str) const {
    return m_string_to_index.find(str) != m_string_to_index.end();
}

void StringTable::write(BinaryChunkWriter& writer) const {
    uint32_t count = static_cast<uint32_t>(m_index_to_string.size());
    writer.writeUInt32(count);
    for (const auto& s : m_index_to_string) {
        uint32_t len = static_cast<uint32_t>(s.size());
        writer.writeUInt32(len);
        if (len > 0) {
            writer.writeBytes(s.data(), len);
        }
    }
}

void StringTable::read(BinaryChunkReader& reader) {
    clear();
    uint32_t count = reader.readUInt32();
    m_index_to_string.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t len = reader.readUInt32();
        std::string str;
        if (len > 0) {
            str.resize(len);
            reader.readBytes(&str[0], len);
        }
        m_string_to_index[str] = i;
        m_index_to_string.push_back(std::move(str));
    }
}

void StringTable::clear() {
    m_string_to_index.clear();
    m_index_to_string.clear();
}

size_t StringTable::size() const {
    return m_index_to_string.size();
}

} // namespace pino
