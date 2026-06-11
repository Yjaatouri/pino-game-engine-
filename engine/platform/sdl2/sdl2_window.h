#pragma once

#include "engine/platform/window.h"

struct SDL_Window;
typedef void* SDL_GLContext;

namespace pino {

class Sdl2Window final : public Window {
public:
    explicit Sdl2Window(const WindowConfig& config);
    ~Sdl2Window() override;

    Sdl2Window(const Sdl2Window&) = delete;
    Sdl2Window& operator=(const Sdl2Window&) = delete;

    void poll_events() override;
    void swap_buffers() override;
    bool should_close() const override;

    u32  width()  const override;
    u32  height() const override;
    f32  aspect() const override;

    void* native_handle() const override;
    void* gl_context()    const override;

private:
    SDL_Window*   m_window  = nullptr;
    SDL_GLContext m_context = nullptr;
    WindowConfig  m_config;
};

} // namespace pino
