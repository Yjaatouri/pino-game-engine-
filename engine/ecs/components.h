#pragma once

#include "engine/core/types.h"
#include "engine/core/asset_handle.h"
#include "engine/physics/collider_component.h"
#include <string>
#include <glm/glm.hpp>

namespace pino {

class Mesh;
class Material;

// ── RenderComponent ─────────────────────────────────────────────
// Attach a visible mesh + material to an entity.
struct RenderComponent {
    AssetHandle<Mesh> mesh;
    const Material*   material   = nullptr;
    bool              transparent = false;
    bool              enabled    = true;

    // Optional local-space AABB for frustum culling in RenderQueue.
    bool      has_bounds = false;
    glm::vec3 aabb_min   = {0,0,0};
    glm::vec3 aabb_max   = {0,0,0};
};

// ── PhysicsComponent ────────────────────────────────────────────
// Collision shape and filtering parameters.
// Compatible with ColliderComponent (same field layout for local AABB).
struct PhysicsComponent {
    glm::vec3 local_min       = {-0.5f, -0.5f, -0.5f};
    glm::vec3 local_max       = { 0.5f,  0.5f,  0.5f};
    bool      is_static       = false;
    bool      enabled         = true;
    u32       collision_layer = 1;
    u32       collision_mask  = 1;
    glm::vec3 velocity        = {0.0f, 0.0f, 0.0f};
};

// ── AudioComponent ──────────────────────────────────────────────
// Spatial sound emitter attached to an entity.
struct AudioComponent {
    std::string sound_path;       // asset path for the sound
    float       volume    = 1.0f;
    bool        looping   = false;
    bool        spatial   = true;

    // Attenuation (spatial only)
    i32   attenuation_model = 2;  // 0=None 1=Inverse 2=Linear 3=Exponential
    float min_dist          = 1.0f;
    float max_dist          = 50.0f;
    float rolloff           = 1.0f;

    // Runtime handle (managed by audio system, not serialized).
    u64 source_id = 0;
};

} // namespace pino
