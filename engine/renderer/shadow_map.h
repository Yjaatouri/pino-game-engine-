#pragma once

#include "engine/core/types.h"
#include "engine/renderer/shader.h"
#include "engine/renderer/texture.h"
#include "engine/renderer/gl_es3.h"
#include <glm/glm.hpp>

namespace pino {

class ShadowMap {
public:
    ShadowMap() = default;
    ~ShadowMap();

    ShadowMap(const ShadowMap&) = delete;
    ShadowMap& operator=(const ShadowMap&) = delete;

    ShadowMap(ShadowMap&& other) noexcept;
    ShadowMap& operator=(ShadowMap&& other) noexcept;

    // Create shadow map FBO + depth texture at the given resolution.
    bool init(u32 resolution = 1024);
    void destroy();

    // Bind the shadow FBO for the depth pass.
    void bind();
    void unbind();

    // Compute light view-projection from a directional light direction.
    // Uses an orthographic frustum covering the region around center.
    void compute_light_vp(const glm::vec3& light_dir,
                          const glm::vec3& center = {0,0,0},
                          f32 half_size = 15.0f,
                          f32 near_plane = 0.1f,
                          f32 far_plane = 40.0f);

    // Bind the shadow depth texture to the given texture unit.
    void bind_depth_texture(u32 slot = 1) const;

    // Shorthands
    GLuint depth_texture() const { return m_depth_tex; }
    const glm::mat4& light_vp() const { return m_light_vp; }
    u32 resolution() const { return m_resolution; }
    bool is_valid() const { return m_fbo != 0; }

private:
    GLuint m_fbo       = 0;
    GLuint m_depth_tex = 0;
    u32    m_resolution = 1024;
    glm::mat4 m_light_vp{1.0f};
    Shader m_depth_shader;
};

} // namespace pino
