#pragma once

#include "engine/ecs/entity.h"
#include "engine/ecs/entity_registry.h"
#include "engine/ecs/entity_handle.h"
#include "engine/ecs/scene_graph.h"
#include "engine/ecs/component_pool.h"
#include "engine/ecs/components.h"
#include "engine/ecs/ecs_physics_adapter.h"
#include "engine/audio/audio_manager.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace pino {

// Aggregate that owns an EntityRegistry, SceneGraph, typed component pools,
// and a physics adapter. Provides unified entity lifecycle and system
// dispatch methods (update_physics / update_render / update_audio).
//
// Systems read from components and write to external managers
// (CollisionWorld, RenderQueue, AudioManager) — they own no data.
class EcsScene {
public:
    EcsScene()
        : m_registry()
        , m_scene_graph(&m_registry)
    {
        m_render_components.set_registry(&m_registry);
        m_physics_components.set_registry(&m_registry);
        m_audio_components.set_registry(&m_registry);
    }

    EcsScene(const EcsScene&) = delete;
    EcsScene& operator=(const EcsScene&) = delete;

    // ── Entity lifecycle ────────────────────────────────────────

    EntityId create_entity() { return m_registry.create(); }

    // Deferred destruction — entity is queued and released during the
    // next flush_destroyed_entities() call.  Safe to call during component
    // pool iteration.  Duplicates are handled (alive check in flush).
    void destroy_entity(EntityId entity) {
        m_destroy_queue.push_back(entity);
    }

    // Immediate destruction — bypasses the queue.  NOT safe during
    // component pool iteration (may invalidate the each() cursor).
    void destroy_entity_immediate(EntityId entity) {
        if (!alive(entity)) return;
        m_render_components.remove(entity);
        m_physics_components.remove(entity);
        m_audio_components.remove(entity);
        m_scene_graph.detach(entity);
        m_registry.destroy(entity);
    }

    // Process all deferred destroys.  Call once per frame at a predictable
    // point (e.g. before physics, after the game-logic callback) to amortize
    // destruction cost.  Idempotent: duplicate entries are safely skipped.
    void flush_destroyed_entities() {
        for (auto e : m_destroy_queue) {
            destroy_entity_immediate(e);
        }
        m_destroy_queue.clear();
    }

    bool alive(EntityId entity) const { return m_registry.alive(entity); }
    u32 entity_count() const { return m_registry.count(); }
    u32 pending_destroy_count() const { return static_cast<u32>(m_destroy_queue.size()); }

    // ── Sub-object access ───────────────────────────────────────

    EntityRegistry&       registry()       { return m_registry; }
    const EntityRegistry& registry() const { return m_registry; }

    SceneGraph&       scene_graph()       { return m_scene_graph; }
    const SceneGraph& scene_graph() const { return m_scene_graph; }

    // ── Component access ────────────────────────────────────────

    template <typename T> T*       get_component(EntityId e)       { return pool_for<T>().get(e); }
    template <typename T> const T* get_component(EntityId e) const { return pool_for<T>().get(e); }
    template <typename T> bool     has_component(EntityId e) const { return pool_for<T>().has(e); }
    template <typename T> T&       add_component(EntityId e)       { return pool_for<T>().add(e); }
    template <typename T> void     remove_component(EntityId e)    { pool_for<T>().remove(e); }

    ComponentPool<RenderComponent>&          render_components()       { return m_render_components; }
    const ComponentPool<RenderComponent>&    render_components() const { return m_render_components; }
    ComponentPool<PhysicsComponent>&         physics_components()       { return m_physics_components; }
    const ComponentPool<PhysicsComponent>&   physics_components() const { return m_physics_components; }
    ComponentPool<AudioComponent>&           audio_components()         { return m_audio_components; }
    const ComponentPool<AudioComponent>&     audio_components() const   { return m_audio_components; }

    // ── System dispatch (coordination layer) ────────────────────

