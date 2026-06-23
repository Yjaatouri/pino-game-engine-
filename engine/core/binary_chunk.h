#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace pino {

static constexpr uint32_t kChunkMagic = 0x50494E4Fu;

struct ChunkHeader {
    uint32_t magic;
    uint32_t type_id;
    uint32_t version;
    uint32_t size;
};

class BinaryChunkWriter {
public:
    BinaryChunkWriter();

    void beginChunk(uint32_t type_id, uint32_t version);

    void writeBytes(const void* data, uint32_t size);
    void writeUInt32(uint32_t value);
    void writeUInt64(uint64_t value);
    void writeFloat(float value);
    void writeInt32(int32_t value);
    void writeBool(bool value);

    void endChunk();

    const std::vector<uint8_t>& getBuffer() const;
    void clear();

private:
    void writeRaw(const void* data, uint32_t size);

    std::vector<uint8_t> m_buffer;
    size_t m_chunk_start;
    bool m_chunk_open;
};

class BinaryChunkReader {
public:
    BinaryChunkReader(const uint8_t* data, size_t size);

    bool nextChunk();
    const ChunkHeader& getHeader() const;

    bool isValid() const;
    bool skipChunk();

    void readBytes(void* out, uint32_t size);
    uint32_t readUInt32();
    uint64_t readUInt64();
    float readFloat();
    int32_t readInt32();
    bool readBool();

    bool endOfStream() const;

private:
    const uint8_t* m_data;
    size_t m_total_size;
    size_t m_cursor;
    ChunkHeader m_header;
    size_t m_payload_end;
    size_t m_payload_cursor;
    bool m_valid;
    bool m_has_chunk;
};

} // namespace pino
