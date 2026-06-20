#pragma once

#include "engine/core/types.h"
#include "engine/renderer/shader.h"
#include "engine/assets/asset_manager.h"
#include <glm/glm.hpp>

namespace pino {

static constexpr i32 MAX_POINT_LIGHTS = 8;

struct AmbientLight {
    glm::vec3 color{1.0f};
    float intensity = 0.3f;
};

struct DirectionalLight {
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    glm::vec3 color{1.0f};
};

struct PointLight {
    glm::vec3 position{0.0f};
    glm::vec3 color{1.0f};
    float constant  = 1.0f;
    float linear    = 0.09f;
    float quadratic = 0.032f;
};

struct PhongMaterial {
    glm::vec3 ambient  {0.2f, 0.2f, 0.2f};
    glm::vec3 diffuse  {0.8f, 0.8f, 0.8f};
    glm::vec3 specular {1.0f, 1.0f, 1.0f};
    glm::vec3 emissive {0.0f, 0.0f, 0.0f};
    float shininess = 32.0f;
    AssetHandle<Texture> diffuse_tex;
};

void upload_ambient_light(Shader& shader, const AmbientLight& light);
void upload_directional_light(Shader& shader, const DirectionalLight& light);
void upload_point_lights(Shader& shader, const PointLight* lights, i32 count);
void upload_material(Shader& shader, const PhongMaterial& material);

} // namespace pino
