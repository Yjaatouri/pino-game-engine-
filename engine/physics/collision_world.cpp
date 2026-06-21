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

// ── Auto-sizing ──────────────────────────────────────────────

void CollisionWorld::auto_size_grid(f32 multiplier) {
    std::vector<AABB> aabbs;
    aabbs.reserve(m_colliders.size());
    for (auto& e : m_colliders) {
        if (e.component.enabled && e.entity->is_active()) {
            // Compute AABB once (matches what update_aabbs would do)
            e.component.update_world_aabb(e.entity->world_matrix());
            aabbs.push_back(e.component.world_aabb);
        }
    }
    m_grid.auto_size(aabbs, multiplier);
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
    entry.moved = true;
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
    CollisionEnterEvent ev; ev.a = &a; ev.b = &b;
    EventBus::instance().emit(ev);
}

void CollisionWorld::emit_exit(Entity& a, Entity& b) const {
    CollisionExitEvent ev; ev.a = &a; ev.b = &b;
    EventBus::instance().emit(ev);
}

// ── Grid diagnostics ─────────────────────────────────────────

void CollisionWorld::collect_grid_diagnostics() {
    stats.candidate_pairs_total = m_pair_scratch.size();
    stats.active_cell_count = 0;
    stats.max_colliders_per_cell = 0;
    f64 collider_sum = 0.0;

    for (const auto& kv : m_grid.cells()) {
        u32 cnt = static_cast<u32>(kv.second.size());
        ++stats.active_cell_count;
        collider_sum += cnt;
        if (cnt > stats.max_colliders_per_cell)
            stats.max_colliders_per_cell = cnt;
    }
    stats.avg_colliders_per_cell =
        stats.active_cell_count > 0
            ? collider_sum / static_cast<f64>(stats.active_cell_count)
            : 0.0;

    // Compute avg cells per collider (insertion count divided by active colliders)
    u32 active = 0;
    for (const auto& e : m_colliders) {
        if (e.component.enabled && e.entity->is_active()) ++active;
    }
    stats.avg_cells_per_collider =
        active > 0
            ? static_cast<f64>(stats.cells_touched_total) / static_cast<f64>(active)
            : 0.0;
}

// ── Sub-steps ────────────────────────────────────────────────

