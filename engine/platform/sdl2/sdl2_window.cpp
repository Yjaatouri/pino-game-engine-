#include "sdl2_window.h"
#include "engine/core/log.h"

#include <SDL.h>

namespace pino {

Sdl2Window::Sdl2Window(const WindowConfig& config)
    : m_config(config)
{
    if (config.gl_es) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    } else {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, static_cast<int>(config.gl_major));
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, static_cast<int>(config.gl_minor));
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,  1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,    24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE,   8);

    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
    if (config.fullscreen) flags |= SDL_WINDOW_FULLSCREEN;
    if (config.resizable)  flags |= SDL_WINDOW_RESIZABLE;

    m_window = SDL_CreateWindow(
        config.title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        static_cast<int>(config.width),
        static_cast<int>(config.height),
        flags
    );
    if (!m_window) {
        PINO_ERROR("SDL_CreateWindow failed: %s", SDL_GetError());
        return;
    }

    m_context = SDL_GL_CreateContext(m_window);
    if (!m_context) {
        PINO_ERROR("SDL_GL_CreateContext failed: %s", SDL_GetError());
        return;
    }
    SDL_GL_MakeCurrent(m_window, m_context);
    SDL_GL_SetSwapInterval(1);

    PINO_INFO("Window created: %ux%u (%s)", config.width, config.height,
              config.gl_es ? "OpenGL ES" : "OpenGL Core");
}

Sdl2Window::~Sdl2Window() {
    if (m_context) SDL_GL_DeleteContext(m_context);
    if (m_window)  SDL_DestroyWindow(m_window);
}

void Sdl2Window::poll_events() {
    // Event queue is drained by the InputSystem — Window just provides
    // the SDL event pump integration. The actual polling happens in
    // Engine::begin_frame() through SDL_PollEvent.
}

void Sdl2Window::swap_buffers() {
    SDL_GL_SwapWindow(m_window);
}

bool Sdl2Window::should_close() const {
    return false; // Handled via Input quit_requested
}

u32 Sdl2Window::width() const {
    int w = 0;
    SDL_GetWindowSize(m_window, &w, nullptr);
    return static_cast<u32>(w);
}

u32 Sdl2Window::height() const {
    int h = 0;
    SDL_GetWindowSize(m_window, nullptr, &h);
    return static_cast<u32>(h);
}

f32 Sdl2Window::aspect() const {
    return static_cast<f32>(width()) / static_cast<f32>(height());
}

void* Sdl2Window::native_handle() const {
    return m_window;
}

void* Sdl2Window::gl_context() const {
    return m_context;
}

} // namespace pino
