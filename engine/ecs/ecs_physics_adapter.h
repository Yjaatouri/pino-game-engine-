#pragma once

#include "engine/ecs/entity.h"
#include "engine/ecs/ecs_scene.h"
#include "engine/physics/collision_world.h"
#include "engine/scene/entity.h"
#include <vector>
#include <unordered_map>

namespace pino {

// Bridges ECS entities (with PhysicsComponent + SceneGraph transform)
// to the old tree-based CollisionWorld.
//
// Each ECS entity that should participate in physics gets a thin
// old-Entity proxy. The adapter syncs transforms from SceneGraph
// into the proxy every frame so CollisionWorld's update_aabbs()
// sees the correct world matrix.
//
// Usage:
//   EcsScene scene;
//   CollisionWorld cw;
//   EcsPhysicsAdapter phys(&scene, &cw);
//
//   auto e = scene.create_entity();
//   scene.scene_graph().attach(e);
//   phys.attach(e);   // creates proxy + registers collider
//
//   // Each frame:
//   phys.sync();  // copies SceneGraph transforms → old Entity → CollisionWorld
class EcsPhysicsAdapter {
public:
    EcsPhysicsAdapter(EcsScene* scene, CollisionWorld* cw)
        : m_scene(scene), m_cw(cw) {}

    ~EcsPhysicsAdapter() { detach_all(); }

    EcsPhysicsAdapter(const EcsPhysicsAdapter&) = delete;
    EcsPhysicsAdapter& operator=(const EcsPhysicsAdapter&) = delete;

    // Attach an entity to physics. The entity must already have a
    // PhysicsComponent and a transform (via SceneGraph). Creates an
    // old-Entity proxy and registers a collider in CollisionWorld.
    void attach(EntityId entity) {
        if (!m_scene || !m_cw) return;
        if (m_proxies.find(entity.index) != m_proxies.end()) return;

        auto* pc = m_scene->get_component<PhysicsComponent>(entity);
        if (!pc) return;

        auto proxy = std::make_unique<Entity>("phys_proxy");

        ColliderComponent cc;
        cc.local_min       = pc->local_min;
        cc.local_max       = pc->local_max;
        cc.is_static       = pc->is_static;
        cc.enabled         = pc->enabled;
        cc.collision_layer = pc->collision_layer;
        cc.collision_mask  = pc->collision_mask;

        m_cw->register_collider(*proxy, cc);
        m_proxies[entity.index] = {std::move(proxy), entity.generation};
    }

    // Detach an entity from physics (unregisters collider, destroys proxy).
    void detach(EntityId entity) {
        auto it = m_proxies.find(entity.index);
        if (it == m_proxies.end()) return;
        if (it->second.generation != entity.generation) return;

        m_cw->unregister_collider(*it->second.proxy);
        m_proxies.erase(it);
    }

    // Sync all transforms: copies SceneGraph world transform into
    // each proxy's local transform so CollisionWorld sees the right matrix.
    void sync() {
        if (!m_scene) return;
        auto& sg = m_scene->scene_graph();

        for (auto& kv : m_proxies) {
            EntityId e{kv.first, kv.second.generation};
            if (!m_scene->alive(e)) {
                // Entity was destroyed externally — clean up.
                m_cw->unregister_collider(*kv.second.proxy);
                // Mark for removal (safe during iteration by deferring).
                kv.second.generation = 0xFFFFFFFF;
                continue;
            }
            if (!sg.has(e)) continue;

            glm::mat4 world = sg.world_matrix(e);
            // Decompose into proxy's local transform.
            auto& t = kv.second.proxy->local_transform();
            t.position = glm::vec3(world[3]);
            // Extract scale.
            glm::vec3 scl;
            scl.x = glm::length(glm::vec3(world[0]));
            scl.y = glm::length(glm::vec3(world[1]));
            scl.z = glm::length(glm::vec3(world[2]));
            t.scale = scl;
            // Extract rotation (orthonormalize scale out).
            glm::mat3 rot_mat(
                glm::vec3(world[0]) / scl.x,
                glm::vec3(world[1]) / scl.y,
                glm::vec3(world[2]) / scl.z
            );
            t.rotation = glm::quat_cast(rot_mat);
        }

        // Remove entries that were invalidated above.
        for (auto it = m_proxies.begin(); it != m_proxies.end(); ) {
            if (it->second.generation == 0xFFFFFFFF)
                it = m_proxies.erase(it);
            else
                ++it;
        }
    }

    u32 proxy_count() const { return static_cast<u32>(m_proxies.size()); }

    void detach_all() {
        for (auto& kv : m_proxies) {
            m_cw->unregister_collider(*kv.second.proxy);
        }
        m_proxies.clear();
    }

private:
    struct ProxyEntry {
        std::unique_ptr<Entity> proxy;
        u32 generation = 0;
    };

    EcsScene*     m_scene;
    CollisionWorld* m_cw;
    std::unordered_map<u32, ProxyEntry> m_proxies; // entity.index → proxy
};

} // namespace pino
