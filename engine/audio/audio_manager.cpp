#include <miniaudio.h>

#include "engine/audio/audio_manager.h"
#include "engine/platform/file_system.h"
#include "engine/core/log.h"

#include <unordered_map>

namespace pino {

struct AudioManager::Impl {
    ma_engine engine;
    u64 next_id = 1;
    std::unordered_map<u64, ma_sound*> active_sounds;
};

AudioManager::AudioManager()
    : m_impl(new Impl)
{
}

AudioManager::~AudioManager() {
    shutdown();
    delete m_impl;
}

bool AudioManager::init(FileSystem& filesystem) {
    m_filesystem = &filesystem;

    ma_engine_config config = ma_engine_config_init();
    if (ma_engine_init(&config, &m_impl->engine) != MA_SUCCESS) {
        PINO_ERROR("AudioManager: failed to initialize audio engine");
        return false;
    }

    m_ready = true;
    PINO_INFO("AudioManager initialized");
    return true;
}

void AudioManager::shutdown() {
    if (!m_ready) return;

    for (auto& [id, sound] : m_impl->active_sounds) {
        if (sound) {
            ma_sound_stop(sound);
            ma_sound_uninit(sound);
            delete sound;
        }
    }
    m_impl->active_sounds.clear();

    ma_engine_uninit(&m_impl->engine);
    m_ready = false;
    m_filesystem = nullptr;
    PINO_INFO("AudioManager shut down");
}

void AudioManager::play_one_shot(const std::string& path, float volume) {
    (void)volume;
    if (!m_ready || !m_filesystem) return;

    std::string resolved = m_filesystem->resolve(path.c_str());

    if (ma_engine_play_sound(&m_impl->engine, resolved.c_str(), nullptr) != MA_SUCCESS) {
        PINO_WARN("AudioManager: failed to play '%s'", path.c_str());
    }
}

u64 AudioManager::play(const std::string& path, bool looping, float volume) {
    if (!m_ready || !m_filesystem) return 0;

    std::string resolved = m_filesystem->resolve(path.c_str());

    ma_sound* sound = new ma_sound;
    if (ma_sound_init_from_file(&m_impl->engine, resolved.c_str(), 0, nullptr, nullptr, sound) != MA_SUCCESS) {
        PINO_WARN("AudioManager: failed to load '%s'", path.c_str());
        delete sound;
        return 0;
    }

    ma_sound_set_looping(sound, looping ? MA_TRUE : MA_FALSE);
    ma_sound_set_volume(sound, volume);
    ma_sound_start(sound);

    u64 id = m_impl->next_id++;
    m_impl->active_sounds[id] = sound;
    return id;
}

void AudioManager::stop(u64 source_id) {
    if (!m_ready) return;

    auto it = m_impl->active_sounds.find(source_id);
    if (it == m_impl->active_sounds.end()) return;

    ma_sound_stop(it->second);
    ma_sound_uninit(it->second);
    delete it->second;
    m_impl->active_sounds.erase(it);
}

void AudioManager::set_volume(u64 source_id, float volume) {
    if (!m_ready) return;

    auto it = m_impl->active_sounds.find(source_id);
    if (it == m_impl->active_sounds.end()) return;

    ma_sound_set_volume(it->second, volume);
}

void AudioManager::set_looping(u64 source_id, bool looping) {
    if (!m_ready) return;

    auto it = m_impl->active_sounds.find(source_id);
    if (it == m_impl->active_sounds.end()) return;

    ma_sound_set_looping(it->second, looping ? MA_TRUE : MA_FALSE);
}

void AudioManager::set_master_volume(float volume) {
    if (!m_ready) return;
    ma_engine_set_volume(&m_impl->engine, volume);
}

float AudioManager::master_volume() const {
    if (!m_ready) return 0.0f;
    return ma_engine_get_volume(&m_impl->engine);
}

} // namespace pino
