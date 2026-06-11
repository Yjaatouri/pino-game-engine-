#pragma once
#include "engine/physics/aabb.h"
#include "engine/physics/collider_component.h"
#include "engine/scene/entity.h"
#include "engine/core/event_bus.h"
#include "engine/core/math_utils.h"
#include <vector>
#include <unordered_map>

namespace pino {

struct RaycastResult {
    Entity*   entity = nullptr;
    f32       t      = 0.0f;
    glm::vec3 point  = {0,0,0};
    glm::vec3 normal = {0,0,0};
};

class CollisionWorld {
public:
    CollisionWorld();
    ~CollisionWorld();

    CollisionWorld(const CollisionWorld&) = delete;
    CollisionWorld& operator=(const CollisionWorld&) = delete;

    // Register/unregister a collider for a given entity
    void register_collider(Entity& entity, const ColliderComponent& component);
    void unregister_collider(Entity& entity);

    // Full collision step: AABB update, pair detection, resolution, events
    void update(f32 dt);

    // Remove all colliders
    void clear();

    // Raycast against all registered colliders.
    // Returns the closest hit within max_distance, filtered by layer_mask.
    // layer_mask: only entities whose collision_layer matches this mask are tested.
    RaycastResult raycast(const Ray& ray, f32 max_distance,
                          u32 layer_mask = 0xFFFFFFFF) const;

    // Overlap query: returns all entities whose world AABB intersects the query AABB.
    // Filtered by layer_mask.
    std::vector<Entity*> overlap_aabb(const AABB& aabb,
                                       u32 layer_mask = 0xFFFFFFFF) const;

    // Toggle debug drawing
    bool show_debug = false;
    const std::vector<AABB>& debug_aabbs() const { return m_debug_aabbs; }

private:
    struct ColliderEntry {
        Entity*          entity = nullptr;
        ColliderComponent component;
    };

    // Emit a CollisionEvent for a colliding pair (once per pair per frame)
    void emit_collision(Entity& a, Entity& b) const;

    std::vector<ColliderEntry> m_colliders;
    std::vector<AABB>          m_debug_aabbs;
    EventBus::HandlerId        m_destroy_handle = 0;
};

} // namespace pino
