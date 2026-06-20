#include <miniaudio.h>
#include <ma_reverb_node.h>

#include "engine/audio/audio_manager.h"
#include "engine/platform/file_system.h"
#include "engine/core/log.h"
#include "engine/core/math_utils.h"

#include <memory>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cmath>

#include "engine/renderer/camera.h"

namespace pino {

struct AudioManager::Impl {
    struct SoundDeleter {
        void operator()(ma_sound* s) {
            if (s) {
                ma_sound_stop(s);
                ma_sound_uninit(s);
                delete s;
            }
        }
    };
    using SoundPtr = std::unique_ptr<ma_sound, SoundDeleter>;

    struct SoundEntry {
        SoundPtr sound;
        Priority priority = Priority::Gameplay;
    };

    ma_engine engine;
    u64 next_id = 1;
    std::unordered_map<u64, SoundEntry> active_sounds;
    std::vector<SoundEntry> one_shot_sounds;
    ma_sound_group group_sfx;
    ma_sound_group group_music;
    ma_sound_group group_voice;

    ma_sound_group* group_for_bus(AudioBus bus) {
        switch (bus) {
            case AudioBus::SFX:   return &group_sfx;
            case AudioBus::Music: return &group_music;
            case AudioBus::Voice: return &group_voice;
        }
        return nullptr;
    }

    // Spatial / zone state
    std::vector<AudioZone> zones;
    f32 zone_volume_scale = 1.0f;
    glm::vec3 listener_pos{0.0f};
    bool reverb_inited = false;

    // Reverb node for zone-based reverb (inserted on SFX bus)
    ma_reverb_node reverb_node;
    bool reverb_attached = false;

    void sweep_finished() {
        for (auto it = active_sounds.begin(); it != active_sounds.end(); ) {
            if (!ma_sound_is_playing(it->second.sound.get())) {
                it = active_sounds.erase(it);
            } else {
                ++it;
            }
        }

        for (auto it = one_shot_sounds.begin(); it != one_shot_sounds.end(); ) {
            if (!ma_sound_is_playing(it->sound.get())) {
                it = one_shot_sounds.erase(it);
            } else {
                ++it;
            }
        }
    }

