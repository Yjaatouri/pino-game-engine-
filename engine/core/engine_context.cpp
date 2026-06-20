#include "engine/core/engine_context.h"
#include "engine/core/log.h"
#include "engine/core/validate.h"

namespace pino {

EngineContext* EngineContext::s_instance = nullptr;

EngineContext::EngineContext(Window& w, Input& i, FileSystem& fs,
                             AudioManager& a, AssetManager& am,
                             TimerManager& t, EngineStats& s,
                             const EngineConfig& c)
    : window(w)
    , input(i)
    , filesystem(fs)
    , audio(a)
    , assets(am)
    , timers(t)
    , stats(s)
    , config(c)
{
}

EngineContext& EngineContext::instance() {
    PINO_REQUIRE(s_instance != nullptr, "EngineContext not initialized");
    return *s_instance;
}

bool EngineContext::is_valid() {
    return s_instance != nullptr;
}

void EngineContext::create(Window& w, Input& i, FileSystem& fs,
                           AudioManager& a, AssetManager& am,
                           TimerManager& t, EngineStats& s,
                           const EngineConfig& c) {
    PINO_REQUIRE(s_instance == nullptr, "EngineContext already initialized");
    s_instance = new EngineContext(w, i, fs, a, am, t, s, c);
}

void EngineContext::destroy() {
    PINO_REQUIRE(s_instance != nullptr, "EngineContext not initialized");
    delete s_instance;
    s_instance = nullptr;
}

}
