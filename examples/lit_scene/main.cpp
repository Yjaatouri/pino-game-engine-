#include "engine/engine.h"
#include "engine/assets/asset_manager.h"
#include "engine/renderer/mesh.h"
#include "engine/renderer/camera.h"
#include "engine/renderer/light.h"
#include "engine/core/transform.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <cstdio>

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
    cfg.app_title     = "Lit Scene";
    cfg.window_width  = 1024;
    cfg.window_height = 768;

    if (!engine.init(cfg)) return 1;

    pino::AssetManager assets(engine.filesystem());

    // Load shader
    auto* shader = assets.load_shader("shaders/lit.vert",
                                      "shaders/lit.frag");
    if (!shader) return 1;

    // Meshes
    auto* cube_mesh = assets.load_mesh("models/cube.obj");
    pino::Mesh sphere_mesh = pino::Mesh::create_sphere(0.5f, 32, 24);
    pino::Mesh floor_mesh  = make_floor();

    if (!cube_mesh) return 1;

    // Camera
    pino::Camera cam;
    cam.perspective(45.0f, 1024.0f / 768.0f, 0.1f, 100.0f);
    cam.look_at({0, 3, 6}, {0, 0, 0}, {0, 1, 0});

    // Lights
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
    mat_floor.ambient   = {0.3f, 0.3f, 0.3f};
    mat_floor.diffuse   = {0.6f, 0.6f, 0.6f};
    mat_floor.specular  = {0.0f, 0.0f, 0.0f};
    mat_floor.shininess = 1.0f;

    pino::Material mat_cube;
    mat_cube.ambient   = {0.2f, 0.1f, 0.1f};
    mat_cube.diffuse   = {0.9f, 0.3f, 0.2f};
    mat_cube.specular  = {0.8f, 0.8f, 0.8f};
    mat_cube.shininess = 64.0f;

    pino::Material mat_sphere;
    mat_sphere.ambient   = {0.1f, 0.2f, 0.1f};
    mat_sphere.diffuse   = {0.2f, 0.7f, 0.3f};
    mat_sphere.specular  = {1.0f, 1.0f, 1.0f};
    mat_sphere.shininess = 32.0f;

    // Transforms
    pino::Transform t_floor;
    t_floor.position = {0, -0.5f, 0};

    pino::Transform t_cube;
    t_cube.position = {-2.0f, 0.0f, 0.0f};
    t_cube.scale    = {0.8f, 0.8f, 0.8f};

    pino::Transform t_sphere;
    t_sphere.position = {2.0f, 0.0f, 0.0f};
    t_sphere.scale    = {1.0f, 1.0f, 1.0f};

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);

    struct Drawable {
        pino::Mesh*     mesh;
        pino::Transform* transform;
        pino::Material*  material;
    };

    Drawable drawables[] = {
        {&floor_mesh,  &t_floor,  &mat_floor},
        { cube_mesh,   &t_cube,   &mat_cube},
        {&sphere_mesh, &t_sphere, &mat_sphere},
    };

    while (engine.is_running()) {
        engine.begin_frame();

        float t = engine.elapsed_time();

        // Orbit the point light
        float radius = 3.5f;
        point.position = {radius * std::cos(t), 2.5f, radius * std::sin(t)};

        // Rotate cube
        t_cube.rotation = glm::angleAxis(t * 0.8f, glm::normalize(glm::vec3{0, 1, 0}));
        // Slowly rotate sphere
        t_sphere.rotation = glm::angleAxis(t * 0.4f, glm::normalize(glm::vec3{0, 1, 0}));

        // Camera orbit
        glm::vec3 cam_pos = glm::vec3{5.0f * std::cos(t * 0.15f), 3.0f, 5.0f * std::sin(t * 0.15f)};
        cam.look_at(cam_pos, {0, 0, 0}, {0, 1, 0});

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader->bind();
        shader->set_mat4("u_view_proj", cam.view_proj());
        shader->set_vec3("u_camera_pos", cam_pos);

        upload_ambient_light(*shader, ambient);
        upload_directional_light(*shader, dir_light);
        upload_point_lights(*shader, &point, 1);

        for (auto& d : drawables) {
            glm::mat4 model = d.transform->matrix();
            glm::mat3 normal_mat = glm::transpose(glm::inverse(glm::mat3(model)));

            shader->set_mat4("u_model", model);
            shader->set_mat3("u_normal_matrix", normal_mat);

            upload_material(*shader, *d.material);
            d.mesh->draw();
        }

        engine.end_frame();
    }

    engine.shutdown();
    return 0;
}
