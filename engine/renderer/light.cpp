#include "light.h"
#include "engine/core/log.h"
#include <cstdio>

namespace pino {

void upload_ambient_light(Shader& shader, const AmbientLight& light) {
    shader.set_vec3("u_ambient.color", light.color);
    shader.set_float("u_ambient.intensity", light.intensity);
}

void upload_directional_light(Shader& shader, const DirectionalLight& light) {
    shader.set_vec3("u_dir_light.direction", light.direction);
    shader.set_vec3("u_dir_light.color", light.color);
}

void upload_point_lights(Shader& shader, const PointLight* lights, i32 count) {
    if (count > MAX_POINT_LIGHTS) {
        PINO_WARN("Too many point lights (%d), clamping to %d", count, MAX_POINT_LIGHTS);
        count = MAX_POINT_LIGHTS;
    }
    shader.set_int("u_num_point_lights", count);
    for (i32 i = 0; i < count; ++i) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "u_point_lights[%d].position", i);
        shader.set_vec3(buf, lights[i].position);
        std::snprintf(buf, sizeof(buf), "u_point_lights[%d].color", i);
        shader.set_vec3(buf, lights[i].color);
        std::snprintf(buf, sizeof(buf), "u_point_lights[%d].constant", i);
        shader.set_float(buf, lights[i].constant);
        std::snprintf(buf, sizeof(buf), "u_point_lights[%d].linear", i);
        shader.set_float(buf, lights[i].linear);
        std::snprintf(buf, sizeof(buf), "u_point_lights[%d].quadratic", i);
        shader.set_float(buf, lights[i].quadratic);
    }
}

void upload_material(Shader& shader, const Material& material) {
    shader.set_vec3("u_mat_ambient", material.ambient);
    shader.set_vec3("u_mat_diffuse", material.diffuse);
    shader.set_vec3("u_mat_specular", material.specular);
    shader.set_vec3("u_mat_emissive", material.emissive);
    shader.set_float("u_mat_shininess", material.shininess);
    if (material.diffuse_tex) {
        shader.set_int("u_has_diffuse_tex", 1);
        shader.set_int("u_mat_diffuse_tex", 0);
        material.diffuse_tex->bind(0);
    } else {
        shader.set_int("u_has_diffuse_tex", 0);
    }
}

} // namespace pino
