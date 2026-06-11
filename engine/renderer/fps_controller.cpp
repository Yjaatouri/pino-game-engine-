#include "fps_controller.h"
#include <glm/gtc/matrix_transform.hpp>

namespace pino {

void FpsController::set_position(const glm::vec3& pos) {
    if (m_camera) m_camera->look_at(pos, pos + glm::vec3(0, 0, -1), glm::vec3(0, 1, 0));
}

void FpsController::look_at(const glm::vec3& target) {
    if (!m_camera) return;
    glm::vec3 dir = glm::normalize(target - m_camera->position());
    yaw   = glm::degrees(atan2(dir.z, dir.x));
    pitch = glm::degrees(asin(glm::clamp(dir.y, -1.0f, 1.0f)));
}

void FpsController::update(const Input& input, f32 dt) {
    if (!m_camera) return;

    // ---- Mouse look ----
    yaw   += static_cast<f32>(input.mouse_dx()) * m_sensitivity;
    pitch += static_cast<f32>(input.mouse_dy()) * m_sensitivity;
    pitch = glm::clamp(pitch, -89.0f, 89.0f);

    // Direction from yaw/pitch
    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front   = glm::normalize(front);

    glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0, 1, 0)));
    glm::vec3 up    = glm::normalize(glm::cross(right, front));

    // ---- WASD movement ----
    glm::vec3 pos = m_camera->position();
    f32 speed = m_speed * dt;

    if (input.key_down(Key::W)) pos += front * speed;
    if (input.key_down(Key::S)) pos -= front * speed;
    if (input.key_down(Key::A)) pos -= right * speed;
    if (input.key_down(Key::D)) pos += right * speed;

    m_camera->look_at(pos, pos + front, up);
}

} // namespace pino
