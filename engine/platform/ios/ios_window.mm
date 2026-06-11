#include "ios_window.h"
#include "engine/core/log.h"
#import <UIKit/UIKit.h>
#import <OpenGLES/EAGL.h>
#import <OpenGLES/ES3/gl.h>
#import <OpenGLES/ES3/glext.h>
#import <QuartzCore/CAEAGLLayer.h>

// Forward-declare EAGLView methods we need
@interface EAGLView : UIView
@property (nonatomic, readonly) EAGLContext* context;
@property (nonatomic, readonly) GLuint colorRenderbuffer;
@property (nonatomic, readonly) GLuint framebuffer;
- (BOOL)createFramebuffer;
- (void)deleteFramebuffer;
@end

namespace pino {

IOSWindow::IOSWindow(void* eagl_view) {
    m_view = (__bridge EAGLView*)eagl_view;
    if (!m_view) {
        PINO_ERROR("IOSWindow: no EAGLView provided");
        return;
    }

    m_ctx = m_view.context;
    if (!m_ctx) {
        PINO_ERROR("IOSWindow: EAGLView has no context");
        return;
    }

    [EAGLContext setCurrentContext:m_ctx];
    PINO_INFO("IOSWindow initialized");
}

IOSWindow::~IOSWindow() {
    if (m_ctx && [EAGLContext currentContext] == m_ctx) {
        [EAGLContext setCurrentContext:nil];
    }
}

void IOSWindow::make_current() const {
    if (m_ctx && [EAGLContext currentContext] != m_ctx) {
        [EAGLContext setCurrentContext:m_ctx];
    }
}

void IOSWindow::on_framebuffer_recreated() {
    make_current();
    // Framebuffer was recreated by EAGLView after layoutSubviews.
    // If we lost the context during backgrounding, set the restore flag.
    // (EAGLContext preservation depends on the device; iOS typically
    //  preserves it, but we detect via the context validity check.)
    m_needs_context_restore = true;
    PINO_INFO("IOSWindow: framebuffer recreated");
}

void IOSWindow::poll_events() {
    // Events handled via UIKit touch callbacks
}

void IOSWindow::swap_buffers() {
    if (!m_ctx || !m_view) return;
    make_current();
    glBindRenderbuffer(GL_RENDERBUFFER, m_view.colorRenderbuffer);
    [m_ctx presentRenderbuffer:GL_RENDERBUFFER];
}

bool IOSWindow::should_close() const {
    return false;
}

u32 IOSWindow::width() const {
    if (!m_view) return 0;
    UIView* view = (__bridge UIView*)m_view;
    return static_cast<u32>(view.bounds.size.width * view.contentScaleFactor);
}

u32 IOSWindow::height() const {
    if (!m_view) return 0;
    UIView* view = (__bridge UIView*)m_view;
    return static_cast<u32>(view.bounds.size.height * view.contentScaleFactor);
}

f32 IOSWindow::aspect() const {
    return static_cast<f32>(width()) / static_cast<f32>(height());
}

void* IOSWindow::native_handle() const {
    return (__bridge void*)m_view;
}

void* IOSWindow::gl_context() const {
    return (__bridge void*)m_ctx;
}

} // namespace pino
