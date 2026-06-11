#pragma once
#include "engine/core/types.h"
#include "engine/renderer/gl_es3.h"
#include "engine/renderer/shader.h"
#include "engine/renderer/camera.h"
#include "engine/physics/aabb.h"
#include <vector>

namespace pino {

class DebugDraw {
public:
    DebugDraw();
    ~DebugDraw();

    DebugDraw(const DebugDraw&) = delete;
    DebugDraw& operator=(const DebugDraw&) = delete;

    DebugDraw(DebugDraw&&) = delete;
    DebugDraw& operator=(DebugDraw&&) = delete;

    bool init();
    void draw_aabbs(const std::vector<AABB>& aabbs, const Camera& camera);
    void destroy();

private:
    Shader  m_shader;
    GLuint  m_vao = 0;
    GLuint  m_vbo = 0;
};

} // namespace pino