    // Full physics step: sync all physics proxies, run CollisionWorld,
    // then write corrected positions back to ECS SceneGraph transforms.
    // Requires the caller to include collision_world.h.
    void update_physics(class CollisionWorld& cw, f32 dt) {
        m_physics_adapter.sync(*this, cw);
        cw.update(dt);
        m_physics_adapter.sync_back(*this);
    }

    // Sync physics proxies only (without running detection/response).
    // Useful when you need custom ordering (e.g. move, sync, detect, resolve).
    void sync_physics(class CollisionWorld& cw) {
        m_physics_adapter.sync(*this, cw);
    }

    // ── ECS-aware physics queries ─────────────────────────────

    // Raycast and return ECS EntityIds directly.
    // hit_entities is populated with pairs of (EntityId, hit fraction along ray).
    void raycast_entities(class CollisionWorld& cw, const Ray& ray,
                           f32 max_distance, u32 layer_mask,
                           std::vector<std::pair<EntityId, f32>>& hits) const {
        hits.clear();
        auto result = cw.raycast(ray, max_distance, layer_mask);
        if (result.entity) {
            EntityId eid = m_physics_adapter.entity_for_proxy(result.entity);
            if (eid) hits.emplace_back(eid, result.t);
        }
    }

    // Overlap AABB and return ECS EntityIds directly.
    std::vector<EntityId> overlap_entities(class CollisionWorld& cw,
                                            const AABB& aabb,
                                            u32 layer_mask) const {
        std::vector<EntityId> result;
        auto proxies = cw.overlap_aabb(aabb, layer_mask);
        for (auto* proxy : proxies) {
            EntityId eid = m_physics_adapter.entity_for_proxy(proxy);
            if (eid) result.push_back(eid);
        }
        return result;
    }

    // Convenience: map a raw tree Entity* (from collision events, raycast, etc.)
    // to its ECS EntityId. Returns NullEntity if not a proxy.
    EntityId entity_for_physics_entity(const Entity* e) const {
        return m_physics_adapter.entity_for_proxy(e);
    }

    // Submit all renderable entities to a RenderQueue.
    // Requires RenderQueue header to be included by the caller.
    template <typename RQ>
    void update_render(RQ& rq) {
        m_render_components.each([&](EntityId e, RenderComponent& rc) {
            if (!rc.enabled) return;
            if (!m_scene_graph.has(e)) return;
            const Mesh* mesh = rc.mesh.get();
            if (!mesh) return;
            glm::mat4 world = m_scene_graph.world_matrix(e);
            RenderCommand cmd;
            cmd.mesh          = mesh;
            cmd.material      = rc.material;
            cmd.model         = world;
            cmd.normal_matrix = glm::mat3(glm::transpose(glm::inverse(world)));
            cmd.transparent   = rc.transparent;
            cmd.has_bounds    = rc.has_bounds;
            cmd.aabb_min      = rc.aabb_min;
            cmd.aabb_max      = rc.aabb_max;
            rq.submit(cmd);
        });
    }

    // Sync 3D audio emitter positions from SceneGraph transforms.
    // Sets the listener once, then updates per-entity emitter positions.
    // Entities with AudioComponent must have a source_id set (from a
    // prior AudioManager::play_3d call) for position tracking.
    void update_audio(class AudioManager& audio) {
        audio_components().each([&](EntityId e, AudioComponent& ac) {
            if (!m_scene_graph.has(e)) return;
            if (ac.source_id == 0) return;
            glm::vec3 pos = m_scene_graph.world_position(e);
            audio.set_position(ac.source_id, pos);
        });
    }

    // ── Clear all ───────────────────────────────────────────────

    void clear() {
        m_destroy_queue.clear();
        m_render_components.clear();
        m_physics_components.clear();
        m_audio_components.clear();
        m_scene_graph.clear();
        m_registry.clear();
        m_physics_adapter.clear();
    }

private:
    template <typename T> auto& pool_for() {
        return pool_for(static_cast<T*>(nullptr));
    }
    template <typename T> const auto& pool_for() const {
        return pool_for(static_cast<T*>(nullptr));
    }

