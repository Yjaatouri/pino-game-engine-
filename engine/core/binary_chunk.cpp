#include "engine/core/binary_chunk.h"
#include <cstring>
#include <cassert>

namespace pino {

namespace {

inline void writeLE32(uint8_t* buf, uint32_t val) {
    buf[0] = static_cast<uint8_t>(val & 0xFFu);
    buf[1] = static_cast<uint8_t>((val >> 8) & 0xFFu);
    buf[2] = static_cast<uint8_t>((val >> 16) & 0xFFu);
    buf[3] = static_cast<uint8_t>((val >> 24) & 0xFFu);
}

inline uint32_t readLE32(const uint8_t* buf) {
    return static_cast<uint32_t>(buf[0]) |
           (static_cast<uint32_t>(buf[1]) << 8) |
           (static_cast<uint32_t>(buf[2]) << 16) |
           (static_cast<uint32_t>(buf[3]) << 24);
}

inline void writeLE_f32(uint8_t* buf, float val) {
    uint32_t bits;
    std::memcpy(&bits, &val, sizeof(bits));
    writeLE32(buf, bits);
}

inline float readLE_f32(const uint8_t* buf) {
    uint32_t bits = readLE32(buf);
    float val;
    std::memcpy(&val, &bits, sizeof(val));
    return val;
}

} // anonymous namespace

BinaryChunkWriter::BinaryChunkWriter()
    : m_chunk_start(0)
    , m_chunk_open(false)
{
}

void BinaryChunkWriter::beginChunk(uint32_t type_id, uint32_t version) {
    assert(!m_chunk_open);

    m_chunk_start = m_buffer.size();

    uint8_t hdr[16];
    writeLE32(hdr + 0,  kChunkMagic);
    writeLE32(hdr + 4,  type_id);
    writeLE32(hdr + 8,  version);
    writeLE32(hdr + 12, 0);
    writeRaw(hdr, 16);

    m_chunk_open = true;
}

void BinaryChunkWriter::endChunk() {
    assert(m_chunk_open);

    uint32_t payload_size = static_cast<uint32_t>(
        m_buffer.size() - m_chunk_start - 16);

    uint8_t size_buf[4];
    writeLE32(size_buf, payload_size);
    std::memcpy(&m_buffer[m_chunk_start + 12], size_buf, 4);

    m_chunk_open = false;
}

void BinaryChunkWriter::writeBytes(const void* data, uint32_t size) {
    assert(m_chunk_open);
    writeRaw(data, size);
}

void BinaryChunkWriter::writeUInt32(uint32_t value) {
    uint8_t buf[4];
    writeLE32(buf, value);
    writeRaw(buf, 4);
}

void BinaryChunkWriter::writeFloat(float value) {
    uint8_t buf[4];
    writeLE_f32(buf, value);
    writeRaw(buf, 4);
}

void BinaryChunkWriter::writeInt32(int32_t value) {
    writeUInt32(static_cast<uint32_t>(value));
}

void BinaryChunkWriter::writeBool(bool value) {
    uint8_t byte = value ? 1 : 0;
    writeRaw(&byte, 1);
}

const std::vector<uint8_t>& BinaryChunkWriter::getBuffer() const {
    return m_buffer;
}

void BinaryChunkWriter::clear() {
    m_buffer.clear();
    m_chunk_start = 0;
    m_chunk_open = false;
}

void BinaryChunkWriter::writeRaw(const void* data, uint32_t size) {
    const uint8_t* src = static_cast<const uint8_t*>(data);
    m_buffer.insert(m_buffer.end(), src, src + size);
}

BinaryChunkReader::BinaryChunkReader(const uint8_t* data, size_t size)
    : m_data(data)
    , m_total_size(size)
    , m_cursor(0)
    , m_header{}
    , m_payload_end(0)
    , m_payload_cursor(0)
    , m_valid(false)
    , m_has_chunk(false)
{
}

bool BinaryChunkReader::nextChunk() {
    if (m_cursor >= m_total_size) {
        m_has_chunk = false;
        return false;
    }

    if (m_cursor + 16 > m_total_size) {
        m_valid = false;
        m_has_chunk = false;
        return false;
    }

    uint32_t magic   = readLE32(m_data + m_cursor + 0);
    uint32_t type_id = readLE32(m_data + m_cursor + 4);
    uint32_t version = readLE32(m_data + m_cursor + 8);
    uint32_t size    = readLE32(m_data + m_cursor + 12);

    if (magic != kChunkMagic) {
        m_valid = false;
        m_has_chunk = false;
        return false;
    }

    size_t chunk_total = 16 + size;
    if (m_cursor + chunk_total > m_total_size) {
        m_valid = false;
        m_has_chunk = false;
        return false;
    }

    m_header.magic   = magic;
    m_header.type_id = type_id;
    m_header.version = version;
    m_header.size    = size;

    m_cursor += 16;
    m_payload_cursor = m_cursor;
    m_payload_end = m_cursor + size;
    m_valid = true;
    m_has_chunk = true;
    return true;
}

const ChunkHeader& BinaryChunkReader::getHeader() const {
    return m_header;
}

bool BinaryChunkReader::isValid() const {
    return m_valid;
}

bool BinaryChunkReader::skipChunk() {
    if (!m_has_chunk) return false;

    m_cursor = m_payload_end;
    m_has_chunk = false;
    m_payload_cursor = 0;
    m_payload_end = 0;
    return true;
}

void BinaryChunkReader::readBytes(void* out, uint32_t size) {
    assert(m_has_chunk);
    assert(m_payload_cursor + size <= m_payload_end);

    std::memcpy(out, m_data + m_payload_cursor, size);
    m_payload_cursor += size;
}

uint32_t BinaryChunkReader::readUInt32() {
    assert(m_has_chunk);
    assert(m_payload_cursor + 4 <= m_payload_end);

    uint32_t val = readLE32(m_data + m_payload_cursor);
    m_payload_cursor += 4;
    return val;
}

float BinaryChunkReader::readFloat() {
    assert(m_has_chunk);
    assert(m_payload_cursor + 4 <= m_payload_end);

    float val = readLE_f32(m_data + m_payload_cursor);
    m_payload_cursor += 4;
    return val;
}

int32_t BinaryChunkReader::readInt32() {
    return static_cast<int32_t>(readUInt32());
}

bool BinaryChunkReader::readBool() {
    assert(m_has_chunk);
    assert(m_payload_cursor + 1 <= m_payload_end);

    uint8_t byte = m_data[m_payload_cursor];
    m_payload_cursor += 1;
    return byte != 0;
}

bool BinaryChunkReader::endOfStream() const {
    return m_cursor >= m_total_size;
}

} // namespace pino
