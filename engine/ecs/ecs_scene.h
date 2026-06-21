#pragma once

#include "engine/ecs/entity.h"
#include "engine/ecs/entity_registry.h"
#include "engine/ecs/entity_handle.h"
#include "engine/ecs/scene_graph.h"
#include "engine/ecs/component_pool.h"
#include "engine/ecs/components.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace pino {

// Aggregate that owns an EntityRegistry, SceneGraph, and typed component pools.
// Provides a unified API for creating/destroying entities and working with
// components and transforms.
//
// Integration helpers:
//   submit_renderables(RenderQueue&) — submits RenderCommands for all entities
//     that have both a RenderComponent and a transform.
//
// For CollisionWorld integration, use EcsPhysicsAdapter (ecs_physics_adapter.h).
// For AudioManager integration, iterate audio_components() + scene_graph.
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

    // Non-copyable.
    EcsScene(const EcsScene&) = delete;
    EcsScene& operator=(const EcsScene&) = delete;

    // ── Entity lifecycle ────────────────────────────────────────

    EntityId create_entity() { return m_registry.create(); }

    void destroy_entity(EntityId entity) {
        m_render_components.remove(entity);
        m_physics_components.remove(entity);
        m_audio_components.remove(entity);
        m_scene_graph.detach(entity);
        m_registry.destroy(entity);
    }

    bool alive(EntityId entity) const { return m_registry.alive(entity); }
    u32 entity_count() const { return m_registry.count(); }

    // ── Sub-object access ───────────────────────────────────────

    EntityRegistry&       registry()       { return m_registry; }
    const EntityRegistry& registry() const { return m_registry; }

    SceneGraph&       scene_graph()       { return m_scene_graph; }
    const SceneGraph& scene_graph() const { return m_scene_graph; }

    // ── Component access ────────────────────────────────────────

    template <typename T>
    T* get_component(EntityId entity) { return pool_for<T>().get(entity); }

    template <typename T>
    const T* get_component(EntityId entity) const { return pool_for<T>().get(entity); }

    template <typename T>
    bool has_component(EntityId entity) const { return pool_for<T>().has(entity); }

    template <typename T>
    T& add_component(EntityId entity) { return pool_for<T>().add(entity); }

    template <typename T>
    void remove_component(EntityId entity) { pool_for<T>().remove(entity); }

    // Direct pool access for iteration.
    ComponentPool<RenderComponent>&   render_components()       { return m_render_components; }
    const ComponentPool<RenderComponent>& render_components() const { return m_render_components; }

    ComponentPool<PhysicsComponent>&  physics_components()       { return m_physics_components; }
    const ComponentPool<PhysicsComponent>& physics_components() const { return m_physics_components; }

    ComponentPool<AudioComponent>&    audio_components()         { return m_audio_components; }
    const ComponentPool<AudioComponent>& audio_components() const { return m_audio_components; }

    // ── Integration helpers ─────────────────────────────────────

    // Submit all renderable entities to a RenderQueue.
    // Requires the RenderQueue header to be included by the caller.
    // Entity must have both a RenderComponent and a transform (via SceneGraph).
    template <typename RenderQueue>
    void submit_renderables(RenderQueue& rq) {
        m_render_components.each([&](EntityId e, RenderComponent& rc) {
            if (!m_scene_graph.has(e)) return;
            const glm::mat4& world = m_scene_graph.world_matrix(e);
            typename RenderQueue::RenderCommand cmd;
            cmd.mesh        = rc.mesh;
            cmd.material    = rc.material;
            cmd.model       = world;
            cmd.normal_matrix = glm::mat3(glm::transpose(glm::inverse(world)));
            cmd.transparent = rc.transparent;
            cmd.has_bounds  = rc.has_bounds;
            cmd.aabb_min    = rc.aabb_min;
            cmd.aabb_max    = rc.aabb_max;
            rq.submit(cmd);
        });
    }

    // ── Clear all ───────────────────────────────────────────────
    void clear() {
        m_render_components.clear();
        m_physics_components.clear();
        m_audio_components.clear();
        m_scene_graph.clear();
        m_registry.clear();
    }

private:
    // Dispatch helpers for template methods.
    ComponentPool<RenderComponent>&   pool_for(RenderComponent*)       { return m_render_components; }
    ComponentPool<PhysicsComponent>&  pool_for(PhysicsComponent*)      { return m_physics_components; }
    ComponentPool<AudioComponent>&    pool_for(AudioComponent*)        { return m_audio_components; }

    const ComponentPool<RenderComponent>&   pool_for(RenderComponent*)   const { return m_render_components; }
    const ComponentPool<PhysicsComponent>&  pool_for(PhysicsComponent*)  const { return m_physics_components; }
    const ComponentPool<AudioComponent>&    pool_for(AudioComponent*)    const { return m_audio_components; }

    template <typename T>
    auto& pool_for() {
        return pool_for(static_cast<T*>(nullptr));
    }

    template <typename T>
    const auto& pool_for() const {
        return pool_for(static_cast<T*>(nullptr));
    }

    EntityRegistry               m_registry;
    SceneGraph                   m_scene_graph;
    ComponentPool<RenderComponent>  m_render_components;
    ComponentPool<PhysicsComponent> m_physics_components;
    ComponentPool<AudioComponent>   m_audio_components;
};

} // namespace pino
