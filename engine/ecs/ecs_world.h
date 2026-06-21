#pragma once

#include "engine/ecs/ecs_scene.h"
#include "engine/physics/collision_world.h"
#include "engine/renderer/render_queue.h"
#include "engine/audio/audio_manager.h"
#include <functional>
#include <vector>

namespace pino {

// Top-level game world that owns an EcsScene and manages the full
// runtime lifecycle:
//
//   load() → update() per frame → destroy()
//
// Update order (each frame):
//   1. flush deferred destroys
//   2. user callback (game logic)
//   3. physics (CollisionWorld)
//   4. render preparation (RenderQueue)
//   5. audio (AudioManager spatial sync)
//
// Entity destruction is deferred to avoid invalidating component-pool
// iteration during the update callback. destroyed entities are collected
// and flushed at the start of the next frame's update.
//
// All external systems are bound via setters (optional — null systems are
// safely skipped). Future serialization support can inspect
// registry().each() + pool access.
class EcsWorld {
public:
    using UpdateCallback = std::function<void(EcsWorld&, f32 dt)>;

    EcsWorld() = default;
    ~EcsWorld() { destroy(); }

    EcsWorld(const EcsWorld&) = delete;
    EcsWorld& operator=(const EcsWorld&) = delete;

    // ── Lifecycle ────────────────────────────────────────────────

    // One-time setup. Create entities and components here.
    void load();

    // Per-frame update. Safe to call destroy_entity() from the callback
    // — destruction is deferred until the start of the next update.
    void update(f32 dt);

    // Teardown. Destroys all entities and releases resources.
    void destroy();

    // ── Entity lifecycle (deferred destruction) ──────────────────

    EntityId create_entity() { return m_scene.create_entity(); }

    // Deferred — safe to call from any context (including update iteration).
    // The entity stays alive until the next flush (start of update).
    void destroy_entity(EntityId entity) { m_pending_destroy.push_back(entity); }

    // Immediate variant — not safe during the update callback.
    void destroy_entity_immediate(EntityId entity) { m_scene.destroy_entity(entity); }

    bool alive(EntityId entity) const { return m_scene.alive(entity); }
    u32 entity_count() const { return m_scene.entity_count(); }

    // ── Sub-object access ────────────────────────────────────────

    EcsScene&       scene()       { return m_scene; }
    const EcsScene& scene() const { return m_scene; }

    // ── External system binding (optional — null is safely skipped) ─

    void set_collision_world(CollisionWorld* cw) { m_collision_world = cw; }
    void set_render_queue(RenderQueue* rq)        { m_render_queue = rq; }
    void set_audio_manager(AudioManager* am)       { m_audio_manager = am; }

    CollisionWorld* collision_world() const { return m_collision_world; }
    RenderQueue*    render_queue()    const { return m_render_queue; }
    AudioManager*   audio_manager()   const { return m_audio_manager; }

    // ── Update callback ──────────────────────────────────────────

    void set_update_callback(UpdateCallback cb) { m_update_callback = std::move(cb); }

    // ── Utilities ────────────────────────────────────────────────

    // Clear all entities, pending destroys, and reset the scene.
    void clear();

    // Immediately process the deferred destruction queue.
    // Called automatically at the start of update().
    void flush_destroyed();

    // ── Serialization support (future) ───────────────────────────
    // Use registry().each() + component pools for full entity/component
    // enumeration. Create via create_entity() + add_component<T>().
    // For stable IDs across serialization rounds, extend EntityRegistry
    // with a create(EntityId) overload.

private:
    EcsScene m_scene;
    CollisionWorld* m_collision_world = nullptr;
    RenderQueue*    m_render_queue    = nullptr;
    AudioManager*   m_audio_manager   = nullptr;
    UpdateCallback  m_update_callback;
    std::vector<EntityId> m_pending_destroy;
};

// ── Inline implementations ───────────────────────────────────────

inline void EcsWorld::load() {
    flush_destroyed();
}

inline void EcsWorld::update(f32 dt) {
    flush_destroyed();

    if (m_update_callback) {
        m_update_callback(*this, dt);
    }

    if (m_collision_world) {
        m_scene.update_physics(*m_collision_world, dt);
    }

    if (m_render_queue) {
        m_scene.update_render(*m_render_queue);
    }

    if (m_audio_manager) {
        m_scene.update_audio(*m_audio_manager);
    }
}

inline void EcsWorld::destroy() {
    flush_destroyed();
    m_scene.clear();
    m_pending_destroy.clear();
    m_update_callback = nullptr;
}

inline void EcsWorld::clear() {
    m_pending_destroy.clear();
    m_scene.clear();
}

inline void EcsWorld::flush_destroyed() {
    if (m_pending_destroy.empty()) return;
    for (auto e : m_pending_destroy) {
        if (m_scene.alive(e)) {
            m_scene.destroy_entity(e);
        }
    }
    m_pending_destroy.clear();
}

} // namespace pino
