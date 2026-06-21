#pragma once

#include "engine/ecs/entity.h"
#include "engine/ecs/entity_registry.h"

namespace pino {

// Safe reference to an entity. Validates liveness on every dereference
// by checking the generation counter in the registry. Prevents use of
// dangling IDs after destruction.
//
// Game code should store EntityHandle, not raw EntityId, to guarantee
// safe cross-frame access.
class EntityHandle {
public:
    EntityHandle() = default;

    EntityHandle(EntityRegistry* registry, EntityId id)
        : m_registry(registry), m_id(id) {}

    // Underlying entity ID.
    EntityId id() const { return m_id; }

    // Returns true if the entity is still alive in the registry.
    bool alive() const {
        return m_registry && m_registry->alive(m_id);
    }

    // Destroy the entity through the registry.
    void destroy() {
        if (alive()) m_registry->destroy(m_id);
    }

    explicit operator bool() const { return alive(); }

    bool operator==(const EntityHandle& o) const { return m_id == o.m_id; }
    bool operator!=(const EntityHandle& o) const { return m_id != o.m_id; }

private:
    EntityRegistry* m_registry = nullptr;
    EntityId m_id = NullEntity;
};

} // namespace pino
