#pragma once

#include "engine/core/types.h"
#include <string>

namespace pino {

class FileSystem;

class AudioManager {
public:
    AudioManager();
    ~AudioManager();

    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    bool init(FileSystem& filesystem);
    void shutdown();

    // Fire-and-forget: load, play, auto-destroy on completion
    void play_one_shot(const std::string& path, float volume = 1.0f);

    // Controllable playback: returns source id
    u64 play(const std::string& path, bool looping = false, float volume = 1.0f);
    void stop(u64 source_id);
    void set_volume(u64 source_id, float volume);
    void set_looping(u64 source_id, bool looping);

    // Master control
    void set_master_volume(float volume);
    float master_volume() const;

    bool is_ready() const { return m_ready; }

private:
    struct Impl;
    Impl* m_impl = nullptr;
    bool m_ready = false;
    FileSystem* m_filesystem = nullptr;
};

} // namespace pino
