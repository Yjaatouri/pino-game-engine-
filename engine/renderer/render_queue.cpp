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
    u32 i = 0;
    const u32 N = static_cast<u32>(m_commands.size());

    while (i < N) {
        const auto& first = m_commands[i];

        // Already-instanced or transparent commands: draw individually
        if (first.instanced || first.transparent) {
            const Mesh&     mesh     = *first.mesh;
            const Material& material = *first.material;

            if (&material != prev_material) {
                material.apply();
                prev_material = &material;
                RenderStats::instance().add_state_change();
            }

            Shader* shader = material.shader().get();
            shader->set_int("u_instanced", first.instanced ? 1 : 0);
            shader->set_mat4("u_model", first.model);
            shader->set_mat3("u_normal_matrix", first.normal_matrix);

            if (first.instance_count > 0)
                mesh.draw_instanced(first.instance_count);
            else
                mesh.draw();

            ++i;
            continue;
        }

        // Find run of consecutive commands with the same mesh+material
        u32 run_start = i;
        const Mesh*     run_mesh     = first.mesh;
        const Material* run_material = first.material;

        ++i;
        while (i < N &&
               m_commands[i].mesh     == run_mesh &&
               m_commands[i].material == run_material &&
               !m_commands[i].instanced &&
               !m_commands[i].transparent)
        {
            ++i;
        }
        u32 run_count = i - run_start;

        // Bind material
        if (run_material != prev_material) {
            run_material->apply();
            prev_material = run_material;
            RenderStats::instance().add_state_change();
        }

        Shader* shader = run_material->shader().get();

        if (run_count > 1) {
            // Batch: pack model matrices into scratch buffer, draw instanced
            m_instance_scratch.clear();
            m_instance_scratch.reserve(run_count);
            for (u32 j = 0; j < run_count; ++j) {
                m_instance_scratch.push_back(m_commands[run_start + j].model);
            }

            shader->set_int("u_instanced", 1);
            // Normal matrix approximated for batch; pass identity so
            // instance vertices use object-space normals (the shader
            // applies u_normal_matrix on the interpolated a_normal).
            shader->set_mat3("u_normal_matrix", glm::mat3(1.0f));

            const_cast<Mesh*>(run_mesh)->set_instance_data(
                m_instance_scratch.data(), run_count);
            run_mesh->draw_instanced(run_count);
        } else {
            // Single draw
            const auto& cmd = m_commands[run_start];
            shader->set_int("u_instanced", 0);
            shader->set_mat4("u_model", cmd.model);
            shader->set_mat3("u_normal_matrix", cmd.normal_matrix);

            if (cmd.instance_count > 0)
                cmd.mesh->draw_instanced(cmd.instance_count);
            else
                cmd.mesh->draw();
        }
    }
}

void RenderQueue::clear() {
    m_commands.clear();
}

} // namespace pino
