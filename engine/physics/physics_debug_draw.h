#pragma once

#include "engine/core/types.h"
#include "engine/physics/collision_world.h"
#include "engine/renderer/debug_renderer.h"
#include "engine/platform/input.h"
#include <glm/glm.hpp>

namespace pino {

// Runtime-togglable physics debug visualization that renders AABBs,
// active collision pairs, broad-phase grid cells, and velocity vectors
// via DebugRenderer.  Has zero impact on physics simulation.
class PhysicsDebugDraw {
public:
    PhysicsDebugDraw();
    ~PhysicsDebugDraw() = default;

    PhysicsDebugDraw(const PhysicsDebugDraw&) = delete;
    PhysicsDebugDraw& operator=(const PhysicsDebugDraw&) = delete;

    // ── Toggle flags ────────────────────────────────────────────
    void set_show_aabbs(bool v)  { m_show_aabbs = v; }
    void set_show_pairs(bool v)  { m_show_pairs = v; }
    void set_show_grid(bool v)   { m_show_grid = v; }
    void set_show_vel(bool v)    { m_show_vel = v; }

    void toggle_aabbs()  { m_show_aabbs = !m_show_aabbs; }
    void toggle_pairs()  { m_show_pairs = !m_show_pairs; }
    void toggle_grid()   { m_show_grid = !m_show_grid; }
    void toggle_vel()    { m_show_vel = !m_show_vel; }

    bool show_aabbs()  const { return m_show_aabbs; }
    bool show_pairs()  const { return m_show_pairs; }
    bool show_grid()   const { return m_show_grid; }
    bool show_vel()    const { return m_show_vel; }

    // Handle keyboard input (F5=aabbs, F6=pairs, F7=grid, F8=vel).
    // Returns true if any toggle changed.
    bool handle_input(Input& input);

    // Render all enabled debug visualizations.
    // view_proj: camera view-projection matrix.
    // scene: optional EcsScene for component-level data (velocity).
    void render(CollisionWorld& cw, DebugRenderer& dr,
                const glm::mat4& view_proj,
                class EcsScene* scene = nullptr);

private:
    void draw_aabbs(CollisionWorld& cw, DebugRenderer& dr,
                    const glm::mat4& view_proj);
    void draw_pairs(CollisionWorld& cw, DebugRenderer& dr,
                    const glm::mat4& view_proj);
    void draw_grid(CollisionWorld& cw, DebugRenderer& dr,
                   const glm::mat4& view_proj);
    void draw_velocities(DebugRenderer& dr,
                         const glm::mat4& view_proj,
                         class EcsScene* scene);

    bool m_show_aabbs  = false;
    bool m_show_pairs  = false;
    bool m_show_grid   = false;
    bool m_show_vel    = false;
};

} // namespace pino
