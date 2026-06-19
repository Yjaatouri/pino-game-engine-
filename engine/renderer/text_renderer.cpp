#include "text_renderer.h"
#include "engine/core/log.h"

namespace pino {

TextRenderer::~TextRenderer() { destroy(); }

bool TextRenderer::init() {
    return true;
}

void TextRenderer::destroy() {
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_ibo) glDeleteBuffers(1, &m_ibo);
    m_vao = m_vbo = m_ibo = 0;
    m_shader.destroy();
}

void TextRenderer::begin_frame() {
}

void TextRenderer::end_frame() {
}

} // namespace pino
