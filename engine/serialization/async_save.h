#pragma once

#include "engine/serialization/save_game_serializer.h"
#include "engine/core/serializer.h"
#include "engine/core/binary_chunk.h"
#include <atomic>
#include <thread>
#include <vector>

namespace pino {

// AsyncSaveSerializer — non-blocking entity serialization on a background
// thread.  Designed for shutdown paths where you want to avoid stalling the
// main thread while saving the game state.
//
// Usage:
//   AsyncSaveSerializer saver;
//   saver.start_async(scene);
//   while (!saver.is_done()) {
//       float pct = saver.progress() * 100.0f;
//       // ... render progress bar, continue game loop, etc.
//   }
//   const auto& buf = saver.finish();
//   fwrite(buf.data(), 1, buf.size(), file);
//
// finish() blocks until the background thread completes.
// The scene must remain alive and unmodified until finish() returns.
class AsyncSaveSerializer {
public:
    AsyncSaveSerializer();
    ~AsyncSaveSerializer();

    AsyncSaveSerializer(const AsyncSaveSerializer&) = delete;
    AsyncSaveSerializer& operator=(const AsyncSaveSerializer&) = delete;

    // Start serializing scene in a background thread.
    // scene must stay alive until finish() returns.
    void start_async(EcsScene& scene);

    // Thread-safe progress queries (atomics).
    float    progress()          const;  // 0.0 – 1.0
    uint32_t entities_serialized() const;

    bool is_done() const;

    // Block until complete and return the internal buffer.
    // Safe to call only once; returns immediately if already done.
    const std::vector<uint8_t>& finish();

private:
    void thread_func(EcsScene* scene);

    std::thread             m_thread;
    std::atomic<float>      m_progress;
    std::atomic<uint32_t>   m_entities_written;
    std::atomic<bool>       m_done;

    // Owned resources (stay alive for the thread's duration)
    TypeRegistry            m_types;
    VersionRegistry         m_versions;
    StringTable             m_strings;
    SaveGameSerializer      m_serializer;
    BinaryChunkWriter       m_writer;
    Serializer              m_ser;
    std::vector<uint8_t>    m_buffer;
};

// ── Inline helpers ──────────────────────────────────────────────

inline float AsyncSaveSerializer::progress() const {
    return m_progress.load(std::memory_order_acquire);
}

inline uint32_t AsyncSaveSerializer::entities_serialized() const {
    return m_entities_written.load(std::memory_order_acquire);
}

inline bool AsyncSaveSerializer::is_done() const {
    return m_done.load(std::memory_order_acquire);
}

} // namespace pino
