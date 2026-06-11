#include "shadow_map.h"
#include "engine/core/log.h"
#include <glm/gtc/matrix_transform.hpp>

namespace pino {

ShadowMap::~ShadowMap() { destroy(); }

ShadowMap::ShadowMap(ShadowMap&& other) noexcept
    : m_fbo(other.m_fbo), m_depth_tex(other.m_depth_tex),
      m_resolution(other.m_resolution), m_light_vp(other.m_light_vp)
{
    other.m_fbo = other.m_depth_tex = 0;
}

ShadowMap& ShadowMap::operator=(ShadowMap&& other) noexcept {
    if (this != &other) {
        destroy();
        m_fbo = other.m_fbo; other.m_fbo = 0;
        m_depth_tex = other.m_depth_tex; other.m_depth_tex = 0;
        m_resolution = other.m_resolution;
        m_light_vp = other.m_light_vp;
    }
    return *this;
}

bool ShadowMap::init(u32 resolution) {
    destroy();

    m_resolution = resolution;

    // Create depth texture
    glGenTextures(1, &m_depth_tex);
    glBindTexture(GL_TEXTURE_2D, m_depth_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
                 static_cast<GLsizei>(resolution), static_cast<GLsizei>(resolution),
                 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Create FBO
    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                           GL_TEXTURE_2D, m_depth_tex, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        PINO_ERROR("Shadow map FBO incomplete (status 0x%x)", status);
        destroy();
        return false;
    }

    // Inline depth-only shader
    static const char* depth_vert = R"(
#version 300 es
layout(location=0) in vec3 a_pos;
uniform mat4 u_light_vp;
uniform mat4 u_model;
void main() {
    gl_Position = u_light_vp * u_model * vec4(a_pos, 1.0);
})";
    static const char* depth_frag = R"(
#version 300 es
precision highp float;
void main() {})";

    if (!m_depth_shader.load(depth_vert, depth_frag)) {
        PINO_ERROR("Shadow depth shader failed to compile");
        destroy();
        return false;
    }

    PINO_INFO("Shadow map initialized (%ux%u)", resolution, resolution);
    return true;
}

void ShadowMap::destroy() {
    if (m_fbo)       glDeleteFramebuffers(1, &m_fbo);
    if (m_depth_tex) glDeleteTextures(1, &m_depth_tex);
    m_fbo = m_depth_tex = 0;
    m_resolution = 1024;
}

void ShadowMap::bind() {
    if (m_fbo) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        glViewport(0, 0, static_cast<GLsizei>(m_resolution),
                   static_cast<GLsizei>(m_resolution));
        glClear(GL_DEPTH_BUFFER_BIT);
    }
}

void ShadowMap::unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ShadowMap::compute_light_vp(const glm::vec3& light_dir,
                                  const glm::vec3& center,
                                  f32 half_size,
                                  f32 near_plane,
                                  f32 far_plane) {
    glm::vec3 dir = glm::normalize(light_dir);
    glm::vec3 eye = center - dir * far_plane;
    glm::mat4 light_view = glm::lookAt(eye, center, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 light_proj = glm::ortho(-half_size, half_size,
                                       -half_size, half_size,
                                       near_plane, far_plane);
    m_light_vp = light_proj * light_view;
}

void ShadowMap::bind_depth_texture(u32 slot) const {
    if (!m_depth_tex) return;
    glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + slot));
    glBindTexture(GL_TEXTURE_2D, m_depth_tex);
}

} // namespace pino
