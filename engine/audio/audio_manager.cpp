#include <miniaudio.h>

#include "engine/audio/audio_manager.h"
#include "engine/platform/file_system.h"
#include "engine/core/log.h"
#include "engine/core/math_utils.h"

#include <memory>
#include <unordered_map>
#include <vector>

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

    m_ready = true;
    PINO_INFO("AudioManager initialized");
    return true;
}

void AudioManager::shutdown() {
    if (!m_ready) return;

    m_impl->active_sounds.clear();
    m_impl->one_shot_sounds.clear();

    ma_sound_group_uninit(&m_impl->group_sfx);
    ma_sound_group_uninit(&m_impl->group_music);
    ma_sound_group_uninit(&m_impl->group_voice);

    ma_engine_uninit(&m_impl->engine);
    m_ready = false;
    m_filesystem = nullptr;
    PINO_INFO("AudioManager shut down");
}

void AudioManager::tick() {
    if (!m_ready) return;
    m_impl->sweep_finished();
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
    if (!m_ready) return;
    ma_engine_set_volume(&m_impl->engine, Math::clamp(volume, 0.0f, 1.0f));
}

float AudioManager::master_volume() const {
    if (!m_ready) return 0.0f;
    return ma_engine_get_volume(&m_impl->engine);
}

void AudioManager::mute() {
    if (!m_ready) return;
    if (m_muted) return;

    m_muted = true;
    m_pre_mute_volume = ma_engine_get_volume(&m_impl->engine);
    ma_engine_set_volume(&m_impl->engine, 0.0f);
}

void AudioManager::unmute() {
    if (!m_ready) return;
    if (!m_muted) return;

    m_muted = false;
    ma_engine_set_volume(&m_impl->engine, m_pre_mute_volume);
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
    info.master_volume = ma_engine_get_volume(&m_impl->engine);
    return info;
}

} // namespace pino
