#pragma once

#include "engine/core/types.h"
#include "engine/renderer/gl_es3.h"
#include "engine/renderer/shader.h"
#include <glm/glm.hpp>
#include <vector>

namespace pino {

struct DebugLine {
    glm::vec3 a, b;
    glm::vec4 color;
};

struct DebugPoint {
    glm::vec3 position;
    glm::vec4 color;
    f32 size;
};

class DebugRenderer {
public:
    DebugRenderer() = default;
    ~DebugRenderer();

    DebugRenderer(const DebugRenderer&) = delete;
    DebugRenderer& operator=(const DebugRenderer&) = delete;

    bool init();

    void draw_line(const glm::vec3& from, const glm::vec3& to,
                   const glm::vec4& color = {1,1,1,1});
    void draw_box(const glm::vec3& center, const glm::vec3& half_extents,
                  const glm::vec4& color = {0,1,0,1});
    void draw_sphere(const glm::vec3& center, f32 radius,
                     const glm::vec4& color = {1,1,1,1}, i32 segments = 16);
    void draw_axes(const glm::mat4& transform, f32 length = 1.0f);

    void begin_frame();
    void render(const glm::mat4& view_proj);
    void end_frame();
    void destroy();

    bool is_valid() const { return m_vao != 0; }

private:
    void flush_lines(const glm::mat4& view_proj);
    void flush_points(const glm::mat4& view_proj);

    Shader  m_line_shader;
    Shader  m_point_shader;
    GLuint  m_vao = 0;
    GLuint  m_vbo = 0;

    std::vector<DebugLine>  m_lines;
    std::vector<DebugPoint> m_points;
    static constexpr u32 MAX_VERTS = 65536;
};

} // namespace pino
