#pragma once

#include "engine/core/types.h"

namespace pino {

class IGame {
public:
    virtual ~IGame() = default;

    // Called once after engine init. Return false to abort.
    virtual bool init() = 0;

    // Called at fixed rate (fixed_update_rate Hz). Use for physics / logic.
    virtual void update(f32 dt) = 0;

    // Called every frame with variable dt. Use for rendering.
    virtual void render(f32 dt) = 0;

    // Called once before engine shutdown.
    virtual void shutdown() = 0;

    // Called when the GL context is lost (e.g. Android backgrounding
    // on devices that do not preserve EGL context). Game should
    // invalidate all GPU resources (shaders, textures, VBOs, FBOs).
    virtual void on_context_lost() {}

    // Called after GL context has been restored. Game should
    // re-upload all GPU resources.
    virtual void on_context_restored() {}
};

} // namespace pino
