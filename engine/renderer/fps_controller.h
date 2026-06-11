#pragma once

#include "engine/core/types.h"
#include "engine/platform/input.h"
#include "engine/renderer/camera.h"
#include <glm/glm.hpp>

namespace pino {

class FpsController {
public:
    void attach(Camera* camera) { m_camera = camera; }

    void set_speed(f32 move_speed, f32 mouse_sensitivity) {
        m_speed = move_speed;
        m_sensitivity = mouse_sensitivity;
    }

    void set_position(const glm::vec3& pos);
    void look_at(const glm::vec3& target);

    // Call once per frame after Input::begin_frame()
    void update(const Input& input, f32 dt);

    f32 yaw   = -90.0f;
    f32 pitch = 0.0f;

private:
    Camera* m_camera = nullptr;
    f32 m_speed        = 5.0f;
    f32 m_sensitivity  = 0.1f;
};

} // namespace pino
