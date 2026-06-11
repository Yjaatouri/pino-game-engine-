#include "collision_world.h"
#include "engine/core/event_bus.h"
#include <algorithm>
#include <cfloat>

namespace pino {

static constexpr f32 COLLISION_EPSILON = 1e-6f;

CollisionWorld::CollisionWorld() {
    m_destroy_handle = EventBus::instance().subscribe<EntityDestroyedEvent>(
        [this](const EntityDestroyedEvent& ev) {
            unregister_collider(*ev.entity);
        });
}

CollisionWorld::~CollisionWorld() {
    EventBus::instance().unsubscribe(m_destroy_handle);
}

void CollisionWorld::register_collider(Entity& entity, const ColliderComponent& component) {
    // Prevent duplicate registration
    auto it = std::find_if(m_colliders.begin(), m_colliders.end(),
                           [&](const ColliderEntry& e) { return e.entity == &entity; });
    if (it != m_colliders.end()) {
        it->component = component;
        it->component.update_world_aabb(entity.world_matrix());
        return;
    }
    ColliderEntry entry;
    entry.entity = &entity;
    entry.component = component;
    entry.component.update_world_aabb(entity.world_matrix());
    m_colliders.push_back(entry);
}

void CollisionWorld::unregister_collider(Entity& entity) {
    auto it = std::find_if(m_colliders.begin(), m_colliders.end(),
                           [&](const ColliderEntry& e) { return e.entity == &entity; });
    if (it != m_colliders.end()) {
        *it = m_colliders.back();
        m_colliders.pop_back();
    }
}

void CollisionWorld::clear() {
    m_colliders.clear();
    m_debug_aabbs.clear();
}

void CollisionWorld::emit_collision(Entity& a, Entity& b) const {
    CollisionEvent ev;
    ev.a = &a;
    ev.b = &b;
    EventBus::instance().emit(ev);
}

void CollisionWorld::update(f32 /*dt*/) {
    // 1. Update world-space AABBs from current entity world transforms
    //    (uses world_matrix() to correctly handle parent hierarchy)
    for (auto& entry : m_colliders) {
        if (!entry.component.enabled) continue;
        if (!entry.entity->is_active()) continue;
        entry.component.update_world_aabb(entry.entity->world_matrix());
    }

    // 2. Collect AABBs for debug rendering (only if requested)
    if (show_debug) {
        m_debug_aabbs.clear();
        for (auto& entry : m_colliders) {
            if (entry.component.enabled && entry.entity->is_active()) {
                m_debug_aabbs.push_back(entry.component.world_aabb);
            }
        }
    }

    // 3. Detect collisions, emit events, and resolve
    for (size_t i = 0; i < m_colliders.size(); ++i) {
        for (size_t j = i + 1; j < m_colliders.size(); ++j) {
            auto& a = m_colliders[i];
            auto& b = m_colliders[j];

            // Skip disabled or inactive entities
            if (!a.component.enabled || !b.component.enabled) continue;
            if (!a.entity->is_active() || !b.entity->is_active()) continue;

            // Layer/mask filtering
            if (!ColliderComponent::should_collide(a.component, b.component)) continue;

            // AABB overlap test
            if (!a.component.world_aabb.overlaps(b.component.world_aabb)) continue;

            // Emit collision event BEFORE resolution (defined order)
            emit_collision(*a.entity, *b.entity);

            // Compute MTV (push a out of b)
            glm::vec3 mtv = a.component.world_aabb.push_out(b.component.world_aabb);

            // Apply push-out based on static/dynamic classification
            if (!a.component.is_static && b.component.is_static) {
                // A dynamic, B static — push A only
                a.entity->local_transform().position += mtv;
                a.component.world_aabb.min += mtv;
                a.component.world_aabb.max += mtv;
            } else if (a.component.is_static && !b.component.is_static) {
                // A static, B dynamic — push B only (reverse MTV)
                mtv = b.component.world_aabb.push_out(a.component.world_aabb);
                b.entity->local_transform().position += mtv;
                b.component.world_aabb.min += mtv;
                b.component.world_aabb.max += mtv;
            } else if (!a.component.is_static && !b.component.is_static) {
                // Both dynamic — split push-out 50/50
                glm::vec3 half = mtv * 0.5f;
                a.entity->local_transform().position += half;
                a.component.world_aabb.min += half;
                a.component.world_aabb.max += half;
                b.entity->local_transform().position -= half;
                b.component.world_aabb.min -= half;
                b.component.world_aabb.max -= half;
            }
            // Both static: no resolution
        }
    }
}

RaycastResult CollisionWorld::raycast(const Ray& ray, f32 max_distance,
                                       u32 layer_mask) const {
    RaycastResult closest;
    closest.t = max_distance;

    for (const auto& entry : m_colliders) {
        if (!entry.component.enabled) continue;
        if (!entry.entity->is_active()) continue;

        // Layer filtering
        if ((entry.component.collision_layer & layer_mask) == 0) continue;

        // Ray-AABB intersection
        f32 t = 0.0f;
        glm::vec3 normal;
        if (rayAABBIntersection(ray, entry.component.world_aabb, t, normal)) {
            if (t > 0.0f && t < closest.t) {
                closest.t      = t;
                closest.entity = entry.entity;
                closest.point  = ray.at(t);
                closest.normal = normal;
            }
        }
    }
    return closest;
}

std::vector<Entity*> CollisionWorld::overlap_aabb(const AABB& aabb,
                                                    u32 layer_mask) const {
    std::vector<Entity*> result;
    for (const auto& entry : m_colliders) {
        if (!entry.component.enabled) continue;
        if (!entry.entity->is_active()) continue;
        if ((entry.component.collision_layer & layer_mask) == 0) continue;
        if (entry.component.world_aabb.overlaps(aabb)) {
            result.push_back(entry.entity);
        }
    }
    return result;
}

} // namespace pino
