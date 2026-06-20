#pragma once

#include "engine/core/types.h"

namespace pino {

class IScene {
public:
    virtual ~IScene() = default;

    // Called once after the scene is pushed (but after on_enter).
    virtual void init() = 0;

    // Called at the engine's fixed update rate.
    virtual void update(f32 dt) = 0;

    // Called every frame with variable dt.
    virtual void render(f32 dt) = 0;

    // Called before the scene is popped / replaced.
    virtual void shutdown() = 0;

    // Lifecycle hooks — called automatically by SceneManager.
    virtual void on_enter(IScene* previous) { (void)previous; }
    virtual void on_exit(IScene* next)      { (void)next; }
    virtual void on_pause()                 {}
    virtual void on_resume()                {}
};

} // namespace pino
