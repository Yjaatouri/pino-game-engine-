#include "per_frame_ubo.h"

namespace pino {

PerFrameUBO::PerFrameUBO() {
    glGenBuffers(1, &m_buffer);
    glBindBuffer(GL_UNIFORM_BUFFER, m_buffer);
    glBufferData(GL_UNIFORM_BUFFER, BLOCK_SIZE, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

PerFrameUBO::~PerFrameUBO() {
    if (m_buffer) glDeleteBuffers(1, &m_buffer);
}

void PerFrameUBO::update(const PerFrameData& data) {
    glBindBuffer(GL_UNIFORM_BUFFER, m_buffer);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, BLOCK_SIZE, &data);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void PerFrameUBO::bind(u32 binding_point) {
    glBindBufferBase(GL_UNIFORM_BUFFER, binding_point, m_buffer);
}

} // namespace pino
