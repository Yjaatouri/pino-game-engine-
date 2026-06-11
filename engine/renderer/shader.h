#pragma once

#include "engine/core/types.h"
#include "engine/renderer/gl_es3.h"
#include <string>
#include <unordered_map>

#include <glm/glm.hpp>

namespace pino {

class Shader {
public:
    Shader() = default;

    bool load(const char* vert_src, const char* frag_src);
    void destroy();

    void bind()   const;
    void unbind() const;

    bool is_valid() const { return m_program != 0; }

    // Uniforms
    void set_int(const char* name, i32 v);
    void set_float(const char* name, f32 v);
    void set_vec3(const char* name, const glm::vec3& v);
    void set_vec4(const char* name, const glm::vec4& v);
    void set_mat3(const char* name, const glm::mat3& m);
    void set_mat4(const char* name, const glm::mat4& m);

    GLuint handle() const { return m_program; }

private:
    GLint uniform_location(const char* name);

    GLuint m_program = 0;
    std::unordered_map<std::string, GLint> m_uniform_cache;
};

} // namespace pino
