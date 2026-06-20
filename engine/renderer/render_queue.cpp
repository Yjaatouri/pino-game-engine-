#include "engine/renderer/render_queue.h"
#include "engine/renderer/frustum.h"
#include "engine/renderer/render_stats.h"

#include <algorithm>

namespace pino {

void RenderQueue::submit(const RenderCommand& cmd) {
    m_commands.push_back(cmd);
}

void RenderQueue::sort() {
    std::sort(m_commands.begin(), m_commands.end(),
        [](const RenderCommand& a, const RenderCommand& b) {
            // 1) Opaque before transparent
            if (a.transparent != b.transparent)
                return !a.transparent;

            // 2) Batch by material pointer (groups same shader + uniforms)
            if (a.material != b.material)
                return a.material < b.material;

            // 3) For transparent: back-to-front (farthest first)
            if (a.transparent)
                return a.depth > b.depth;

            // 4) Static draw order for opaque with same material
            return false;
        });
}

void RenderQueue::cull(const Frustum& frustum) {
    m_commands.erase(
        std::remove_if(m_commands.begin(), m_commands.end(),
            [&](const RenderCommand& cmd) {
                return cmd.has_bounds && !frustum.intersects(cmd.aabb_min, cmd.aabb_max);
            }),
        m_commands.end());
}

void RenderQueue::flush() {
    const Material* prev_material = nullptr;

    for (const auto& cmd : m_commands) {
        const Mesh&     mesh     = *cmd.mesh;
        const Material& material = *cmd.material;

        // Bind material if changed (shader + uniforms + textures)
        if (&material != prev_material) {
            material.apply();
            prev_material = &material;
            RenderStats::instance().add_state_change();
        }

        // Upload per-command uniforms
        Shader* shader = material.shader().get();
        shader->set_int("u_instanced", cmd.instanced ? 1 : 0);
        shader->set_mat4("u_model", cmd.model);
        shader->set_mat3("u_normal_matrix", cmd.normal_matrix);

        // Draw
        if (cmd.instance_count > 0) {
            mesh.draw_instanced(cmd.instance_count);
        } else {
            mesh.draw();
        }
    }
}

void RenderQueue::clear() {
    m_commands.clear();
}

} // namespace pino