    ComponentPool<RenderComponent>&   pool_for(RenderComponent*)       { return m_render_components; }
    ComponentPool<PhysicsComponent>&  pool_for(PhysicsComponent*)      { return m_physics_components; }
    ComponentPool<AudioComponent>&    pool_for(AudioComponent*)        { return m_audio_components; }
    const ComponentPool<RenderComponent>&   pool_for(RenderComponent*)   const { return m_render_components; }
    const ComponentPool<PhysicsComponent>&  pool_for(PhysicsComponent*)  const { return m_physics_components; }
    const ComponentPool<AudioComponent>&    pool_for(AudioComponent*)    const { return m_audio_components; }

    EntityRegistry                m_registry;
    SceneGraph                    m_scene_graph;
    ComponentPool<RenderComponent>   m_render_components;
    ComponentPool<PhysicsComponent>  m_physics_components;
    ComponentPool<AudioComponent>    m_audio_components;
    EcsPhysicsAdapter              m_physics_adapter;
    std::vector<EntityId>          m_destroy_queue;
};

// ── EcsPhysicsAdapter::sync implementation (needs full EcsScene) ──
inline void EcsPhysicsAdapter::sync_back(EcsScene& scene) {
    auto& sg = scene.scene_graph();
    for (auto& [index, entry] : m_proxies) {
        EntityId eid{index, entry.generation};
        if (!scene.alive(eid) || !sg.has(eid)) continue;
        PhysicsComponent* pc = scene.get_component<PhysicsComponent>(eid);
        if (!pc || pc->is_static) continue;
        sg.set_position(eid, entry.proxy->local_transform().position);
        m_reverse[entry.proxy.get()] = eid;
    }
}


inline void EcsPhysicsAdapter::sync(EcsScene& scene, CollisionWorld& cw) {
    auto& sg = scene.scene_graph();

    // 1. Create/update proxies for active physics entities.
    scene.physics_components().each([&](EntityId e, PhysicsComponent& pc) {
        if (!sg.has(e)) return;

        auto it = m_proxies.find(e.index);

        // Proxy exists and generation matches → sync transform only.
        if (it != m_proxies.end() && it->second.generation == e.generation) {
            glm::mat4 world = sg.world_matrix(e);
            sync_proxy_transform(it->second.proxy.get(), world);
            m_reverse[it->second.proxy.get()] = e;
            return;
        }

        // Stale or missing proxy → create fresh.
        if (it != m_proxies.end()) {
            cw.unregister_collider(*it->second.proxy);
            m_proxies.erase(it);
        }

        auto proxy = std::make_unique<Entity>("phys_proxy");
        proxy->set_user_data((static_cast<uint64_t>(e.index) << 32) | e.generation);
        ColliderComponent cc;
        cc.local_min       = pc.local_min;
        cc.local_max       = pc.local_max;
        cc.is_static       = pc.is_static;
        cc.enabled         = pc.enabled;
        cc.collision_layer = pc.collision_layer;
        cc.collision_mask  = pc.collision_mask;
        cw.register_collider(*proxy, cc);

        sync_proxy_transform(proxy.get(), sg.world_matrix(e));
        m_reverse[proxy.get()] = e;
        m_proxies[e.index] = {std::move(proxy), e.generation};
    });

    // 2. Clean up proxies whose entity is dead or lost the component.
    for (auto it = m_proxies.begin(); it != m_proxies.end(); ) {
        EntityId e{it->first, it->second.generation};
        bool keep = scene.alive(e)
                 && scene.has_component<PhysicsComponent>(e)
                 && sg.has(e);
        if (!keep) {
            cw.unregister_collider(*it->second.proxy);
            m_reverse.erase(it->second.proxy.get());
            it = m_proxies.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace pino
