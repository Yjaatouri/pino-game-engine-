#pragma once

#include "engine/core/types.h"
#include <string>
#include <unordered_set>
#include <glm/glm.hpp>

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

enum class AttenuationModel : u8 {
    None,
    Inverse,
    Linear,
    Exponential
};

struct AudioZone {
    glm::vec3 center;
    float radius = 10.0f;
    float volume_multiplier = 1.0f;
    bool active = true;
};

struct AudioDebugInfo {
    u32 active_sounds;
    u32 one_shot_sounds;
    u32 total_sounds;
    u32 max_voices;
    f32 master_volume;
    u32 active_zones;
    f32 zone_volume_scale;
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

    // ---- Spatial audio: listener ----
    void set_listener_position(const glm::vec3& pos);
    void set_listener_velocity(const glm::vec3& vel);
    void set_listener_orientation(const glm::vec3& forward, const glm::vec3& up);
    glm::vec3 listener_position() const;

    // ---- Spatial audio: 3D playback ----
    u64 play_3d(const std::string& path, const glm::vec3& position,
                bool looping = false, float volume = 1.0f,
                Priority priority = Priority::Gameplay,
                AudioBus bus = AudioBus::SFX,
                bool stream = false);

    u64 play_3d(const SoundHandle& handle, const glm::vec3& position,
                bool looping = false, float volume = 1.0f,
                Priority priority = Priority::Gameplay,
                AudioBus bus = AudioBus::SFX,
                bool stream = false);

    void play_one_shot_3d(const std::string& path, const glm::vec3& position,
                          float volume = 1.0f,
                          Priority priority = Priority::Gameplay,
                          AudioBus bus = AudioBus::SFX);

    void play_one_shot_3d(const SoundHandle& handle, const glm::vec3& position,
                          float volume = 1.0f,
                          Priority priority = Priority::Gameplay,
                          AudioBus bus = AudioBus::SFX);

    // ---- Spatial audio: per-sound control ----
    void set_position(u64 source_id, const glm::vec3& pos);
    void set_velocity(u64 source_id, const glm::vec3& vel);
    void set_attenuation_model(u64 source_id, AttenuationModel model);
    void set_attenuation_params(u64 source_id, float min_distance, float max_distance, float rolloff);
    void set_doppler_factor(u64 source_id, float factor);

    // ---- Audio zones ----
    u32 create_zone(const AudioZone& zone);
    void update_zone(u32 zone_id, const AudioZone& zone);
    void destroy_zone(u32 zone_id);

    // When set, listener is auto-synced from the camera each frame in tick()
    void set_active_camera(class Camera* cam);

    // Debug
    AudioDebugInfo debug_info() const;

    bool is_ready() const { return m_ready; }

private:
    struct Impl;
    Impl* m_impl = nullptr;
    bool m_ready = false;
    bool m_muted = false;
    f32 m_target_volume = 1.0f;
    u32 m_max_voices = 32;
    FileSystem* m_filesystem = nullptr;
    std::unordered_set<std::string> m_preloaded_paths;
    class Camera* m_active_camera = nullptr;

    std::string normalize_path(const std::string& path) const;
};

} // namespace pino