void CollisionWorld::update_aabbs() {
    ScopedTimer timer(stats.aabb_update_us);

    for (auto& entry : m_colliders) {
        if (!entry.component.enabled) continue;
        if (!entry.entity->is_active()) continue;
        // Skip static colliders — their AABBs never change
        if (entry.component.is_static) continue;
        entry.component.update_world_aabb(entry.entity->world_matrix());
        entry.moved = true;
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
    stats.candidate_pairs_unique = 0;
    stats.actual_overlaps = 0;
    stats.cells_touched_total = 0;

    m_pair_scratch.clear();
    m_current_pairs.clear();

    u32 N = static_cast<u32>(m_colliders.size());

    auto timer_start = [&]() -> f64 {
        return static_cast<f64>(std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count());
    };

    f64 bp_start = timer_start();
    f64 bp_end = bp_start;

    switch (m_broad_phase) {

    case BroadPhaseMode::BruteForce: {
        u64 total_checks = 0;
        for (u32 i = 0; i < N; ++i) {
            const auto& a = m_colliders[i];
            if (!a.component.enabled || !a.entity->is_active()) continue;
            for (u32 j = i + 1; j < N; ++j) {
                const auto& b = m_colliders[j];
                if (!b.component.enabled || !b.entity->is_active()) continue;
                ++total_checks;
                if (!ColliderComponent::should_collide(a.component, b.component)) continue;
                if (!a.component.world_aabb.overlaps(b.component.world_aabb)) continue;
                u64 pair_id = (static_cast<u64>(i) << 32) | j;
                m_current_pairs.push_back(pair_id);
            }
        }
        stats.candidate_pairs_total = total_checks;
        stats.candidate_pairs_unique = total_checks;
        std::sort(m_current_pairs.begin(), m_current_pairs.end());
        auto last = std::unique(m_current_pairs.begin(), m_current_pairs.end());
        m_current_pairs.erase(last, m_current_pairs.end());
        stats.actual_overlaps = m_current_pairs.size();
        // All time reported as narrow-phase (no distinct broad-phase)
        bp_end = timer_start();
        stats.broad_phase_us = 0.0;
        stats.narrow_phase_us = (bp_end - bp_start) / 1000.0;
        break;
    }

    case BroadPhaseMode::UniformGrid: {
        m_grid.clear();
        for (u32 i = 0; i < N; ++i) {
            if (!m_colliders[i].component.enabled) continue;
            if (!m_colliders[i].entity->is_active()) continue;
            m_grid.insert(i, m_colliders[i].component.world_aabb);
        }
        stats.cells_touched_total = m_grid.total_insertions();
        m_grid.collect_pairs(m_pair_scratch);
        bp_end = timer_start();
        stats.broad_phase_us = (bp_end - bp_start) / 1000.0;

        std::sort(m_pair_scratch.begin(), m_pair_scratch.end());
        auto last = std::unique(m_pair_scratch.begin(), m_pair_scratch.end());
        stats.candidate_pairs_unique = static_cast<u64>(std::distance(m_pair_scratch.begin(), last));
        m_pair_scratch.erase(last, m_pair_scratch.end());

        f64 np_start = bp_end;
        for (u64 pair_id : m_pair_scratch) {
            u32 i = static_cast<u32>(pair_id >> 32);
            u32 j = static_cast<u32>(pair_id & 0xFFFFFFFF);
            if (test_pair(m_colliders[i], m_colliders[j])) {
                m_current_pairs.push_back(pair_id);
            }
        }
        f64 np_end = timer_start();
        stats.narrow_phase_us = (np_end - np_start) / 1000.0;
        stats.actual_overlaps = m_current_pairs.size();
        collect_grid_diagnostics();
        break;
    }

    case BroadPhaseMode::LooseGrid: {
        m_loose_grid.clear();
        for (u32 i = 0; i < N; ++i) {
            if (!m_colliders[i].component.enabled) continue;
            if (!m_colliders[i].entity->is_active()) continue;
            m_loose_grid.insert(i, m_colliders[i].component.world_aabb);
        }
        stats.cells_touched_total = m_loose_grid.total_insertions();
        m_loose_grid.collect_pairs(m_pair_scratch);
        bp_end = timer_start();
        stats.broad_phase_us = (bp_end - bp_start) / 1000.0;

        std::sort(m_pair_scratch.begin(), m_pair_scratch.end());
        auto last = std::unique(m_pair_scratch.begin(), m_pair_scratch.end());
        stats.candidate_pairs_unique = static_cast<u64>(std::distance(m_pair_scratch.begin(), last));
        m_pair_scratch.erase(last, m_pair_scratch.end());

        f64 np_start = bp_end;
        for (u64 pair_id : m_pair_scratch) {
            u32 i = static_cast<u32>(pair_id >> 32);
            u32 j = static_cast<u32>(pair_id & 0xFFFFFFFF);
            if (test_pair(m_colliders[i], m_colliders[j])) {
                m_current_pairs.push_back(pair_id);
            }
        }
        f64 np_end = timer_start();
        stats.narrow_phase_us = (np_end - np_start) / 1000.0;
        stats.actual_overlaps = m_current_pairs.size();
        break;
    }

    case BroadPhaseMode::SweepAndPrune: {
        // Collect AABBs for active colliders
        std::vector<std::pair<u32, AABB>> idx_aabb;
        idx_aabb.reserve(N);
        for (u32 i = 0; i < N; ++i) {
            if (!m_colliders[i].component.enabled) continue;
            if (!m_colliders[i].entity->is_active()) continue;
            idx_aabb.emplace_back(i, m_colliders[i].component.world_aabb);
        }
        m_sap.collect_pairs(idx_aabb, m_pair_scratch);
        bp_end = timer_start();
        stats.broad_phase_us = (bp_end - bp_start) / 1000.0;

        stats.candidate_pairs_unique = m_pair_scratch.size();

        f64 np_start = bp_end;
        for (u64 pair_id : m_pair_scratch) {
            u32 i = static_cast<u32>(pair_id >> 32);
            u32 j = static_cast<u32>(pair_id & 0xFFFFFFFF);
            if (test_pair(m_colliders[i], m_colliders[j])) {
                m_current_pairs.push_back(pair_id);
            }
        }
        f64 np_end = timer_start();
        stats.narrow_phase_us = (np_end - np_start) / 1000.0;
        stats.actual_overlaps = m_current_pairs.size();
        break;
    }

    } // switch
}

void CollisionWorld::dispatch_events() {
    ScopedTimer timer(stats.event_dispatch_us);

    auto cur = m_current_pairs.begin();
    auto prev = m_prev_pairs.begin();

    while (cur != m_current_pairs.end() && prev != m_prev_pairs.end()) {
        if (*cur < *prev) {
            u32 i = static_cast<u32>(*cur >> 32);
            u32 j = static_cast<u32>(*cur & 0xFFFFFFFF);
            emit_enter(*m_colliders[i].entity, *m_colliders[j].entity);
            ++cur;
        } else if (*prev < *cur) {
            u32 i = static_cast<u32>(*prev >> 32);
            u32 j = static_cast<u32>(*prev & 0xFFFFFFFF);
            emit_exit(*m_colliders[i].entity, *m_colliders[j].entity);
            ++prev;
        } else {
            CollisionStayEvent ev;
            u32 i = static_cast<u32>(*cur >> 32);
            u32 j = static_cast<u32>(*cur & 0xFFFFFFFF);
            ev.a = m_colliders[i].entity; ev.b = m_colliders[j].entity;
            EventBus::instance().emit(ev);
            ++cur; ++prev;
        }
    }
    while (cur != m_current_pairs.end()) {
        u32 i = static_cast<u32>(*cur >> 32);
        u32 j = static_cast<u32>(*cur & 0xFFFFFFFF);
        emit_enter(*m_colliders[i].entity, *m_colliders[j].entity);
        ++cur;
    }
    while (prev != m_prev_pairs.end()) {
        u32 i = static_cast<u32>(*prev >> 32);
        u32 j = static_cast<u32>(*prev & 0xFFFFFFFF);
        emit_exit(*m_colliders[i].entity, *m_colliders[j].entity);
        ++prev;
    }

    m_prev_pairs.swap(m_current_pairs);
    m_current_pairs.clear();
}

void CollisionWorld::resolve_collisions() {
    ScopedTimer timer(stats.resolution_us);

    for (u64 pair_id : m_prev_pairs) {
        u32 i = static_cast<u32>(pair_id >> 32);
        u32 j = static_cast<u32>(pair_id & 0xFFFFFFFF);

        auto& a = m_colliders[i];
        auto& b = m_colliders[j];

        glm::vec3 mtv = a.component.world_aabb.push_out(b.component.world_aabb);

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
    }
}

// ── Full update ──────────────────────────────────────────────

void CollisionWorld::update(f32 /*dt*/) {
    stats.reset();
    f64 frame_start = static_cast<f64>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count());

    update_aabbs();
    detect_collisions();
    dispatch_events();
    resolve_collisions();

    f64 frame_end = static_cast<f64>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count());
    stats.total_us = (frame_end - frame_start) / 1000.0;
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
                closest.t = t;
                closest.entity = entry.entity;
                closest.point = ray.at(t);
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
