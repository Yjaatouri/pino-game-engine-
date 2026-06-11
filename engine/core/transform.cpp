#include "transform.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace pino {

glm::mat4 Transform::matrix() const {
    return glm::translate(glm::mat4(1.0f), position)
         * glm::mat4_cast(rotation)
         * glm::scale(glm::mat4(1.0f), scale);
}

glm::vec3 Transform::forward() const {
    return glm::normalize(rotation * glm::vec3(0.0f, 0.0f, -1.0f));
}

glm::vec3 Transform::right() const {
    return glm::normalize(rotation * glm::vec3(1.0f, 0.0f, 0.0f));
}

glm::vec3 Transform::up() const {
    return glm::normalize(rotation * glm::vec3(0.0f, 1.0f, 0.0f));
}

void Transform::translate(const glm::vec3& delta) {
    position += delta;
}

void Transform::rotate(f32 angle_rad, const glm::vec3& axis) {
    f32 len = glm::length(axis);
    if (len < Math::EPSILON) return;
    rotation = glm::rotate(rotation, angle_rad, axis / len);
}

void Transform::look_at(const glm::vec3& target, const glm::vec3& up) {
    glm::vec3 dir = target - position;
    f32 dist = glm::length(dir);
    if (dist < Math::EPSILON) return;
    dir /= dist;

    // Handle degenerate case: dir parallel to up
    glm::vec3 fwd = -dir;
    glm::vec3 right = glm::cross(up, fwd);
    f32 right_len = glm::length(right);
    if (right_len < Math::EPSILON) {
        // dir is parallel to up; use a different reference up
        glm::vec3 alt_up = (std::fabs(up.y) < 0.9f) ? glm::vec3(0,1,0) : glm::vec3(1,0,0);
        right = glm::cross(alt_up, fwd);
        right_len = glm::length(right);
        if (right_len < Math::EPSILON) return;
    }
    right /= right_len;
    glm::vec3 new_up = glm::cross(fwd, right);

    glm::mat3 rot;
    rot[0] = right;
    rot[1] = new_up;
    rot[2] = fwd;
    rotation = glm::quat_cast(rot);
}

Transform Transform::lerp(const Transform& a, const Transform& b, f32 t) {
    Transform result;
    result.position = glm::mix(a.position, b.position, t);
    result.rotation = glm::slerp(a.rotation, b.rotation, t);
    result.scale    = glm::mix(a.scale, b.scale, t);
    return result;
}

} // namespace pino
