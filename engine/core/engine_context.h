#pragma once

#include "engine/core/types.h"

namespace pino {

class Window;
class Input;
class FileSystem;
class AudioManager;
class AssetManager;
class TimerManager;
struct EngineConfig;
struct EngineStats;

struct EngineContext {
    Window&         window;
    Input&          input;
    FileSystem&     filesystem;
    AudioManager&   audio;
    AssetManager&   assets;
    TimerManager&   timers;
    EngineStats&    stats;
    const EngineConfig& config;

    static EngineContext& instance();
    static bool is_valid();
    static void create(Window& w, Input& i, FileSystem& fs,
                       AudioManager& a, AssetManager& am,
                       TimerManager& t, EngineStats& s,
                       const EngineConfig& c);
    static void destroy();

private:
    EngineContext(Window& w, Input& i, FileSystem& fs,
                  AudioManager& a, AssetManager& am,
                  TimerManager& t, EngineStats& s,
                  const EngineConfig& c);

    static EngineContext* s_instance;
};

}
