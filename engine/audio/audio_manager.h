#pragma once

#include "engine/core/types.h"
#include <string>
#include <unordered_set>

namespace pino {

class FileSystem;

enum class Priority : u8 {
    Critical,  // never stolen
    Gameplay,  // default, can be stolen
    Ambient    // first to be stolen
};

enum class AudioBus : u8 {
    SFX,
    Music,
    Voice
};

struct AudioDebugInfo {
    u32 active_sounds;
    u32 one_shot_sounds;
    u32 total_sounds;
    u32 max_voices;
    f32 master_volume;
};

class SoundHandle {
public:
    SoundHandle() = default;
    explicit SoundHandle(std::string path) : m_path(std::move(path)) {}

    const std::string& path() const { return m_path; }
    bool is_valid() const { return !m_path.empty(); }

private:
    std::string m_path;

    friend class AudioManager;
};

class AudioManager {
public:
    AudioManager();
    ~AudioManager();

    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    bool init(FileSystem& filesystem, u32 max_voices = 32);
    u32 max_voices() const { return m_max_voices; }
    void shutdown();
    void tick();

    // Preload / cache management
    SoundHandle preload(const std::string& path);
    void unload(const SoundHandle& handle);

    // Fire-and-forget: load, play, auto-destroy on completion
    void play_one_shot(const std::string& path, float volume = 1.0f,
                       Priority priority = Priority::Gameplay,
                       AudioBus bus = AudioBus::SFX);

    void play_one_shot(const SoundHandle& handle, float volume = 1.0f,
                       Priority priority = Priority::Gameplay,
                       AudioBus bus = AudioBus::SFX);

    // Controllable playback: returns source id
    u64 play(const std::string& path, bool looping = false, float volume = 1.0f,
             Priority priority = Priority::Gameplay,
             AudioBus bus = AudioBus::SFX,
             bool stream = false);

    u64 play(const SoundHandle& handle, bool looping = false, float volume = 1.0f,
             Priority priority = Priority::Gameplay,
             AudioBus bus = AudioBus::SFX,
             bool stream = false);

    void stop(u64 source_id);
    void stop_all();
    void set_volume(u64 source_id, float volume);
    void set_looping(u64 source_id, bool looping);

    // Pause / Resume
    void pause(u64 source_id);
    void resume(u64 source_id);
    void pause_all();
    void resume_all();

    // State queries
    bool is_playing(u64 source_id) const;
    float get_volume(u64 source_id) const;
    bool is_looping(u64 source_id) const;

    // Master mute
    void mute();
    void unmute();
    bool is_muted() const;

    // Bus volume
    void set_bus_volume(AudioBus bus, float volume);
    float get_bus_volume(AudioBus bus) const;

    // Master control
    void set_master_volume(float volume);
    float master_volume() const;

    // Debug
    AudioDebugInfo debug_info() const;

    bool is_ready() const { return m_ready; }

private:
    struct Impl;
    Impl* m_impl = nullptr;
    bool m_ready = false;
    bool m_muted = false;
    f32 m_pre_mute_volume = 1.0f;
    u32 m_max_voices = 32;
    FileSystem* m_filesystem = nullptr;
    std::unordered_set<std::string> m_preloaded_paths;

    std::string normalize_path(const std::string& path) const;
};

} // namespace pino
