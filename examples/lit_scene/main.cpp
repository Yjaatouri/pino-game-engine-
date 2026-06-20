#include "engine/engine.h"
#include "engine/assets/asset_manager.h"
#include "engine/renderer/mesh.h"
#include "engine/renderer/material.h"
#include "engine/renderer/render_queue.h"
#include "engine/renderer/camera.h"
#include "engine/renderer/light.h"
#include "engine/renderer/render_stats.h"
#include "engine/core/transform.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/constants.hpp>
#include <cstdio>
#include <vector>

using pino::u32;

static pino::Mesh make_floor() {
    struct V { glm::vec3 pos; glm::vec3 nrm; glm::vec2 uv; };
    V verts[] = {
        {{-10, 0, -10}, {0, 1, 0}, {0, 0}},
        {{ 10, 0, -10}, {0, 1, 0}, {4, 0}},
        {{ 10, 0,  10}, {0, 1, 0}, {4, 4}},
        {{-10, 0,  10}, {0, 1, 0}, {0, 4}},
    };
    pino::u32 idx[] = {0, 1, 2, 0, 2, 3};
    pino::Mesh m;
    m.upload(reinterpret_cast<const pino::Vertex*>(verts), 4, idx, 6);
    return m;
}

int main(int, char**) {
    pino::Engine engine;
    pino::EngineConfig cfg;
    cfg.app_title     = "Lit Scene + RenderQueue";
    cfg.window_width  = 1024;
    cfg.window_height = 768;

    if (!engine.init(cfg)) return 1;

    auto shader = engine.assets().get_shader("shaders/lit.vert",
                                             "shaders/lit.frag");
    if (!shader) return 1;

    auto cube_mesh = engine.assets().get_mesh("models/cube.obj");
    pino::Mesh sphere_mesh = pino::Mesh::create_sphere(0.5f, 32, 24);
    pino::Mesh floor_mesh  = make_floor();
    if (!cube_mesh) return 1;

    pino::Camera cam;
    cam.perspective(45.0f, 1024.0f / 768.0f, 0.1f, 100.0f);
    cam.look_at({0, 3, 6}, {0, 0, 0}, {0, 1, 0});

    pino::AmbientLight ambient;
    ambient.color     = {1, 1, 1};
    ambient.intensity = 0.25f;

    pino::DirectionalLight dir_light;
    dir_light.direction = {0.2f, -1.0f, -0.3f};
    dir_light.color     = {0.6f, 0.6f, 0.7f};

    pino::PointLight point;
    point.color     = {1.0f, 0.7f, 0.3f};
    point.constant  = 1.0f;
    point.linear    = 0.09f;
    point.quadratic = 0.032f;

    // Materials
    pino::Material mat_floor;
    mat_floor.set_shader(shader);
    mat_floor.set_uniform("u_mat_ambient",   glm::vec3(0.3f, 0.3f, 0.3f));
    mat_floor.set_uniform("u_mat_diffuse",   glm::vec3(0.6f, 0.6f, 0.6f));
    mat_floor.set_uniform("u_mat_specular",  glm::vec3(0.0f, 0.0f, 0.0f));
    mat_floor.set_uniform("u_mat_shininess", 1.0f);
    mat_floor.set_uniform("u_has_diffuse_tex", 0);

    pino::Material mat_cube;
    mat_cube.set_shader(shader);
    mat_cube.set_uniform("u_mat_ambient",   glm::vec3(0.2f, 0.1f, 0.1f));
    mat_cube.set_uniform("u_mat_diffuse",   glm::vec3(0.9f, 0.3f, 0.2f));
    mat_cube.set_uniform("u_mat_specular",  glm::vec3(0.8f, 0.8f, 0.8f));
    mat_cube.set_uniform("u_mat_shininess", 64.0f);
    mat_cube.set_uniform("u_has_diffuse_tex", 0);

    pino::Material mat_sphere;
    mat_sphere.set_shader(shader);
    mat_sphere.set_uniform("u_mat_ambient",   glm::vec3(0.1f, 0.2f, 0.1f));
    mat_sphere.set_uniform("u_mat_diffuse",   glm::vec3(0.2f, 0.7f, 0.3f));
    mat_sphere.set_uniform("u_mat_specular",  glm::vec3(1.0f, 1.0f, 1.0f));
    mat_sphere.set_uniform("u_mat_shininess", 32.0f);
    mat_sphere.set_uniform("u_has_diffuse_tex", 0);

    pino::Material mat_orbit;
    mat_orbit.set_shader(shader);
    mat_orbit.set_uniform("u_mat_ambient",   glm::vec3(0.1f, 0.1f, 0.2f));
    mat_orbit.set_uniform("u_mat_diffuse",   glm::vec3(0.1f, 0.4f, 0.8f));
    mat_orbit.set_uniform("u_mat_specular",  glm::vec3(0.9f, 0.9f, 0.9f));
    mat_orbit.set_uniform("u_mat_shininess", 64.0f);
    mat_orbit.set_uniform("u_has_diffuse_tex", 0);

    // Transparent sphere
    pino::Material mat_transparent;
    mat_transparent.set_shader(shader);
    mat_transparent.set_uniform("u_mat_ambient",   glm::vec3(0.0f, 0.0f, 0.0f));
    mat_transparent.set_uniform("u_mat_diffuse",   glm::vec3(0.0f, 0.8f, 0.8f));
    mat_transparent.set_uniform("u_mat_specular",  glm::vec3(0.5f, 0.5f, 0.5f));
    mat_transparent.set_uniform("u_mat_shininess", 16.0f);
    mat_transparent.set_uniform("u_has_diffuse_tex", 0);

    static constexpr u32 NUM_INSTANCES = 64;
    std::vector<glm::mat4> instance_transforms(NUM_INSTANCES);

    pino::Transform t_floor;
    t_floor.position = {0, -0.5f, 0};

    pino::Transform t_cube;
    t_cube.position = {-2.0f, 0.0f, 0.0f};
    t_cube.scale    = {0.8f, 0.8f, 0.8f};

    pino::Transform t_sphere;
    t_sphere.position = {2.0f, 0.0f, 0.0f};

    pino::Transform t_transparent;
    t_transparent.position = {0.0f, 0.5f, 0.0f};
    t_transparent.scale    = {1.5f, 1.5f, 1.5f};

    pino::RenderQueue queue;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);

    while (engine.is_running()) {
        engine.begin_frame();

        float t = engine.elapsed_time();
        float radius = 3.5f;

        point.position = {radius * std::cos(t), 2.5f, radius * std::sin(t)};

        t_cube.rotation = glm::angleAxis(t * 0.8f, glm::normalize(glm::vec3{0, 1, 0}));
        t_sphere.rotation = glm::angleAxis(t * 0.4f, glm::normalize(glm::vec3{0, 1, 0}));
        t_transparent.rotation = glm::angleAxis(t * 0.3f, glm::normalize(glm::vec3{0, 1, 0}));

        glm::vec3 cam_pos = glm::vec3{5.0f * std::cos(t * 0.15f), 3.0f, 5.0f * std::sin(t * 0.15f)};
        cam.look_at(cam_pos, {0, 0, 0}, {0, 1, 0});

        // Build instance transforms
        for (u32 i = 0; i < NUM_INSTANCES; ++i) {
            float phase = static_cast<float>(i) / static_cast<float>(NUM_INSTANCES)
                          * glm::two_pi<float>();
            float x = radius * 0.7f * std::cos(phase + t * 0.2f);
            float z = radius * 0.7f * std::sin(phase + t * 0.2f);
            float h = 0.3f * std::sin(phase * 3.0f + t * 1.2f);

            glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3{x, h, z});
            float s = 0.15f + 0.08f * std::sin(phase * 5.0f + t);
            m = glm::scale(m, glm::vec3{s, s, s});
            m = glm::rotate(m, t * 1.5f + phase, glm::vec3{0, 1, 0});
            instance_transforms[i] = m;
        }
        cube_mesh->set_instance_data(instance_transforms.data(), NUM_INSTANCES);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ── Frame uniforms (set once on the shader before flush) ──
        shader->bind();
        shader->set_mat4("u_view_proj", cam.view_proj());
        shader->set_vec3("u_camera_pos", cam_pos);
        upload_ambient_light(*shader, ambient);
        upload_directional_light(*shader, dir_light);
        upload_point_lights(*shader, &point, 1);

        // ── Build queue ──
        queue.clear();

        // 1) Floor (opaque)
        {
            glm::mat4 model = t_floor.matrix();
            glm::mat3 nm = glm::transpose(glm::inverse(glm::mat3(model)));
            queue.submit({&floor_mesh, &mat_floor, model, nm, false, 0.0f, false, 0});
        }

        // 2) Center cube (opaque)
        {
            glm::mat4 model = t_cube.matrix();
            glm::mat3 nm = glm::transpose(glm::inverse(glm::mat3(model)));
            queue.submit({cube_mesh.get(), &mat_cube, model, nm, false, 0.0f, false, 0});
        }

        // 3) Center sphere (opaque)
        {
            glm::mat4 model = t_sphere.matrix();
            glm::mat3 nm = glm::transpose(glm::inverse(glm::mat3(model)));
            queue.submit({&sphere_mesh, &mat_sphere, model, nm, false, 0.0f, false, 0});
        }

        // 4) 64 orbiting cubes — instanced (opaque)
        {
            queue.submit({cube_mesh.get(), &mat_orbit, glm::mat4(1.0f), glm::mat3(1.0f),
                          false, 0.0f, true, NUM_INSTANCES});
        }

        // 5) Transparent sphere (rendered last after sorting)
        {
            glm::mat4 model = t_transparent.matrix();
            glm::mat3 nm = glm::transpose(glm::inverse(glm::mat3(model)));
            float depth = glm::distance(glm::vec3(model[3]), cam_pos);
            queue.submit({&sphere_mesh, &mat_transparent, model, nm, true, depth, false, 0});
        }

        queue.sort();
        queue.flush();

        engine.end_frame();
    }

    engine.shutdown();
    return 0;
}
