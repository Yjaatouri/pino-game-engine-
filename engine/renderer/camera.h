#pragma once

#include "engine/core/types.h"
#include <glm/glm.hpp>

namespace pino {

class Camera {
public:
    Camera() = default;

    void perspective(f32 fov_degrees, f32 aspect, f32 near_plane, f32 far_plane);

    void look_at(const glm::vec3& eye, const glm::vec3& target, const glm::vec3& up);
    void set_position(const glm::vec3& pos);
    void set_target(const glm::vec3& target);

    void move(const glm::vec3& delta);
    void orbit(f32 yaw_delta, f32 pitch_delta);

    const glm::vec3& position() const { return m_position; }
    const glm::vec3& target()   const { return m_target; }

    const glm::mat4& view()        const { return m_view; }
    const glm::mat4& projection()  const { return m_proj; }
    glm::mat4        view_proj()   const { return m_proj * m_view; }

    f32 aspect() const { return m_aspect; }

private:
    void rebuild_view();

    glm::mat4 m_view = glm::mat4(1.0f);
    glm::mat4 m_proj = glm::mat4(1.0f);

    glm::vec3 m_position = glm::vec3(0.0f, 0.0f, 3.0f);
    glm::vec3 m_target   = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 m_up       = glm::vec3(0.0f, 1.0f, 0.0f);

    f32 m_yaw   = 0.0f;
    f32 m_pitch = 0.0f;
    f32 m_distance = 3.0f;

    f32 m_aspect   = 16.0f / 9.0f;
    f32 m_fov      = 60.0f;
    f32 m_near     = 0.1f;
    f32 m_far      = 100.0f;
};

} // namespace pino
