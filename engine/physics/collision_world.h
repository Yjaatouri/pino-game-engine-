#pragma once
#include "engine/physics/aabb.h"
#include "engine/physics/collider_component.h"
#include "engine/physics/uniform_grid.h"
#include "engine/physics/loose_uniform_grid.h"
#include "engine/physics/sweep_and_prune.h"
#include "engine/physics/collision_stats.h"
#include "engine/scene/entity.h"
#include "engine/core/event_bus.h"
#include "engine/core/math_utils.h"
#include <vector>

namespace pino {

enum class BroadPhaseMode {
    BruteForce,     // O(n²) baseline
    UniformGrid,    // default sparse uniform grid
    LooseGrid,      // uniform grid with larger cells (3x)
    SweepAndPrune   // sort + sweep along X axis
};

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

    // Broad-phase selection
    void set_broad_phase_mode(BroadPhaseMode mode);
    BroadPhaseMode broad_phase_mode() const { return m_broad_phase; }

    // Register/unregister a collider for a given entity
    void register_collider(Entity& entity, const ColliderComponent& component);
    void unregister_collider(Entity& entity);

    // Remove all colliders
    void clear();

    // Sub-steps (exposed for custom ordering, e.g. movement between detetion and resolution)
    void update_aabbs();
    void detect_collisions();   // broad-phase + narrow-phase, populates overlap list
    void dispatch_events();     // emit CollisionEnter/Stay/Exit
    void resolve_collisions();  // push-out

    // Full collision step: update_aabbs + detect + dispatch + resolve
    void update(f32 dt);

    // Queries (brute-force against all colliders)
    RaycastResult raycast(const Ray& ray, f32 max_distance,
                          u32 layer_mask = 0xFFFFFFFF) const;
    std::vector<Entity*> overlap_aabb(const AABB& aabb,
                                       u32 layer_mask = 0xFFFFFFFF) const;

    // Debug
    bool show_debug = false;
    const std::vector<AABB>& debug_aabbs() const { return m_debug_aabbs; }

    // Auto-size the uniform grid based on average collider extent.
    // Call once after registering all colliders (or when distribution changes).
    void auto_size_grid(f32 multiplier = 2.0f);

    // Profiling
    CollisionStats stats;

    u32 collider_count() const { return static_cast<u32>(m_colliders.size()); }

private:
struct ColliderEntry {
    Entity*           entity = nullptr;
    ColliderComponent component;
    bool              moved = true;  // true when AABB needs refresh
};

    // Emit both Enter and Exit events (Stay is emitted separately)
    void emit_enter(Entity& a, Entity& b) const;
    void emit_exit(Entity& a, Entity& b) const;

    // Test a single pair for overlap (layer/mask + AABB)
    static bool test_pair(const ColliderEntry& a, const ColliderEntry& b);

    // Collect grid diagnostics into stats
    void collect_grid_diagnostics();

    std::vector<ColliderEntry> m_colliders;
    std::vector<AABB>          m_debug_aabbs;
    EventBus::HandlerId        m_destroy_handle = 0;

    BroadPhaseMode   m_broad_phase = BroadPhaseMode::UniformGrid;
    UniformGrid      m_grid;
    LooseUniformGrid m_loose_grid;
    SweepAndPrune    m_sap;

    // Pair state for enter/stay/exit — kept sorted and uniqued
    std::vector<u64> m_current_pairs;  // this frame's overlapping pair IDs
    std::vector<u64> m_prev_pairs;     // last frame's overlapping pair IDs
    std::vector<u64> m_pair_scratch;   // reusable scratch buffer
};

} // namespace pino
