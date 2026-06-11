#pragma once

#include "engine/core/types.h"
#include "engine/renderer/gl_es3.h"

namespace pino {

class Framebuffer {
public:
    Framebuffer() = default;
    ~Framebuffer();

    Framebuffer(Framebuffer&& other) noexcept;
    Framebuffer& operator=(Framebuffer&& other) noexcept;
    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    bool create(i32 width, i32 height, bool with_depth = true);
    void destroy();

    void bind();
    void unbind();

    void bind_color_texture(u32 slot = 0) const;

    bool is_complete() const { return m_complete; }
    bool is_valid()    const { return m_fbo != 0; }

    i32  width()  const { return m_width; }
    i32  height() const { return m_height; }
    GLuint handle() const { return m_fbo; }

private:
    GLuint m_fbo    = 0;
    GLuint m_color  = 0;
    GLuint m_depth  = 0;
    i32    m_width  = 0;
    i32    m_height = 0;
    bool   m_complete = false;
};

} // namespace pino
