#pragma once

#include "engine/core/types.h"
#include "engine/renderer/shader.h"
#include "engine/assets/asset_manager.h"
#include <array>
#include <unordered_map>
#include <string>

#include <glm/glm.hpp>

namespace pino {

enum class TextureSlot : u8 {
    Diffuse  = 0,
    Specular,
    Normal,
    Emissive,
    COUNT
};

struct UniformValue {
    enum Type : u8 { None, Int, Float, Vec3, Vec4, Mat3, Mat4 };
    Type type = None;
    i32 i_val = 0;
    float f_val = 0.0f;
    glm::vec3 v3{0.0f};
    glm::vec4 v4{0.0f};
    glm::mat3 m3{1.0f};
    glm::mat4 m4{1.0f};
};

class Material {
public:
    Material() = default;

    void set_shader(AssetHandle<Shader> shader);
    AssetHandle<Shader> shader() const { return m_shader; }

    void set_texture(TextureSlot slot, AssetHandle<Texture> tex);
    AssetHandle<Texture> texture(TextureSlot slot) const;

    void set_uniform(const char* name, i32 v);
    void set_uniform(const char* name, float v);
    void set_uniform(const char* name, const glm::vec3& v);
    void set_uniform(const char* name, const glm::vec4& v);
    void set_uniform(const char* name, const glm::mat3& m);
    void set_uniform(const char* name, const glm::mat4& m);
    void remove_uniform(const char* name);
    void clear_uniforms();

    void apply() const;

    bool is_valid() const { return m_shader.is_loaded(); }

private:
    AssetHandle<Shader> m_shader;
    std::array<AssetHandle<Texture>, static_cast<usize>(TextureSlot::COUNT)> m_textures;
    std::unordered_map<std::string, UniformValue> m_uniforms;

    void upload_uniform(const std::string& name, const UniformValue& val) const;
};

} // namespace pino
