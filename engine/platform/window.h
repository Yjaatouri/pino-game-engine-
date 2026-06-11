#pragma once

#include "engine/core/types.h"
#include <memory>

namespace pino {

struct WindowConfig {
    const char* title       = "Pino Engine";
    u32         width       = 1280;
    u32         height      = 720;
    bool        fullscreen  = false;
    bool        resizable   = false;
    u32         gl_major    = 3;
    u32         gl_minor    = 0;
    bool        gl_es       = true;
    void*       existing_native_window = nullptr;
};

class Window {
public:
    virtual ~Window() = default;

    virtual void poll_events() = 0;
    virtual void swap_buffers() = 0;
    virtual bool should_close() const = 0;

    virtual u32  width()  const = 0;
    virtual u32  height() const = 0;
    virtual f32  aspect() const = 0;

    // Opaque platform handles — needed by the renderer
    virtual void* native_handle() const = 0;
    virtual void* gl_context()    const = 0;
};

} // namespace pino
