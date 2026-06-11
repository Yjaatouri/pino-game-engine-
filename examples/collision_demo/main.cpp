#include "engine/engine.h"
#include "engine/physics/collision_world.h"
#include "engine/physics/debug_draw.h"
#include "engine/assets/asset_manager.h"
#include "engine/renderer/shader.h"
#include "engine/renderer/mesh.h"
#include "engine/renderer/camera.h"
#include "engine/renderer/light.h"
#include "engine/scene/entity.h"
#include "engine/scene/scene.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <cstdio>

int main(int, char**) {
    pino::Engine engine;
    pino::EngineConfig cfg;
    cfg.app_title     = "Collision Demo";
    cfg.window_width  = 1024;
    cfg.window_height = 768;

    if (!engine.init(cfg)) return 1;

    pino::AssetManager assets(engine.filesystem());
    const char* dir = PINO_ASSET_DIR;

    auto* shader = assets.load_shader(
        (std::string(dir) + "shaders/lit.vert").c_str(),
        (std::string(dir) + "shaders/lit.frag").c_str());
    if (!shader) return 1;

    auto* cube_mesh = assets.load_mesh((std::string(dir) + "models/cube.obj").c_str());
    if (!cube_mesh) return 1;

    // Retrieve local bounds from the mesh (computed on upload)
    glm::vec3 mesh_min = cube_mesh->local_min();
    glm::vec3 mesh_max = cube_mesh->local_max();

    // Scene
    pino::Scene scene;
    pino::Entity* ground = scene.root()->create_child("ground");
    pino::Entity* wall   = scene.root()->create_child("wall");
    pino::Entity* player = scene.root()->create_child("player");

    ground->local_transform().position = { 0.0f, -1.0f, 0.0f };
    ground->local_transform().scale    = { 5.0f, 0.2f, 5.0f };

    wall->local_transform().position = { 2.0f, 0.0f, 0.0f };

    player->local_transform().position = { -2.0f, 0.0f, 0.0f };

    // Camera (orbit)
    pino::Camera cam;
    cam.perspective(45.0f, 1024.0f / 768.0f, 0.1f, 100.0f);
    cam.look_at({0, 3, 6}, {0, 0, 0}, {0, 1, 0});

    // Lights
    pino::AmbientLight ambient;
    ambient.color     = {1,1,1};
    ambient.intensity = 0.3f;
    pino::DirectionalLight dir_l;
    dir_l.direction = {0.2f, -1, -0.3f};
    dir_l.color     = {0.6f, 0.6f, 0.7f};

    // Materials
    pino::Material mat_ground;
    mat_ground.ambient   = {0.1f, 0.1f, 0.1f};
    mat_ground.diffuse   = {0.3f, 0.3f, 0.3f};
    mat_ground.specular  = {0.5f, 0.5f, 0.5f};
    mat_ground.shininess = 4.0f;

    pino::Material mat_player;
    mat_player.ambient   = {0.1f, 0.1f, 0.2f};
    mat_player.diffuse   = {0.3f, 0.5f, 0.9f};
    mat_player.specular  = {1,1,1};
    mat_player.shininess = 32.0f;

    pino::Material mat_wall;
    mat_wall.ambient   = {0.2f, 0.1f, 0.1f};
    mat_wall.diffuse   = {0.9f, 0.3f, 0.2f};
    mat_wall.specular  = {1,1,1};
    mat_wall.shininess = 32.0f;

    // Collision world
    pino::CollisionWorld cw;

    pino::ColliderComponent ground_col;
    ground_col.local_min  = mesh_min;
    ground_col.local_max  = mesh_max;
    ground_col.is_static  = true;
    cw.register_collider(*ground, ground_col);

    pino::ColliderComponent wall_col;
    wall_col.local_min = mesh_min;
    wall_col.local_max = mesh_max;
    wall_col.is_static = true;
    cw.register_collider(*wall, wall_col);

    pino::ColliderComponent player_col;
    player_col.local_min = mesh_min;
    player_col.local_max = mesh_max;
    player_col.is_static = false;
    cw.register_collider(*player, player_col);

    // Debug draw
    pino::DebugDraw debug_draw;
    debug_draw.init();

    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glEnable(GL_DEPTH_TEST);

    // Mouse orbit state
    float yaw      = 0.0f;
    float pitch    = -20.0f;
    float cam_dist = 6.0f;
    bool orbit_enabled = false;

    while (engine.is_running()) {
        engine.begin_frame();

        auto* in = pino::Input::instance();
        float dt  = engine.delta_time();
        float speed = 2.0f * dt;

        // Toggle debug visualization (F3) — just_pressed fires on the frame
        // the key transitions from up→down
        if (in->is_key_just_pressed(pino::Key::F3)) {
            cw.show_debug = !cw.show_debug;
        }

        // Player movement (WASD) — is_key_pressed checks held state
        if (in->is_key_pressed(pino::Key::W)) player->local_transform().position.z -= speed;
        if (in->is_key_pressed(pino::Key::S)) player->local_transform().position.z += speed;
        if (in->is_key_pressed(pino::Key::A)) player->local_transform().position.x -= speed;
        if (in->is_key_pressed(pino::Key::D)) player->local_transform().position.x += speed;

        // Camera orbit (mouse left drag)
        if (in->is_mouse_pressed(pino::MouseButton::Left)) {
            yaw   += static_cast<float>(in->mouse_dx()) * 0.3f;
            pitch += static_cast<float>(in->mouse_dy()) * 0.3f;
            pitch = glm::clamp(pitch, -89.0f, 89.0f);
            orbit_enabled = true;
        }
        if (orbit_enabled) {
            glm::vec3 dir;
            dir.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
            dir.y = sin(glm::radians(pitch));
            dir.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
            dir = glm::normalize(dir);
            cam.look_at(dir * cam_dist, {0, 0, 0}, {0, 1, 0});
            orbit_enabled = in->is_mouse_pressed(pino::MouseButton::Left);
        }

        // Update collision world (auto-updates AABBs from transforms, detects, resolves)
        cw.update(dt);

        // Render
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        shader->bind();

        auto vp = cam.view_proj();
        shader->set_mat4("u_view_proj", vp);

        // Helper lambda to draw an entity
        auto draw_entity = [&](pino::Entity& entity, const pino::Material& mat, pino::Mesh* mesh) {
            glm::mat4 model = entity.world_matrix();
            shader->set_mat4("u_model", model);
            shader->set_mat3("u_normal_matrix", glm::inverseTranspose(glm::mat3(model)));
            shader->set_vec3("u_camera_pos", cam.position());
            pino::upload_ambient_light(*shader, ambient);
            pino::upload_directional_light(*shader, dir_l);
            shader->set_int("u_num_point_lights", 0);
            pino::upload_material(*shader, mat);
            shader->set_int("u_has_diffuse_tex", 0);
            mesh->draw();
        };

        draw_entity(*ground, mat_ground, cube_mesh);
        draw_entity(*wall,   mat_wall,   cube_mesh);
        draw_entity(*player, mat_player, cube_mesh);

        // Debug visualization
        if (cw.show_debug) {
            debug_draw.draw_aabbs(cw.debug_aabbs(), cam);
        }

        engine.end_frame();
    }

    return 0;
}
