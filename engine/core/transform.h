#pragma once

#include "engine/core/types.h"
#include "engine/core/math_utils.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace pino {

struct Transform {
    glm::vec3 position = {0.0f, 0.0f, 0.0f};
    glm::quat rotation = glm::identity<glm::quat>();
    glm::vec3 scale    = {1.0f, 1.0f, 1.0f};

    glm::mat4 matrix() const;

    glm::vec3 forward() const;
    glm::vec3 right()   const;
    glm::vec3 up()      const;

    void translate(const glm::vec3& delta);
    void rotate(f32 angle_rad, const glm::vec3& axis);
    void look_at(const glm::vec3& target, const glm::vec3& up = {0, 1, 0});

    static Transform lerp(const Transform& a, const Transform& b, f32 t);
};

} // namespace pino
