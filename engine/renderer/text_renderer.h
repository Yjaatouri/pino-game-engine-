#pragma once

#include "engine/core/types.h"
#include "engine/renderer/gl_es3.h"
#include "engine/renderer/shader.h"
#include "engine/renderer/font.h"

namespace pino {

class TextRenderer {
public:
    TextRenderer() = default;
    ~TextRenderer();

    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;

    bool init();
    void destroy();

    void begin_frame();
    void end_frame();

    bool is_valid() const { return m_vao != 0; }

private:
    Shader  m_shader;
    GLuint  m_vao = 0;
    GLuint  m_vbo = 0;
    GLuint  m_ibo = 0;
};

} // namespace pino
