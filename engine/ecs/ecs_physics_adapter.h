#pragma once

#include "engine/ecs/entity.h"
#include "engine/physics/collision_world.h"
#include "engine/scene/entity.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <unordered_map>

namespace pino {

class EcsScene; // forward decl

// Bridges ECS entities (with PhysicsComponent + SceneGraph transform)
// to the old tree-based CollisionWorld.
//
// Usage:
//   EcsScene scene;
//   CollisionWorld cw;
//   EcsPhysicsAdapter phys;
//
//   // Each frame:
//   phys.sync(scene, cw);
//   cw.update(dt);
class EcsPhysicsAdapter {
public:
    EcsPhysicsAdapter() = default;
    ~EcsPhysicsAdapter() { clear(); }

    EcsPhysicsAdapter(const EcsPhysicsAdapter&) = delete;
    EcsPhysicsAdapter& operator=(const EcsPhysicsAdapter&) = delete;

    // Synchronise all physics proxies with the current ECS state.
    // Auto-creates/destroys proxies and syncs transforms.
    // Safe to call every frame — no allocations on steady state.
    void sync(EcsScene& scene, CollisionWorld& cw);

    // Write corrected proxy positions back to ECS SceneGraph transforms.
    // Call AFTER CollisionWorld::update() to capture collision resolution.
    void sync_back(EcsScene& scene);   // implemented in ecs_scene.h
    void clear() { m_proxies.clear(); }
    u32 proxy_count() const { return static_cast<u32>(m_proxies.size()); }

private:
    struct ProxyEntry {
        std::unique_ptr<Entity> proxy;
        u32 generation = 0;
    };
    static void sync_proxy_transform(Entity* proxy, const glm::mat4& world);
    std::unordered_map<u32, ProxyEntry> m_proxies;
};

inline void EcsPhysicsAdapter::sync_proxy_transform(Entity* proxy, const glm::mat4& world) {
    auto& t = proxy->local_transform();
    t.position = glm::vec3(world[3]);
    glm::vec3 scl;
    scl.x = glm::length(glm::vec3(world[0]));
    scl.y = glm::length(glm::vec3(world[1]));
    scl.z = glm::length(glm::vec3(world[2]));
    t.scale = scl;
    if (scl.x > 0.0f && scl.y > 0.0f && scl.z > 0.0f) {
        glm::mat3 rot_mat(
            glm::vec3(world[0]) / scl.x,
            glm::vec3(world[1]) / scl.y,
            glm::vec3(world[2]) / scl.z
        );
        t.rotation = glm::quat_cast(rot_mat);
    }
}

} // namespace pino
