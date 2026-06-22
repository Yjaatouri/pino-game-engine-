#pragma once

#include "engine/core/binary_chunk.h"
#include <glm/glm.hpp>
#include <string>

namespace pino {

class Serializer {
public:
    explicit Serializer(BinaryChunkWriter& writer);

    void beginChunk(uint32_t type_id, uint32_t version);
    void endChunk();

    void writeUInt32(uint32_t value);
    void writeInt32(int32_t value);
    void writeFloat(float value);
    void writeBool(bool value);

    void writeVec2(const glm::vec2& v);
    void writeVec3(const glm::vec3& v);
    void writeVec4(const glm::vec4& v);

    void writeString(const std::string& str);

    void writeBytes(const void* data, uint32_t size);

private:
    BinaryChunkWriter* m_writer;
    bool m_chunk_open;
};

class Deserializer {
public:
    explicit Deserializer(BinaryChunkReader& reader);

    bool nextChunk();
    const ChunkHeader& getHeader() const;
    bool skipChunk();
    bool isValid() const;

    uint32_t readUInt32();
    int32_t readInt32();
    float readFloat();
    bool readBool();

    glm::vec2 readVec2();
    glm::vec3 readVec3();
    glm::vec4 readVec4();

    std::string readString();

    void readBytes(void* out, uint32_t size);

private:
    bool canRead(uint32_t size) const;

    BinaryChunkReader* m_reader;
    ChunkHeader m_header;
    bool m_has_chunk;
    bool m_valid;
    size_t m_remaining;
};

} // namespace pino
