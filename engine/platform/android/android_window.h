#pragma once
#include "engine/platform/window.h"

#if defined(__ANDROID__)
#include <android/native_window.h>
#include <EGL/egl.h>
#else
struct ANativeWindow;
using EGLDisplay = void*;
using EGLSurface = void*;
using EGLContext = void*;
using EGLConfig  = void*;
#define EGL_NO_DISPLAY  ((EGLDisplay)0)
#define EGL_NO_CONTEXT  ((EGLContext)0)
#define EGL_NO_SURFACE  ((EGLSurface)0)
#endif

namespace pino {

class AndroidWindow final : public Window {
public:
    explicit AndroidWindow(const WindowConfig& config);
    ~AndroidWindow() override;

    void poll_events() override;
    void swap_buffers() override;
    bool should_close() const override;

    u32  width()  const override;
    u32  height() const override;
    f32  aspect() const override;

    void* native_handle() const override;
    void* gl_context()    const override;

    // Called on APP_CMD_INIT_WINDOW (window surface available)
    void recreate(ANativeWindow* win);

    // Called on APP_CMD_TERM_WINDOW — preserves EGL context, only
    // destroys the surface so we don't need full resource re-upload.
    // On some devices the context may still be lost; check context_valid().
    void destroy_surface();

    // Returns true if the EGL context survived the last destroy_surface().
    // Call after recreate() + init_egl() to check.
    bool context_valid() const { return m_context_valid; }

    // Returns true if the EGL context was destroyed and recreated during
    // the last recreate() call (i.e. GPU resources must be re-uploaded).
    // Only meaningful once per lifecycle transition; call this after
    // recreate() to decide whether to invoke on_context_lost/restored.
    bool needs_context_restore() const {
        bool v = m_needs_context_restore;
        const_cast<AndroidWindow*>(this)->m_needs_context_restore = false;
        return v;
    }

private:
    bool init_egl(EGLContext share_ctx = EGL_NO_CONTEXT);
    void destroy_egl();

    ANativeWindow* m_window  = nullptr;
    EGLDisplay     m_display = EGL_NO_DISPLAY;
    EGLSurface     m_surface = EGL_NO_SURFACE;
    EGLContext     m_context = EGL_NO_CONTEXT;
    EGLConfig      m_egl_config{};
    WindowConfig   m_config;
    bool           m_initialized             = false;
    bool           m_context_valid           = false;
    bool           m_needs_context_restore   = false;
};

} // namespace pino
