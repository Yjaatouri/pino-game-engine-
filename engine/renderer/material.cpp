#include "engine/renderer/material.h"

namespace pino {

void Material::set_shader(AssetHandle<Shader> shader) {
    m_shader = std::move(shader);
}

void Material::set_texture(TextureSlot slot, AssetHandle<Texture> tex) {
    auto idx = static_cast<usize>(slot);
    if (idx < m_textures.size())
        m_textures[idx] = std::move(tex);
}

AssetHandle<Texture> Material::texture(TextureSlot slot) const {
    auto idx = static_cast<usize>(slot);
    return idx < m_textures.size() ? m_textures[idx] : AssetHandle<Texture>{};
}

void Material::set_uniform(const char* name, i32 v) {
    UniformValue uv;
    uv.type = UniformValue::Int;
    uv.i_val = v;
    m_uniforms[name] = uv;
}

void Material::set_uniform(const char* name, float v) {
    UniformValue uv;
    uv.type = UniformValue::Float;
    uv.f_val = v;
    m_uniforms[name] = uv;
}

void Material::set_uniform(const char* name, const glm::vec3& v) {
    UniformValue uv;
    uv.type = UniformValue::Vec3;
    uv.v3 = v;
    m_uniforms[name] = uv;
}

void Material::set_uniform(const char* name, const glm::vec4& v) {
    UniformValue uv;
    uv.type = UniformValue::Vec4;
    uv.v4 = v;
    m_uniforms[name] = uv;
}

void Material::set_uniform(const char* name, const glm::mat3& m) {
    UniformValue uv;
    uv.type = UniformValue::Mat3;
    uv.m3 = m;
    m_uniforms[name] = uv;
}

void Material::set_uniform(const char* name, const glm::mat4& m) {
    UniformValue uv;
    uv.type = UniformValue::Mat4;
    uv.m4 = m;
    m_uniforms[name] = uv;
}

void Material::remove_uniform(const char* name) {
    m_uniforms.erase(name);
}

void Material::clear_uniforms() {
    m_uniforms.clear();
}

void Material::upload_uniform(const std::string& name, const UniformValue& val) const {
    if (!m_shader) return;
    Shader& s = *m_shader;
    switch (val.type) {
        case UniformValue::Int:   s.set_int(name.c_str(), val.i_val); break;
        case UniformValue::Float: s.set_float(name.c_str(), val.f_val); break;
        case UniformValue::Vec3:  s.set_vec3(name.c_str(), val.v3); break;
        case UniformValue::Vec4:  s.set_vec4(name.c_str(), val.v4); break;
        case UniformValue::Mat3:  s.set_mat3(name.c_str(), val.m3); break;
        case UniformValue::Mat4:  s.set_mat4(name.c_str(), val.m4); break;
        default: break;
    }
}

void Material::apply() const {
    if (!m_shader) return;

    m_shader->bind();

    for (auto& [name, val] : m_uniforms) {
        upload_uniform(name, val);
    }

    for (usize i = 0; i < m_textures.size(); ++i) {
        if (m_textures[i]) {
            m_textures[i]->bind(static_cast<u32>(i));
        }
    }
}

} // namespace pino