    void ensure_voice_available(u32 max_voices) {
        sweep_finished();
        u32 total = static_cast<u32>(active_sounds.size() + one_shot_sounds.size());

        while (total >= max_voices) {
            int worst_prio = -1;
            int os_idx = -1;
            u64 active_id = 0;
            bool from_active = false;

            for (int i = 0; i < static_cast<int>(one_shot_sounds.size()); ++i) {
                int p = static_cast<int>(one_shot_sounds[i].priority);
                if (p > worst_prio) {
                    worst_prio = p;
                    os_idx = i;
                    from_active = false;
                }
            }
            for (auto& [id, entry] : active_sounds) {
                int p = static_cast<int>(entry.priority);
                if (p > worst_prio) {
                    worst_prio = p;
                    active_id = id;
                    from_active = true;
                }
            }

            if (from_active) {
                active_sounds.erase(active_id);
            } else if (os_idx >= 0) {
                one_shot_sounds.erase(one_shot_sounds.begin() + os_idx);
            } else {
                break;
            }

            total = static_cast<u32>(active_sounds.size() + one_shot_sounds.size());
        }
    }
};

AudioManager::AudioManager()
    : m_impl(new Impl)
{
}

AudioManager::~AudioManager() {
    shutdown();
    delete m_impl;
}

bool AudioManager::init(FileSystem& filesystem, u32 max_voices) {
    if (m_ready) {
        PINO_WARN("AudioManager: already initialized");
        return true;
    }

    m_filesystem = &filesystem;
    m_max_voices = max_voices;

    ma_engine_config config = ma_engine_config_init();
    if (ma_engine_init(&config, &m_impl->engine) != MA_SUCCESS) {
        PINO_ERROR("AudioManager: failed to initialize audio engine");
        m_filesystem = nullptr;
        return false;
    }

    // Create audio buses (children of master)
    if (ma_sound_group_init(&m_impl->engine, 0, nullptr, &m_impl->group_sfx) != MA_SUCCESS ||
        ma_sound_group_init(&m_impl->engine, 0, nullptr, &m_impl->group_music) != MA_SUCCESS ||
        ma_sound_group_init(&m_impl->engine, 0, nullptr, &m_impl->group_voice) != MA_SUCCESS) {
        PINO_ERROR("AudioManager: failed to create sound groups");
        ma_engine_uninit(&m_impl->engine);
        m_filesystem = nullptr;
        return false;
    }

    // Initialize reverb node (not yet inserted into the graph)
    {
        ma_reverb_node_config rcfg = ma_reverb_node_config_init(2, 44100);
        rcfg.wetVolume = 0.0f;
        rcfg.dryVolume = 1.0f;
        rcfg.roomSize  = 0.6f;
        rcfg.damping   = 0.3f;
        rcfg.width     = 1.0f;

        if (ma_reverb_node_init(ma_engine_get_node_graph(&m_impl->engine),
                                &rcfg, nullptr, &m_impl->reverb_node) == MA_SUCCESS) {
            m_impl->reverb_inited = true;
        }
    }

    m_ready = true;
    PINO_INFO("AudioManager initialized");
    return true;
}

void AudioManager::shutdown() {
    if (!m_ready) return;

    m_impl->active_sounds.clear();
    m_impl->one_shot_sounds.clear();
    m_impl->zones.clear();

    if (m_impl->reverb_inited) {
        ma_reverb_node_uninit(&m_impl->reverb_node, nullptr);
        m_impl->reverb_inited = false;
    }

    ma_sound_group_uninit(&m_impl->group_sfx);
    ma_sound_group_uninit(&m_impl->group_music);
    ma_sound_group_uninit(&m_impl->group_voice);

    ma_engine_uninit(&m_impl->engine);
    m_ready = false;
    m_filesystem = nullptr;
    PINO_INFO("AudioManager shut down");
}

std::string AudioManager::normalize_path(const std::string& path) const {
    return m_filesystem ? m_filesystem->resolve(path.c_str()) : path;
}

SoundHandle AudioManager::preload(const std::string& path) {
    if (!m_ready || !m_filesystem) return SoundHandle{};

    std::string resolved = normalize_path(path);

    if (m_preloaded_paths.count(resolved)) {
        return SoundHandle{resolved};
    }

    ma_resource_manager* rm = m_impl->engine.pResourceManager;
    if (ma_resource_manager_register_file(rm, resolved.c_str(),
                                          MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_DECODE) != MA_SUCCESS) {
        PINO_WARN("AudioManager::preload: failed to register '%s'", path.c_str());
        return SoundHandle{};
    }

    m_preloaded_paths.insert(resolved);
    return SoundHandle{std::move(resolved)};
}

void AudioManager::unload(const SoundHandle& handle) {
    if (!m_ready || !handle.is_valid()) return;

    auto it = m_preloaded_paths.find(handle.m_path);
    if (it == m_preloaded_paths.end()) return;

    ma_resource_manager* rm = m_impl->engine.pResourceManager;
    ma_resource_manager_unregister_file(rm, it->c_str());

    m_preloaded_paths.erase(it);
}

void AudioManager::tick() {
    if (!m_ready) return;
    m_impl->sweep_finished();

    // ---- Audio zones: find zones containing the listener ----
    float v_scale = 1.0f;
    for (const auto& zone : m_impl->zones) {
        if (!zone.active) continue;
        glm::vec3 d = m_impl->listener_pos - zone.center;
        float dist = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
        if (dist < zone.radius) {
            v_scale *= zone.volume_multiplier;
        }
    }
    m_impl->zone_volume_scale = v_scale;

    // Auto-sync listener from active camera
    if (m_active_camera) {
        const glm::vec3& pos = m_active_camera->position();
        const glm::vec3& target = m_active_camera->target();
        const glm::vec3 up(0.0f, 1.0f, 0.0f); // camera.h stores m_up but doesn't expose it

        glm::vec3 forward = target - pos;
        float len = forward.x * forward.x + forward.y * forward.y + forward.z * forward.z;
        if (len > 0.0001f) {
            float inv = 1.0f / std::sqrt(len);
            forward = { forward.x * inv, forward.y * inv, forward.z * inv };
        }

        set_listener_position(pos);
        ma_engine_listener_set_direction(&m_impl->engine, 0, forward.x, forward.y, forward.z);
        ma_engine_listener_set_world_up(&m_impl->engine, 0, up.x, up.y, up.z);
    }

    // Apply final volume: mute → zone → target
    float final_v = m_muted ? 0.0f : m_target_volume * v_scale;
    ma_engine_set_volume(&m_impl->engine, final_v);
}

void AudioManager::play_one_shot(const std::string& path, float volume,
                                  Priority priority, AudioBus bus)
{
    if (!m_ready || !m_filesystem) return;

    m_impl->ensure_voice_available(m_max_voices);

    std::string resolved = m_filesystem->resolve(path.c_str());

    Impl::SoundPtr sound(new ma_sound);
    ma_uint32 flags = 0; // one-shots are never streamed
    if (ma_sound_init_from_file(&m_impl->engine, resolved.c_str(), flags,
                                m_impl->group_for_bus(bus), nullptr, sound.get()) != MA_SUCCESS) {
        PINO_WARN("AudioManager: failed to load '%s'", path.c_str());
        return;
    }

    ma_sound_set_volume(sound.get(), Math::clamp(volume, 0.0f, 1.0f));
    ma_sound_start(sound.get());

    m_impl->one_shot_sounds.push_back({std::move(sound), priority});
}

void AudioManager::play_one_shot(const SoundHandle& handle, float volume,
                                  Priority priority, AudioBus bus)
{
    play_one_shot(handle.path(), volume, priority, bus);
}

u64 AudioManager::play(const std::string& path, bool looping, float volume,
                        Priority priority, AudioBus bus, bool stream)
{
    if (!m_ready || !m_filesystem) return 0;

    m_impl->ensure_voice_available(m_max_voices);

    std::string resolved = m_filesystem->resolve(path.c_str());

    Impl::SoundPtr sound(new ma_sound);
    ma_uint32 flags = stream ? MA_SOUND_FLAG_STREAM : 0;
    if (ma_sound_init_from_file(&m_impl->engine, resolved.c_str(), flags,
                                m_impl->group_for_bus(bus), nullptr, sound.get()) != MA_SUCCESS) {
        PINO_WARN("AudioManager: failed to load '%s'", path.c_str());
        return 0;
    }

    ma_sound_set_looping(sound.get(), looping ? MA_TRUE : MA_FALSE);
    ma_sound_set_volume(sound.get(), Math::clamp(volume, 0.0f, 1.0f));
    ma_sound_start(sound.get());

    u64 id = m_impl->next_id++;
    m_impl->active_sounds[id] = {std::move(sound), priority};
    return id;
}

u64 AudioManager::play(const SoundHandle& handle, bool looping, float volume,
                        Priority priority, AudioBus bus, bool stream)
{
    return play(handle.path(), looping, volume, priority, bus, stream);
}

void AudioManager::stop(u64 source_id) {
    if (!m_ready) return;

    auto it = m_impl->active_sounds.find(source_id);
    if (it == m_impl->active_sounds.end()) return;

    m_impl->active_sounds.erase(it);
}

void AudioManager::pause(u64 source_id) {
    if (!m_ready) return;

    auto it = m_impl->active_sounds.find(source_id);
    if (it == m_impl->active_sounds.end()) return;

    if (!ma_sound_is_playing(it->second.sound.get())) return;

    ma_sound_stop(it->second.sound.get());
}

void AudioManager::resume(u64 source_id) {
    if (!m_ready) return;

    auto it = m_impl->active_sounds.find(source_id);
    if (it == m_impl->active_sounds.end()) return;

    if (ma_sound_is_playing(it->second.sound.get())) return;

    if (ma_sound_at_end(it->second.sound.get())) return;

    ma_sound_start(it->second.sound.get());
}

bool AudioManager::is_playing(u64 source_id) const {
    if (!m_ready) return false;

    auto it = m_impl->active_sounds.find(source_id);
    if (it == m_impl->active_sounds.end()) return false;

    return ma_sound_is_playing(it->second.sound.get()) != MA_FALSE;
}

float AudioManager::get_volume(u64 source_id) const {
    if (!m_ready) return 0.0f;

    auto it = m_impl->active_sounds.find(source_id);
    if (it == m_impl->active_sounds.end()) return 0.0f;

    return ma_sound_get_volume(it->second.sound.get());
}

bool AudioManager::is_looping(u64 source_id) const {
    if (!m_ready) return false;

    auto it = m_impl->active_sounds.find(source_id);
    if (it == m_impl->active_sounds.end()) return false;

    return ma_sound_is_looping(it->second.sound.get()) != MA_FALSE;
}

void AudioManager::stop_all() {
    if (!m_ready) return;

    m_impl->active_sounds.clear();
    m_impl->one_shot_sounds.clear();
}

void AudioManager::set_volume(u64 source_id, float volume) {
    if (!m_ready) return;

    auto it = m_impl->active_sounds.find(source_id);
    if (it == m_impl->active_sounds.end()) return;

    ma_sound_set_volume(it->second.sound.get(), Math::clamp(volume, 0.0f, 1.0f));
}

void AudioManager::set_looping(u64 source_id, bool looping) {
    if (!m_ready) return;

    auto it = m_impl->active_sounds.find(source_id);
    if (it == m_impl->active_sounds.end()) return;

    ma_sound_set_looping(it->second.sound.get(), looping ? MA_TRUE : MA_FALSE);
}

void AudioManager::set_master_volume(float volume) {
    m_target_volume = Math::clamp(volume, 0.0f, 1.0f);
}

float AudioManager::master_volume() const {
    return m_target_volume;
}

void AudioManager::mute() {
    m_muted = true;
}

void AudioManager::unmute() {
    m_muted = false;
}

bool AudioManager::is_muted() const {
    return m_muted;
}

void AudioManager::set_bus_volume(AudioBus bus, float volume) {
    if (!m_ready) return;

    ma_sound_group* group = m_impl->group_for_bus(bus);
    if (group) {
        ma_sound_group_set_volume(group, Math::clamp(volume, 0.0f, 1.0f));
    }
}

float AudioManager::get_bus_volume(AudioBus bus) const {
    if (!m_ready) return 0.0f;

    ma_sound_group* group = m_impl->group_for_bus(bus);
    if (group) {
        return ma_sound_group_get_volume(group);
    }
    return 0.0f;
}

void AudioManager::pause_all() {
    if (!m_ready) return;

    for (auto& [id, entry] : m_impl->active_sounds) {
        if (ma_sound_is_playing(entry.sound.get())) {
            ma_sound_stop(entry.sound.get());
        }
    }
}

void AudioManager::resume_all() {
    if (!m_ready) return;

    for (auto& [id, entry] : m_impl->active_sounds) {
        if (!ma_sound_is_playing(entry.sound.get()) && !ma_sound_at_end(entry.sound.get())) {
            ma_sound_start(entry.sound.get());
        }
    }
}

AudioDebugInfo AudioManager::debug_info() const {
    AudioDebugInfo info{};
    if (!m_ready) return info;

    info.active_sounds = static_cast<u32>(m_impl->active_sounds.size());
    info.one_shot_sounds = static_cast<u32>(m_impl->one_shot_sounds.size());
    info.total_sounds = info.active_sounds + info.one_shot_sounds;
    info.max_voices = m_max_voices;
    info.master_volume = m_target_volume;
    info.active_zones = static_cast<u32>(m_impl->zones.size());
    info.zone_volume_scale = m_impl->zone_volume_scale;
    return info;
}

// ======================================================================
// Spatial audio: listener
// ======================================================================

void AudioManager::set_listener_position(const glm::vec3& pos) {
    if (!m_ready) return;
    m_impl->listener_pos = pos;
    ma_engine_listener_set_position(&m_impl->engine, 0, pos.x, pos.y, pos.z);
}

void AudioManager::set_listener_velocity(const glm::vec3& vel) {
    if (!m_ready) return;
    ma_engine_listener_set_velocity(&m_impl->engine, 0, vel.x, vel.y, vel.z);
}

void AudioManager::set_listener_orientation(const glm::vec3& forward, const glm::vec3& up) {
    if (!m_ready) return;
    ma_engine_listener_set_direction(&m_impl->engine, 0, forward.x, forward.y, forward.z);
    ma_engine_listener_set_world_up(&m_impl->engine, 0, up.x, up.y, up.z);
}

glm::vec3 AudioManager::listener_position() const {
    return m_impl->listener_pos;
}

// ======================================================================
// Spatial audio: 3D playback
// ======================================================================

static ma_sound* create_3d_sound(
    ma_engine* engine, ma_sound_group* group, const char* path,
    const glm::vec3& position, bool looping, bool stream)
{
    ma_sound* sound = new ma_sound;
    ma_uint32 flags = stream ? MA_SOUND_FLAG_STREAM : 0;
    if (ma_sound_init_from_file(engine, path, flags, group, nullptr, sound) != MA_SUCCESS) {
        delete sound;
        return nullptr;
    }

    ma_sound_set_position(sound, position.x, position.y, position.z);
    ma_sound_set_looping(sound, looping ? MA_TRUE : MA_FALSE);
    ma_sound_set_attenuation_model(sound, ma_attenuation_model_inverse);

    return sound;
}

u64 AudioManager::play_3d(const std::string& path, const glm::vec3& position,
                           bool looping, float volume,
                           Priority priority, AudioBus bus, bool stream)
{
    if (!m_ready || !m_filesystem) return 0;

    m_impl->ensure_voice_available(m_max_voices);

    std::string resolved = m_filesystem->resolve(path.c_str());
    ma_sound* raw = create_3d_sound(&m_impl->engine, m_impl->group_for_bus(bus),
                                     resolved.c_str(), position, looping, stream);
    if (!raw) {
        PINO_WARN("AudioManager::play_3d: failed to load '%s'", path.c_str());
        return 0;
    }

    Impl::SoundPtr sound(raw);
    ma_sound_set_volume(sound.get(), Math::clamp(volume, 0.0f, 1.0f));
    ma_sound_start(sound.get());

    u64 id = m_impl->next_id++;
    m_impl->active_sounds[id] = {std::move(sound), priority};
    return id;
}

u64 AudioManager::play_3d(const SoundHandle& handle, const glm::vec3& position,
                           bool looping, float volume,
                           Priority priority, AudioBus bus, bool stream)
{
    return play_3d(handle.path(), position, looping, volume, priority, bus, stream);
}

void AudioManager::play_one_shot_3d(const std::string& path, const glm::vec3& position,
                                     float volume, Priority priority, AudioBus bus)
{
    if (!m_ready || !m_filesystem) return;

    m_impl->ensure_voice_available(m_max_voices);

    std::string resolved = m_filesystem->resolve(path.c_str());
    ma_sound* raw = create_3d_sound(&m_impl->engine, m_impl->group_for_bus(bus),
                                     resolved.c_str(), position, false, false);
    if (!raw) {
        PINO_WARN("AudioManager::play_one_shot_3d: failed to load '%s'", path.c_str());
        return;
    }

    Impl::SoundPtr sound(raw);
    ma_sound_set_volume(sound.get(), Math::clamp(volume, 0.0f, 1.0f));
    ma_sound_start(sound.get());

    m_impl->one_shot_sounds.push_back({std::move(sound), priority});
}

void AudioManager::play_one_shot_3d(const SoundHandle& handle, const glm::vec3& position,
                                     float volume, Priority priority, AudioBus bus)
{
    play_one_shot_3d(handle.path(), position, volume, priority, bus);
}

// ======================================================================
// Spatial audio: per-sound control
// ======================================================================

void AudioManager::set_position(u64 source_id, const glm::vec3& pos) {
    if (!m_ready) return;

    auto it = m_impl->active_sounds.find(source_id);
    if (it == m_impl->active_sounds.end()) return;

    ma_sound_set_position(it->second.sound.get(), pos.x, pos.y, pos.z);
}

void AudioManager::set_velocity(u64 source_id, const glm::vec3& vel) {
    if (!m_ready) return;

    auto it = m_impl->active_sounds.find(source_id);
    if (it == m_impl->active_sounds.end()) return;

    ma_sound_set_velocity(it->second.sound.get(), vel.x, vel.y, vel.z);
}

void AudioManager::set_attenuation_model(u64 source_id, AttenuationModel model) {
    if (!m_ready) return;

    auto it = m_impl->active_sounds.find(source_id);
    if (it == m_impl->active_sounds.end()) return;

    static const ma_attenuation_model table[] = {
        ma_attenuation_model_none,          // None
        ma_attenuation_model_inverse,       // Inverse
        ma_attenuation_model_linear,        // Linear
        ma_attenuation_model_exponential    // Exponential
    };
    ma_sound_set_attenuation_model(it->second.sound.get(),
                                    table[static_cast<u8>(model)]);
}

void AudioManager::set_attenuation_params(u64 source_id, float min_distance,
                                           float max_distance, float rolloff) {
    if (!m_ready) return;

    auto it = m_impl->active_sounds.find(source_id);
    if (it == m_impl->active_sounds.end()) return;

    ma_sound_set_min_distance(it->second.sound.get(), min_distance);
    ma_sound_set_max_distance(it->second.sound.get(), max_distance);
    ma_sound_set_rolloff(it->second.sound.get(), rolloff);
}

void AudioManager::set_doppler_factor(u64 source_id, float factor) {
    if (!m_ready) return;

    auto it = m_impl->active_sounds.find(source_id);
    if (it == m_impl->active_sounds.end()) return;

    ma_sound_set_doppler_factor(it->second.sound.get(), factor);
}

// ======================================================================
// Audio zones
// ======================================================================

u32 AudioManager::create_zone(const AudioZone& zone) {
    if (!m_ready) return UINT32_MAX;

    m_impl->zones.push_back(zone);
    return static_cast<u32>(m_impl->zones.size() - 1);
}

void AudioManager::update_zone(u32 zone_id, const AudioZone& zone) {
    if (!m_ready) return;
    if (zone_id >= m_impl->zones.size()) return;

    m_impl->zones[zone_id] = zone;
}

void AudioManager::destroy_zone(u32 zone_id) {
    if (!m_ready) return;
    if (zone_id >= m_impl->zones.size()) return;

    // Swap-and-pop to keep O(1)
    m_impl->zones[zone_id] = m_impl->zones.back();
    m_impl->zones.pop_back();
}

void AudioManager::set_active_camera(Camera* cam) {
    m_active_camera = cam;
}

} // namespace pino
