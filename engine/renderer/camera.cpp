#include "camera.h"
#include <glm/gtc/matrix_transform.hpp>

namespace pino {

void Camera::perspective(f32 fov_degrees, f32 aspect, f32 near_plane, f32 far_plane) {
    m_fov    = fov_degrees;
    m_aspect = aspect;
    m_near   = near_plane;
    m_far    = far_plane;
    m_proj   = glm::perspective(glm::radians(fov_degrees), aspect, near_plane, far_plane);
}

void Camera::look_at(const glm::vec3& eye, const glm::vec3& target, const glm::vec3& up) {
    m_position = eye;
    m_target   = target;
    m_up       = up;
    m_distance = glm::length(eye - target);
    rebuild_view();
}

void Camera::set_position(const glm::vec3& pos) {
    m_position = pos;
    rebuild_view();
}

void Camera::set_target(const glm::vec3& target) {
    m_target = target;
    rebuild_view();
}

void Camera::move(const glm::vec3& delta) {
    m_position += delta;
    m_target   += delta;
    rebuild_view();
}

void Camera::orbit(f32 yaw_delta, f32 pitch_delta) {
    m_yaw   = glm::mod(m_yaw + yaw_delta, 360.0f);
    m_pitch = glm::clamp(m_pitch + pitch_delta, -89.0f, 89.0f);

    f32 cy = cos(glm::radians(m_yaw));
    f32 sy = sin(glm::radians(m_yaw));
    f32 cp = cos(glm::radians(m_pitch));
    f32 sp = sin(glm::radians(m_pitch));

    glm::vec3 dir;
    dir.x = cy * cp;
    dir.y = sp;
    dir.z = sy * cp;

    m_position = m_target + dir * m_distance;
    rebuild_view();
}

void Camera::rebuild_view() {
    m_view = glm::lookAt(m_position, m_target, m_up);
}

} // namespace pino
