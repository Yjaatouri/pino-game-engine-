#pragma once

#include "engine/core/types.h"
#include "engine/renderer/material.h"
#include "engine/renderer/mesh.h"

#include <vector>
#include <glm/glm.hpp>

namespace pino {

struct RenderCommand {
    const Mesh*     mesh           = nullptr;
    const Material* material       = nullptr;
    glm::mat4       model          = glm::mat4(1.0f);
    glm::mat3       normal_matrix  = glm::mat3(1.0f);
    bool            transparent    = false;
    float           depth          = 0.0f;   // camera distance (for transparent sorting)
    bool            instanced      = false;
    u32             instance_count = 0;       // 0 = regular draw, >0 = draw_instanced
};

class RenderQueue {
public:
    void submit(const RenderCommand& cmd);
    void sort();
    void flush();
    void clear();

    u32 command_count() const { return static_cast<u32>(m_commands.size()); }

private:
    std::vector<RenderCommand> m_commands;
};

} // namespace pino
