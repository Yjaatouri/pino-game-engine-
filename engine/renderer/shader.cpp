#include "shader.h"
#include "engine/core/log.h"
#include "engine/renderer/render_stats.h"
#include <glm/gtc/type_ptr.hpp>

namespace pino {

Shader::~Shader() { destroy(); }

Shader::Shader(Shader&& other) noexcept
    : m_program(other.m_program), m_uniform_cache(std::move(other.m_uniform_cache))
{
    other.m_program = 0;
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        destroy();
        m_program = other.m_program;
        m_uniform_cache = std::move(other.m_uniform_cache);
        other.m_program = 0;
    }
    return *this;
}

static GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        PINO_ERROR("Shader compile (%s): %s",
                   type == GL_VERTEX_SHADER ? "vert" : "frag", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

bool Shader::load(const char* vert_src, const char* frag_src) {
    destroy();

    GLuint vs = compile(GL_VERTEX_SHADER, vert_src);
    GLuint fs = compile(GL_FRAGMENT_SHADER, frag_src);
    if (!vs || !fs) {
        glDeleteShader(vs);
        glDeleteShader(fs);
        return false;
    }

    m_program = glCreateProgram();
    glAttachShader(m_program, vs);
    glAttachShader(m_program, fs);
    glLinkProgram(m_program);

    GLint ok = 0;
    glGetProgramiv(m_program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(m_program, sizeof(log), nullptr, log);
        PINO_ERROR("Program link: %s", log);
        glDeleteProgram(m_program);
        m_program = 0;
        return false;
    }

    glDetachShader(m_program, vs);
    glDetachShader(m_program, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    return true;
}

void Shader::destroy() {
    if (m_program) {
        glDeleteProgram(m_program);
        m_program = 0;
    }
    m_uniform_cache.clear();
}

void Shader::bind() const {
    GLint current = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &current);
    if (static_cast<GLuint>(current) != m_program) {
        RenderStats::instance().add_shader_switch();
        glUseProgram(m_program);
    }
}

void Shader::unbind() const {
    glUseProgram(0);
}

GLint Shader::uniform_location(const char* name) {
    auto it = m_uniform_cache.find(name);
    if (it != m_uniform_cache.end())
        return it->second;
    GLint loc = glGetUniformLocation(m_program, name);
    m_uniform_cache[name] = loc;
    return loc;
}

void Shader::set_int(const char* name, i32 v) {
    glUniform1i(uniform_location(name), v);
}

void Shader::set_float(const char* name, f32 v) {
    glUniform1f(uniform_location(name), v);
}

void Shader::set_vec3(const char* name, const glm::vec3& v) {
    glUniform3fv(uniform_location(name), 1, glm::value_ptr(v));
}

void Shader::set_vec4(const char* name, const glm::vec4& v) {
    glUniform4fv(uniform_location(name), 1, glm::value_ptr(v));
}

void Shader::set_mat3(const char* name, const glm::mat3& m) {
    glUniformMatrix3fv(uniform_location(name), 1, GL_FALSE, glm::value_ptr(m));
}

void Shader::set_mat4(const char* name, const glm::mat4& m) {
    glUniformMatrix4fv(uniform_location(name), 1, GL_FALSE, glm::value_ptr(m));
}

} // namespace pino
