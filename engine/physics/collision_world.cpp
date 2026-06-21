#include "collision_world.h"
#include "engine/core/event_bus.h"
#include "engine/core/log.h"
#include <algorithm>
#include <cfloat>

namespace pino {

static constexpr f32 COLLISION_EPSILON = 1e-6f;

// ── Lifecycle ─────────────────────────────────────────────────

CollisionWorld::CollisionWorld() {
    m_destroy_handle = EventBus::instance().subscribe<EntityDestroyedEvent>(
        [this](const EntityDestroyedEvent& ev) {
            unregister_collider(*ev.entity);
        });
    // Reserve space for pair vectors (avoids initial allocations)
    m_current_pairs.reserve(256);
    m_prev_pairs.reserve(256);
    m_pair_scratch.reserve(1024);
}

CollisionWorld::~CollisionWorld() {
    EventBus::instance().unsubscribe(m_destroy_handle);
}

// ── Broad-phase mode ─────────────────────────────────────────

void CollisionWorld::set_broad_phase_mode(BroadPhaseMode mode) {
    m_broad_phase = mode;
}

// ── Registration ─────────────────────────────────────────────

void CollisionWorld::register_collider(Entity& entity,
                                       const ColliderComponent& component) {
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
    m_current_pairs.clear();
    m_prev_pairs.clear();
}

// ── Pair helpers ─────────────────────────────────────────────

bool CollisionWorld::test_pair(const ColliderEntry& a, const ColliderEntry& b) {
    if (!a.component.enabled || !b.component.enabled) return false;
    if (!a.entity->is_active() || !b.entity->is_active()) return false;
    if (!ColliderComponent::should_collide(a.component, b.component)) return false;
    return a.component.world_aabb.overlaps(b.component.world_aabb);
}

void CollisionWorld::emit_enter(Entity& a, Entity& b) const {
    CollisionEnterEvent ev;
    ev.a = &a; ev.b = &b;
    EventBus::instance().emit(ev);
}

void CollisionWorld::emit_exit(Entity& a, Entity& b) const {
    CollisionExitEvent ev;
    ev.a = &a; ev.b = &b;
    EventBus::instance().emit(ev);
}

// ── Sub-steps ────────────────────────────────────────────────

void CollisionWorld::update_aabbs() {
    for (auto& entry : m_colliders) {
        if (!entry.component.enabled) continue;
        if (!entry.entity->is_active()) continue;
        entry.component.update_world_aabb(entry.entity->world_matrix());
    }

    if (show_debug) {
        m_debug_aabbs.clear();
        for (auto& entry : m_colliders) {
            if (entry.component.enabled && entry.entity->is_active())
                m_debug_aabbs.push_back(entry.component.world_aabb);
        }
    }
}

void CollisionWorld::detect_collisions() {
    m_pair_scratch.clear();
    m_current_pairs.clear();

    u32 N = static_cast<u32>(m_colliders.size());

    if (m_broad_phase == BroadPhaseMode::UniformGrid) {
        // ── Uniform grid broad-phase ──
        m_grid.clear();
        for (u32 i = 0; i < N; ++i) {
            if (!m_colliders[i].component.enabled) continue;
            if (!m_colliders[i].entity->is_active()) continue;
            m_grid.insert(i, m_colliders[i].component.world_aabb);
        }
        m_grid.collect_pairs(m_pair_scratch);

        // Sort + unique candidate pairs
        std::sort(m_pair_scratch.begin(), m_pair_scratch.end());
        auto last = std::unique(m_pair_scratch.begin(), m_pair_scratch.end());
        m_pair_scratch.erase(last, m_pair_scratch.end());

        // Narrow-phase
        for (u64 pair_id : m_pair_scratch) {
            u32 i = static_cast<u32>(pair_id >> 32);
            u32 j = static_cast<u32>(pair_id & 0xFFFFFFFF);
            if (test_pair(m_colliders[i], m_colliders[j])) {
                m_current_pairs.push_back(pair_id);
            }
        }
    } else {
        // ── Brute-force O(n²) ──
        for (u32 i = 0; i < N; ++i) {
            const auto& a = m_colliders[i];
            if (!a.component.enabled || !a.entity->is_active()) continue;
            for (u32 j = i + 1; j < N; ++j) {
                const auto& b = m_colliders[j];
                if (!b.component.enabled || !b.entity->is_active()) continue;
                if (!ColliderComponent::should_collide(a.component, b.component)) continue;
                if (!a.component.world_aabb.overlaps(b.component.world_aabb)) continue;
                u64 pair_id = (static_cast<u64>(i) << 32) | j;
                m_current_pairs.push_back(pair_id);
            }
        }
    }

    // m_current_pairs is already sorted (grid path sorts scratch; brute-force inserts in order)
    if (m_broad_phase == BroadPhaseMode::BruteForce) {
        std::sort(m_current_pairs.begin(), m_current_pairs.end());
    }
    auto last = std::unique(m_current_pairs.begin(), m_current_pairs.end());
    m_current_pairs.erase(last, m_current_pairs.end());
}

void CollisionWorld::dispatch_events() {
    // Both vectors are sorted + uniqued.
    // Walk both to find Enter (in cur, not in prev), Stay (in both), Exit (in prev, not in cur).
    auto cur = m_current_pairs.begin();
    auto prev = m_prev_pairs.begin();

    while (cur != m_current_pairs.end() && prev != m_prev_pairs.end()) {
        if (*cur < *prev) {
            // Enter
            u32 i = static_cast<u32>(*cur >> 32);
            u32 j = static_cast<u32>(*cur & 0xFFFFFFFF);
            emit_enter(*m_colliders[i].entity, *m_colliders[j].entity);
            ++cur;
        } else if (*prev < *cur) {
            // Exit
            u32 i = static_cast<u32>(*prev >> 32);
            u32 j = static_cast<u32>(*prev & 0xFFFFFFFF);
            emit_exit(*m_colliders[i].entity, *m_colliders[j].entity);
            ++prev;
        } else {
            // Stay
            CollisionStayEvent ev;
            u32 i = static_cast<u32>(*cur >> 32);
            u32 j = static_cast<u32>(*cur & 0xFFFFFFFF);
            ev.a = m_colliders[i].entity;
            ev.b = m_colliders[j].entity;
            EventBus::instance().emit(ev);
            ++cur;
            ++prev;
        }
    }

    // Remaining cur = Enter
    while (cur != m_current_pairs.end()) {
        u32 i = static_cast<u32>(*cur >> 32);
        u32 j = static_cast<u32>(*cur & 0xFFFFFFFF);
        emit_enter(*m_colliders[i].entity, *m_colliders[j].entity);
        ++cur;
    }

    // Remaining prev = Exit
    while (prev != m_prev_pairs.end()) {
        u32 i = static_cast<u32>(*prev >> 32);
        u32 j = static_cast<u32>(*prev & 0xFFFFFFFF);
        emit_exit(*m_colliders[i].entity, *m_colliders[j].entity);
        ++prev;
    }

    // Rotate for next frame
    m_prev_pairs.swap(m_current_pairs);
    m_current_pairs.clear();
}

void CollisionWorld::resolve_collisions() {
    for (u64 pair_id : m_prev_pairs) {  // m_prev_pairs holds what was m_current_pairs after swap
        u32 i = static_cast<u32>(pair_id >> 32);
        u32 j = static_cast<u32>(pair_id & 0xFFFFFFFF);

        auto& a = m_colliders[i];
        auto& b = m_colliders[j];

        // Compute MTV (push a out of b)
        glm::vec3 mtv = a.component.world_aabb.push_out(b.component.world_aabb);

        // Apply push-out based on static/dynamic classification
        if (!a.component.is_static && b.component.is_static) {
            a.entity->local_transform().position += mtv;
            a.component.world_aabb.min += mtv;
            a.component.world_aabb.max += mtv;
        } else if (a.component.is_static && !b.component.is_static) {
            mtv = b.component.world_aabb.push_out(a.component.world_aabb);
            b.entity->local_transform().position += mtv;
            b.component.world_aabb.min += mtv;
            b.component.world_aabb.max += mtv;
        } else if (!a.component.is_static && !b.component.is_static) {
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

// ── Full update ──────────────────────────────────────────────

void CollisionWorld::update(f32 /*dt*/) {
    update_aabbs();
    detect_collisions();
    dispatch_events();
    resolve_collisions();
}

// ── Queries ──────────────────────────────────────────────────

RaycastResult CollisionWorld::raycast(const Ray& ray, f32 max_distance,
                                      u32 layer_mask) const {
    RaycastResult closest;
    closest.t = max_distance;

    for (const auto& entry : m_colliders) {
        if (!entry.component.enabled) continue;
        if (!entry.entity->is_active()) continue;
        if ((entry.component.collision_layer & layer_mask) == 0) continue;

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
