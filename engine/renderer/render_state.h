#pragma once

#include "engine/core/types.h"
#include "engine/renderer/gl_es3.h"

namespace pino {

class RenderState {
public:
    static RenderState& instance();

    void set_depth_test(bool enable);
    void set_depth_write(bool enable);
    void set_depth_func(GLenum func);

    void set_blend(bool enable);
    void set_blend_func(GLenum src, GLenum dst);
    void set_blend_equation(GLenum mode);

    void set_cull_face(bool enable);
    void set_cull_mode(GLenum mode);
    void set_front_face(GLenum mode);

    void set_viewport(GLint x, GLint y, GLsizei w, GLsizei h);

    void push_state();
    void pop_state();

    bool depth_test()  const { return m_state.depth_test; }
    bool blend()       const { return m_state.blend; }
    bool cull_face()   const { return m_state.cull_face; }

private:
    RenderState();

    struct GLState {
        bool   depth_test  = true;
        bool   depth_write = true;
        GLenum depth_func  = GL_LESS;
        bool   blend       = false;
        GLenum blend_src   = GL_SRC_ALPHA;
        GLenum blend_dst   = GL_ONE_MINUS_SRC_ALPHA;
        GLenum blend_eq    = GL_FUNC_ADD;
        bool   cull_face   = false;
        GLenum cull_mode   = GL_BACK;
        GLenum front_face  = GL_CCW;
        GLint  vp_x = 0, vp_y = 0;
        GLsizei vp_w = 0, vp_h = 0;
    };

    GLState m_state;
    GLState m_stack[8];
    i32     m_stack_depth = 0;
};

} // namespace pino
