#pragma once
#include "engine/platform/window.h"

#ifdef __OBJC__
@class EAGLContext;
@class EAGLView;
#else
typedef struct objc_object EAGLContext;
typedef struct objc_object EAGLView;
#endif

namespace pino {

class IOSWindow final : public Window {
public:
    explicit IOSWindow(void* eagl_view);
    ~IOSWindow() override;

    void poll_events() override;
    void swap_buffers() override;
    bool should_close() const override;

    u32  width()  const override;
    u32  height() const override;
    f32  aspect() const override;

    void* native_handle() const override;
    void* gl_context()    const override;

    // Called when the EAGLView recreates its framebuffer (e.g. after
    // layoutSubviews following background/foreground or resize).
    void on_framebuffer_recreated();

    // Returns true if the GL context was freshly restored and all GPU
    // resources (shaders, textures, VBOs) need re-upload.
    // Call once per lifecycle transition; auto-clears.
    bool needs_context_restore() const {
        bool v = m_needs_context_restore;
        const_cast<IOSWindow*>(this)->m_needs_context_restore = false;
        return v;
    }

    // Ensure current context before any GL call
    void make_current() const;

private:
    EAGLView*   m_view    = nullptr;
    EAGLContext* m_ctx    = nullptr;
    bool         m_context_restored = false;
    mutable bool m_needs_context_restore = false;
};

} // namespace pino
