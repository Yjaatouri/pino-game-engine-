#include "engine/core/serializer.h"

namespace pino {

Serializer::Serializer(BinaryChunkWriter& writer)
    : m_writer(&writer)
    , m_chunk_open(false)
{
}

void Serializer::beginChunk(uint32_t type_id, uint32_t version) {
    m_writer->beginChunk(type_id, version);
    m_chunk_open = true;
}

void Serializer::endChunk() {
    m_writer->endChunk();
    m_chunk_open = false;
}

void Serializer::writeUInt32(uint32_t value) {
    m_writer->writeUInt32(value);
}

void Serializer::writeInt32(int32_t value) {
    m_writer->writeInt32(value);
}

void Serializer::writeFloat(float value) {
    m_writer->writeFloat(value);
}

void Serializer::writeBool(bool value) {
    m_writer->writeBool(value);
}

void Serializer::writeVec2(const glm::vec2& v) {
    m_writer->writeFloat(v.x);
    m_writer->writeFloat(v.y);
}

void Serializer::writeVec3(const glm::vec3& v) {
    m_writer->writeFloat(v.x);
    m_writer->writeFloat(v.y);
    m_writer->writeFloat(v.z);
}

void Serializer::writeVec4(const glm::vec4& v) {
    m_writer->writeFloat(v.x);
    m_writer->writeFloat(v.y);
    m_writer->writeFloat(v.z);
    m_writer->writeFloat(v.w);
}

void Serializer::writeString(const std::string& str) {
    uint32_t len = static_cast<uint32_t>(str.size());
    m_writer->writeUInt32(len);
    if (len > 0) {
        m_writer->writeBytes(str.data(), len);
    }
}

void Serializer::writeBytes(const void* data, uint32_t size) {
    m_writer->writeBytes(data, size);
}

Deserializer::Deserializer(BinaryChunkReader& reader)
    : m_reader(&reader)
    , m_header{}
    , m_has_chunk(false)
    , m_valid(true)
    , m_remaining(0)
{
}

bool Deserializer::nextChunk() {
    if (!m_reader->nextChunk()) {
        m_has_chunk = false;
        m_valid = false;
        return false;
    }
    m_header = m_reader->getHeader();
    m_has_chunk = true;
    m_valid = true;
    m_remaining = m_header.size;
    return true;
}

const ChunkHeader& Deserializer::getHeader() const {
    return m_header;
}

bool Deserializer::skipChunk() {
    if (!m_has_chunk) return false;
    m_reader->skipChunk();
    m_has_chunk = false;
    m_remaining = 0;
    return true;
}

bool Deserializer::isValid() const {
    return m_valid;
}

bool Deserializer::canRead(uint32_t size) const {
    return m_has_chunk && m_valid && size <= m_remaining;
}

uint32_t Deserializer::readUInt32() {
    if (!canRead(4)) {
        m_valid = false;
        return 0;
    }
    uint32_t val = m_reader->readUInt32();
    m_remaining -= 4;
    return val;
}

int32_t Deserializer::readInt32() {
    if (!canRead(4)) {
        m_valid = false;
        return 0;
    }
    int32_t val = m_reader->readInt32();
    m_remaining -= 4;
    return val;
}

float Deserializer::readFloat() {
    if (!canRead(4)) {
        m_valid = false;
        return 0.0f;
    }
    float val = m_reader->readFloat();
    m_remaining -= 4;
    return val;
}

bool Deserializer::readBool() {
    if (!canRead(1)) {
        m_valid = false;
        return false;
    }
    bool val = m_reader->readBool();
    m_remaining -= 1;
    return val;
}

glm::vec2 Deserializer::readVec2() {
    float x = readFloat();
    float y = readFloat();
    return {x, y};
}

glm::vec3 Deserializer::readVec3() {
    float x = readFloat();
    float y = readFloat();
    float z = readFloat();
    return {x, y, z};
}

glm::vec4 Deserializer::readVec4() {
    float x = readFloat();
    float y = readFloat();
    float z = readFloat();
    float w = readFloat();
    return {x, y, z, w};
}

std::string Deserializer::readString() {
    if (!canRead(4)) {
        m_valid = false;
        return {};
    }
    uint32_t len = readUInt32();
    if (!m_valid || len == 0) {
        return {};
    }
    if (!canRead(len)) {
        m_valid = false;
        return {};
    }
    std::string str(len, '\0');
    m_reader->readBytes(&str[0], len);
    m_remaining -= len;
    return str;
}

void Deserializer::readBytes(void* out, uint32_t size) {
    if (!canRead(size)) {
        m_valid = false;
        return;
    }
    m_reader->readBytes(out, size);
    m_remaining -= size;
}

} // namespace pino
