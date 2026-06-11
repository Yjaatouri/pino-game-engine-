#pragma once

#include "engine/core/types.h"
#include "engine/renderer/gl_es3.h"
#include <glm/glm.hpp>

namespace pino {

struct PerFrameData {
    glm::mat4 view_proj;
    glm::vec4 camera_pos;        // .w unused
    glm::vec4 ambient;           // .rgb = color, .a = intensity
    glm::vec4 dir_light_dir;     // direction (normalized), .w unused
    glm::vec4 dir_light_color;   // .w unused
};

class PerFrameUBO {
public:
    PerFrameUBO();
    ~PerFrameUBO();

    PerFrameUBO(const PerFrameUBO&) = delete;
    PerFrameUBO& operator=(const PerFrameUBO&) = delete;

    void update(const PerFrameData& data);
    void bind(u32 binding_point = 0);

private:
    GLuint m_buffer = 0;
    static constexpr u32 BLOCK_SIZE = 128;
};

} // namespace pino
