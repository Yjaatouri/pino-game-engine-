#include "framebuffer.h"

namespace pino {

Framebuffer::~Framebuffer() { destroy(); }

Framebuffer::Framebuffer(Framebuffer&& other) noexcept
    : m_fbo(other.m_fbo), m_color(other.m_color), m_depth(other.m_depth),
      m_width(other.m_width), m_height(other.m_height), m_complete(other.m_complete)
{
    other.m_fbo = other.m_color = other.m_depth = 0;
    other.m_width = other.m_height = 0;
    other.m_complete = false;
}

Framebuffer& Framebuffer::operator=(Framebuffer&& other) noexcept {
    if (this != &other) {
        destroy();
        m_fbo = other.m_fbo; other.m_fbo = 0;
        m_color = other.m_color; other.m_color = 0;
        m_depth = other.m_depth; other.m_depth = 0;
        m_width = other.m_width; other.m_width = 0;
        m_height = other.m_height; other.m_height = 0;
        m_complete = other.m_complete;
        other.m_complete = false;
    }
    return *this;
}

bool Framebuffer::create(i32 width, i32 height, bool with_depth) {
    destroy();
    m_width = width;
    m_height = height;

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    glGenTextures(1, &m_color);
    glBindTexture(GL_TEXTURE_2D, m_color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, m_color, 0);

    if (with_depth) {
        glGenRenderbuffers(1, &m_depth);
        glBindRenderbuffer(GL_RENDERBUFFER, m_depth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24,
                              width, height);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  GL_RENDERBUFFER, m_depth);
    }

    m_complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return m_complete;
}

void Framebuffer::destroy() {
    if (m_fbo)   glDeleteFramebuffers(1, &m_fbo);
    if (m_color) glDeleteTextures(1, &m_color);
    if (m_depth) glDeleteRenderbuffers(1, &m_depth);
    m_fbo = m_color = m_depth = 0;
    m_complete = false;
}

void Framebuffer::bind() {
    if (m_fbo) glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
}

void Framebuffer::unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::bind_color_texture(u32 slot) const {
    if (!m_color) return;
    glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + slot));
    glBindTexture(GL_TEXTURE_2D, m_color);
}

} // namespace pino
