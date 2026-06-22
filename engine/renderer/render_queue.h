#pragma once

#include "engine/core/types.h"
#include "engine/renderer/material.h"
#include "engine/renderer/mesh.h"

#include <vector>
#include <glm/glm.hpp>

namespace pino {

class Frustum;

struct RenderCommand {
    const Mesh*     mesh           = nullptr;
    const Material* material       = nullptr;
    glm::mat4       model          = glm::mat4(1.0f);
    glm::mat3       normal_matrix  = glm::mat3(1.0f);
    bool            transparent    = false;
    float           depth          = 0.0f;   // camera distance (for transparent sorting)
    bool            instanced      = false;
    u32             instance_count = 0;       // 0 = regular draw, >0 = draw_instanced
    bool            has_bounds     = false;   // set when aabb_min/aabb_max are valid
    glm::vec3       aabb_min       = glm::vec3(0.0f);
    glm::vec3       aabb_max       = glm::vec3(0.0f);
};

class RenderQueue {
public:
    void submit(const RenderCommand& cmd);
    void sort();
    void cull(const Frustum& frustum);
    void flush();
    void clear();

    u32 command_count() const { return static_cast<u32>(m_commands.size()); }

private:
    std::vector<RenderCommand> m_commands;
    std::vector<glm::mat4> m_instance_scratch;
};

} // namespace pino
