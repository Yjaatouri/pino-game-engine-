#include "render_state.h"
#include "render_stats.h"

namespace pino {

RenderState& RenderState::instance() {
    static RenderState s;
    return s;
}

RenderState::RenderState() {
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    m_state.vp_x = vp[0]; m_state.vp_y = vp[1];
    m_state.vp_w = vp[2]; m_state.vp_h = vp[3];
}

void RenderState::set_depth_test(bool enable) {
    if (m_state.depth_test == enable) return;
    m_state.depth_test = enable;
    if (enable) glEnable(GL_DEPTH_TEST);
    else        glDisable(GL_DEPTH_TEST);
    RenderStats::instance().add_state_change();
}

void RenderState::set_depth_write(bool enable) {
    if (m_state.depth_write == enable) return;
    m_state.depth_write = enable;
    glDepthMask(enable ? GL_TRUE : GL_FALSE);
    RenderStats::instance().add_state_change();
}

void RenderState::set_depth_func(GLenum func) {
    if (m_state.depth_func == func) return;
    m_state.depth_func = func;
    glDepthFunc(func);
    RenderStats::instance().add_state_change();
}

void RenderState::set_blend(bool enable) {
    if (m_state.blend == enable) return;
    m_state.blend = enable;
    if (enable) glEnable(GL_BLEND);
    else        glDisable(GL_BLEND);
    RenderStats::instance().add_state_change();
}

void RenderState::set_blend_func(GLenum src, GLenum dst) {
    if (m_state.blend_src == src && m_state.blend_dst == dst) return;
    m_state.blend_src = src;
    m_state.blend_dst = dst;
    glBlendFunc(src, dst);
    RenderStats::instance().add_state_change();
}

void RenderState::set_blend_equation(GLenum mode) {
    if (m_state.blend_eq == mode) return;
    m_state.blend_eq = mode;
    glBlendEquation(mode);
    RenderStats::instance().add_state_change();
}

void RenderState::set_cull_face(bool enable) {
    if (m_state.cull_face == enable) return;
    m_state.cull_face = enable;
    if (enable) glEnable(GL_CULL_FACE);
    else        glDisable(GL_CULL_FACE);
    RenderStats::instance().add_state_change();
}

void RenderState::set_cull_mode(GLenum mode) {
    if (m_state.cull_mode == mode) return;
    m_state.cull_mode = mode;
    glCullFace(mode);
    RenderStats::instance().add_state_change();
}

void RenderState::set_front_face(GLenum mode) {
    if (m_state.front_face == mode) return;
    m_state.front_face = mode;
    glFrontFace(mode);
    RenderStats::instance().add_state_change();
}

void RenderState::set_viewport(GLint x, GLint y, GLsizei w, GLsizei h) {
    if (m_state.vp_x == x && m_state.vp_y == y &&
        m_state.vp_w == w && m_state.vp_h == h) return;
    m_state.vp_x = x; m_state.vp_y = y;
    m_state.vp_w = w; m_state.vp_h = h;
    glViewport(x, y, w, h);
    RenderStats::instance().add_state_change();
}

void RenderState::push_state() {
    if (m_stack_depth >= 8) return;
    m_stack[m_stack_depth++] = m_state;
}

void RenderState::pop_state() {
    if (m_stack_depth <= 0) return;
    GLState prev = m_stack[--m_stack_depth];

    set_depth_test(prev.depth_test);
    set_depth_write(prev.depth_write);
    set_depth_func(prev.depth_func);
    set_blend(prev.blend);
    set_blend_func(prev.blend_src, prev.blend_dst);
    set_blend_equation(prev.blend_eq);
    set_cull_face(prev.cull_face);
    set_cull_mode(prev.cull_mode);
    set_front_face(prev.front_face);
    set_viewport(prev.vp_x, prev.vp_y, prev.vp_w, prev.vp_h);
}

} // namespace pino
