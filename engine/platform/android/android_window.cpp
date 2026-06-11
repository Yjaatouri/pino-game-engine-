#include "android_window.h"
#include "engine/core/log.h"
#include <android/api-level.h>

namespace pino {

static constexpr EGLint EGL_ATTRIBS[] = {
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
    EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
    EGL_RED_SIZE,        8,
    EGL_GREEN_SIZE,      8,
    EGL_BLUE_SIZE,       8,
    EGL_ALPHA_SIZE,      8,
    EGL_DEPTH_SIZE,      24,
    EGL_STENCIL_SIZE,    8,
    EGL_NONE
};

static constexpr EGLint CONTEXT_ATTRIBS[] = {
    EGL_CONTEXT_CLIENT_VERSION, 3,
    EGL_NONE
};

AndroidWindow::AndroidWindow(const WindowConfig& config)
    : m_config(config)
{
    m_window = static_cast<ANativeWindow*>(config.existing_native_window);
    if (m_window) {
        init_egl();
    }
}

AndroidWindow::~AndroidWindow() {
    destroy_egl();
}

void AndroidWindow::recreate(ANativeWindow* win) {
    m_window = win;

    if (m_initialized && m_surface != EGL_NO_SURFACE) {
        // Context exists, just need a new surface
        if (!m_window) return;

        EGLSurface new_surface = eglCreateWindowSurface(m_display, m_egl_config, m_window, nullptr);
        if (new_surface == EGL_NO_SURFACE) {
            PINO_ERROR("eglCreateWindowSurface failed after recreate: 0x%x", eglGetError());
            // Context may be lost — fall back to full re-init
            destroy_egl();
            init_egl();
            m_needs_context_restore = true;
            return;
        }

        // Replace old surface
        eglDestroySurface(m_display, m_surface);
        m_surface = new_surface;

        if (!eglMakeCurrent(m_display, m_surface, m_surface, m_context)) {
            PINO_WARN("eglMakeCurrent with saved context failed — context lost, full reinit");
            destroy_egl();
            init_egl();
            m_needs_context_restore = true;
            return;
        }

        m_context_valid = true;
        PINO_INFO("Android EGL surface recreated (context preserved)");
    } else {
        // First-time init or full rebuild after lost context
        m_context_valid = false;
        bool was_initialized = m_initialized;
        if (m_initialized) destroy_egl();
        if (m_window) {
            init_egl();
            if (was_initialized) m_needs_context_restore = true;
        }
    }
}

void AndroidWindow::destroy_surface() {
    if (!m_initialized) return;

    // Detach context, destroy surface, keep context + display
    eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    if (m_surface != EGL_NO_SURFACE) {
        eglDestroySurface(m_display, m_surface);
        m_surface = EGL_NO_SURFACE;
    }

    m_context_valid = false;
    PINO_INFO("Android EGL surface destroyed (context preserved)");
}

bool AndroidWindow::init_egl(EGLContext share_ctx) {
    m_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (m_display == EGL_NO_DISPLAY) {
        PINO_ERROR("eglGetDisplay failed");
        return false;
    }

    EGLint major, minor;
    if (!eglInitialize(m_display, &major, &minor)) {
        PINO_ERROR("eglInitialize failed");
        m_display = EGL_NO_DISPLAY;
        return false;
    }

    EGLint num_configs = 0;
    if (!eglChooseConfig(m_display, EGL_ATTRIBS, &m_egl_config, 1, &num_configs) || num_configs == 0) {
        PINO_ERROR("eglChooseConfig failed (no matching config)");
        eglTerminate(m_display);
        m_display = EGL_NO_DISPLAY;
        return false;
    }

    m_surface = eglCreateWindowSurface(m_display, m_egl_config, m_window, nullptr);
    if (m_surface == EGL_NO_SURFACE) {
        PINO_ERROR("eglCreateWindowSurface failed: 0x%x", eglGetError());
        eglTerminate(m_display);
        m_display = EGL_NO_DISPLAY;
        return false;
    }

    m_context = eglCreateContext(m_display, m_egl_config, share_ctx, CONTEXT_ATTRIBS);
    if (m_context == EGL_NO_CONTEXT) {
        PINO_ERROR("eglCreateContext failed: 0x%x", eglGetError());
        eglDestroySurface(m_display, m_surface);
        m_surface = EGL_NO_SURFACE;
        eglTerminate(m_display);
        m_display = EGL_NO_DISPLAY;
        return false;
    }

    if (!eglMakeCurrent(m_display, m_surface, m_surface, m_context)) {
        PINO_ERROR("eglMakeCurrent failed");
        destroy_egl();
        return false;
    }

    int api_level = android_get_device_api_level();
    PINO_INFO("Android EGL initialized (EGL %d.%d, API level %d)", major, minor, api_level);
    m_initialized   = true;
    m_context_valid = true;
    return true;
}

void AndroidWindow::destroy_egl() {
    if (!m_initialized) return;

    eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    if (m_context != EGL_NO_CONTEXT) {
        eglDestroyContext(m_display, m_context);
        m_context = EGL_NO_CONTEXT;
    }
    if (m_surface != EGL_NO_SURFACE) {
        eglDestroySurface(m_display, m_surface);
        m_surface = EGL_NO_SURFACE;
    }
    if (m_display != EGL_NO_DISPLAY) {
        eglTerminate(m_display);
        m_display = EGL_NO_DISPLAY;
    }
    m_initialized   = false;
    m_context_valid = false;
}

void AndroidWindow::poll_events() {
    // Events are polled externally via the NativeActivity looper
}

void AndroidWindow::swap_buffers() {
    if (m_initialized) {
        EGLBoolean ok = eglSwapBuffers(m_display, m_surface);
        if (!ok) {
            EGLint err = eglGetError();
            if (err == EGL_BAD_SURFACE || err == EGL_BAD_CONTEXT || err == EGL_BAD_DISPLAY) {
                m_context_valid = false;
            }
        }
    }
}

bool AndroidWindow::should_close() const {
    return false;
}

u32 AndroidWindow::width() const {
    if (!m_window) return 0;
    return static_cast<u32>(ANativeWindow_getWidth(m_window));
}

u32 AndroidWindow::height() const {
    if (!m_window) return 0;
    return static_cast<u32>(ANativeWindow_getHeight(m_window));
}

f32 AndroidWindow::aspect() const {
    return static_cast<f32>(width()) / static_cast<f32>(height());
}

void* AndroidWindow::native_handle() const {
    return m_window;
}

void* AndroidWindow::gl_context() const {
    return reinterpret_cast<void*>(m_context);
}

} // namespace pino
