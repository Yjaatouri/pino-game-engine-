#pragma once

#include "engine/core/types.h"
#include "engine/scene/entity.h"
#include "engine/renderer/light.h"
#include <glm/glm.hpp>

namespace pino {

struct LightComponent {
    enum class Type : u8 { Point, Directional };

    Type type = Type::Point;

    glm::vec3 color{1.0f, 1.0f, 1.0f};
    float constant  = 1.0f;
    float linear    = 0.09f;
    float quadratic = 0.032f;

    glm::vec3 direction{0.0f, -1.0f, 0.0f};

    // If set, sync() reads world_position/forward from this entity.
    // Otherwise, the light uses position/direction as-is.
    Entity* entity = nullptr;

    // Sync with entity transform:
    //   Point:        position = entity->world_position()
    //   Directional:  direction = entity->local_transform().forward()
    void sync() {
        if (!entity) return;
        if (type == Type::Point) {
            position = entity->world_position();
        } else {
            direction = entity->local_transform().forward();
        }
    }

    // Convenience: populate a PointLight struct from this component.
    PointLight to_point() const {
        PointLight p;
        p.position  = position;
        p.color     = color;
        p.constant  = constant;
        p.linear    = linear;
        p.quadratic = quadratic;
        return p;
    }

    // Internal state — set after sync()
    glm::vec3 position{0.0f};
};

} // namespace pino
