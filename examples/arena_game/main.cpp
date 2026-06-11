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
#include <cstdlib>
#include <cmath>

// ── Constants ──────────────────────────────────────────────────────────
static constexpr float ARENA_HALF  = 4.5f;
static constexpr float CUBE_SIZE   = 0.5f;
static constexpr float ENEMY_SPEED = 1.2f;
static constexpr float PLAYER_SPEED = 4.0f;
static constexpr int   NUM_ENEMIES = 3;
static constexpr int   NUM_COINS   = 3;

static float randf() { return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX); }

static glm::vec3 arena_pos() {
    float m = 0.8f;
    float h = ARENA_HALF - m;
    return { (randf() * 2.0f - 1.0f) * h, 0, (randf() * 2.0f - 1.0f) * h };
}

static void clamp_to_arena(glm::vec3& p) {
    float limit = ARENA_HALF - CUBE_SIZE * 0.5f;
    p.x = glm::clamp(p.x, -limit, limit);
    p.z = glm::clamp(p.z, -limit, limit);
}

// ── Main ──────────────────────────────────────────────────────────────
int main(int, char**) {
    std::srand(12345);

    pino::Engine engine;
    pino::EngineConfig cfg;
    cfg.app_title     = "Arena Collector";
    cfg.window_width  = 1024;
    cfg.window_height = 768;
    if (!engine.init(cfg)) return 1;

    int fb_w = static_cast<int>(engine.window().width());
    int fb_h = static_cast<int>(engine.window().height());

    pino::AssetManager assets(engine.filesystem());
    const char* dir = PINO_ASSET_DIR;

    auto* lit_shader = assets.load_shader(
        (std::string(dir) + "shaders/lit.vert").c_str(),
        (std::string(dir) + "shaders/lit.frag").c_str());
    if (!lit_shader) return 1;

    auto* cube_mesh = assets.load_mesh((std::string(dir) + "models/cube.obj").c_str());
    if (!cube_mesh) return 1;

    // ── Scene ──────────────────────────────────────────────────────
    pino::Scene scene;
    auto* root = scene.root();

    // Floor
    pino::Entity* floor_e = root->create_child("floor");
    floor_e->local_transform().position = { 0, -0.5f, 0 };
    floor_e->local_transform().scale    = { ARENA_HALF * 2, 0.1f, ARENA_HALF * 2 };

    // 4 walls (visual only — boundary enforced by clamping)
    auto make_wall = [&](const glm::vec3& pos, const glm::vec3& scl) {
        auto* e = root->create_child("wall");
        e->local_transform().position = pos;
        e->local_transform().scale    = scl;
    };
    float w = ARENA_HALF + 0.3f;
    float t = 0.3f;
    make_wall({ -w, 0.3f, 0 },  { t, 0.6f, w * 2 });
    make_wall({  w, 0.3f, 0 },  { t, 0.6f, w * 2 });
    make_wall({ 0,  0.3f, -w }, { w * 2, 0.6f, t });
    make_wall({ 0,  0.3f,  w }, { w * 2, 0.6f, t });

    // Player
    pino::Entity* player_e = root->create_child("player");
    player_e->local_transform().position = { 0, 0, 0 };
    player_e->local_transform().scale    = { CUBE_SIZE, CUBE_SIZE, CUBE_SIZE };

    // Enemies (3 red cubes)
    pino::Entity* enemies[NUM_ENEMIES];
    for (int i = 0; i < NUM_ENEMIES; ++i) {
        char name[16]; std::snprintf(name, sizeof(name), "enemy_%d", i);
        enemies[i] = root->create_child(name);
        glm::vec3 p = arena_pos();
        enemies[i]->local_transform().position = p;
        enemies[i]->local_transform().scale    = { CUBE_SIZE, CUBE_SIZE, CUBE_SIZE };
    }

    // Coins (3 gold cubes)
    pino::Entity* coins[NUM_COINS];
    for (int i = 0; i < NUM_COINS; ++i) {
        char name[16]; std::snprintf(name, sizeof(name), "coin_%d", i);
        coins[i] = root->create_child(name);
        coins[i]->local_transform().position = arena_pos();
        coins[i]->local_transform().scale    = { CUBE_SIZE * 0.7f, CUBE_SIZE * 0.7f, CUBE_SIZE * 0.7f };
    }

    // ── Collision world (player only vs floor/walls) ──────────────
    pino::CollisionWorld cw;

    {
        pino::ColliderComponent cc;
        cc.local_min = cube_mesh->local_min();
        cc.local_max = cube_mesh->local_max();
        cc.is_static = true;
        cw.register_collider(*floor_e, cc);
    }
    {
        pino::ColliderComponent cc;
        cc.local_min = cube_mesh->local_min();
        cc.local_max = cube_mesh->local_max();
        cc.is_static = false;
        cw.register_collider(*player_e, cc);
    }

    // ── Camera ────────────────────────────────────────────────────
    pino::Camera cam;
    cam.perspective(50.0f, static_cast<float>(fb_w) / static_cast<float>(fb_h), 0.1f, 30.0f);
    cam.look_at({ 0, 10, 10 }, { 0, 0, 0 }, { 0, 1, 0 });

    // ── Lights ────────────────────────────────────────────────────
    pino::AmbientLight ambient = { {1,1,1}, 0.5f };
    pino::DirectionalLight dir_light = { {0.3f, -1, -0.4f}, {0.7f, 0.7f, 0.8f} };

    // ── Materials ─────────────────────────────────────────────────
    pino::Material mat_floor  = { {0.12f,0.12f,0.12f}, {0.25f,0.25f,0.25f}, {0.2f,0.2f,0.2f}, {0,0,0}, 4.0f };
    pino::Material mat_wall   = { {0.2f,0.2f,0.22f},   {0.35f,0.35f,0.4f}, {0.3f,0.3f,0.3f}, {0,0,0}, 8.0f };
    pino::Material mat_player = { {0.08f,0.08f,0.2f},  {0.2f,0.5f,0.9f},   {1,1,1},          {0,0,0}, 32.0f };
    pino::Material mat_enemy  = { {0.2f,0.05f,0.05f},  {0.9f,0.15f,0.15f}, {0.7f,0.7f,0.7f}, {0,0,0}, 24.0f };
    pino::Material mat_coin   = { {0.2f,0.15f,0.05f},  {0.9f,0.8f,0.1f},   {1,1,1},          {0,0,0}, 48.0f };

    // ── Debug draw ────────────────────────────────────────────────
    pino::DebugDraw debug_draw;
    debug_draw.init();

    // ── GL state ──────────────────────────────────────────────────
    glClearColor(0.07f, 0.07f, 0.1f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // ── Game loop ─────────────────────────────────────────────────
    while (engine.is_running()) {
        engine.begin_frame();
        float dt = engine.delta_time();

        auto* in = pino::Input::instance();

        // 1. Input — keyboard
        float mx = 0, mz = 0;
        if (in->is_key_pressed(pino::Key::W) || in->is_key_pressed(pino::Key::Up))    mz -= 1;
        if (in->is_key_pressed(pino::Key::S) || in->is_key_pressed(pino::Key::Down))  mz += 1;
        if (in->is_key_pressed(pino::Key::A) || in->is_key_pressed(pino::Key::Left))  mx -= 1;
        if (in->is_key_pressed(pino::Key::D) || in->is_key_pressed(pino::Key::Right)) mx += 1;

        // Input — touch
        if (in->touch_count() > 0) {
            float sx = in->swipe_delta_x();
            float sy = in->swipe_delta_y();
            if (std::fabs(sx) > 0.02f) mx += (sx > 0 ? 1.0f : -1.0f);
            if (std::fabs(sy) > 0.02f) mz -= (sy > 0 ? 1.0f : -1.0f);
        }

        if (in->is_key_just_pressed(pino::Key::F3))
            cw.show_debug = !cw.show_debug;

        // 2. Move player
        if (mx != 0 || mz != 0) {
            float len = std::sqrt(mx * mx + mz * mz);
            mx /= len;  mz /= len;
            glm::vec3 p = player_e->local_transform().position;
            p.x += mx * PLAYER_SPEED * dt;
            p.z += mz * PLAYER_SPEED * dt;
            clamp_to_arena(p);
            player_e->local_transform().position = p;
        }

        // 3. Move enemies toward player
        glm::vec3 ppos = player_e->local_transform().position;
        for (auto* e : enemies) {
            glm::vec3 pos = e->local_transform().position;
            glm::vec3 d = ppos - pos;
            float dist = glm::length(d);
            if (dist > 0.1f) {
                pos += (d / dist) * ENEMY_SPEED * dt;
                clamp_to_arena(pos);
                e->local_transform().position = pos;
            }
        }

        // 4. Collision update (player vs floor only — no enemy physics)
        cw.update(dt);

        // 5. Coin collection check
        glm::vec3 pp = player_e->local_transform().position;
        float collect_dist = CUBE_SIZE * 1.0f;
        for (auto* c : coins) {
            glm::vec3 cp = c->local_transform().position;
            if (std::fabs(pp.x - cp.x) < collect_dist && std::fabs(pp.z - cp.z) < collect_dist) {
                c->local_transform().position = arena_pos();
            }
        }

        // 6. Enemy collision check
        float hit_dist = CUBE_SIZE * 0.9f;
        for (auto* e : enemies) {
            glm::vec3 ep = e->local_transform().position;
            if (std::fabs(pp.x - ep.x) < hit_dist && std::fabs(pp.z - ep.z) < hit_dist) {
                e->local_transform().position = arena_pos();
            }
        }

        // ── Render ───────────────────────────────────────────────
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        lit_shader->bind();

        auto vp = cam.view_proj();
        lit_shader->set_mat4("u_view_proj", vp);
        lit_shader->set_vec3("u_camera_pos", cam.position());
        pino::upload_ambient_light(*lit_shader, ambient);
        pino::upload_directional_light(*lit_shader, dir_light);
        lit_shader->set_int("u_num_point_lights", 0);

        auto draw = [&](pino::Entity& e, const pino::Material& m) {
            glm::mat4 model = e.world_matrix();
            lit_shader->set_mat4("u_model", model);
            lit_shader->set_mat3("u_normal_matrix", glm::inverseTranspose(glm::mat3(model)));
            pino::upload_material(*lit_shader, m);
            lit_shader->set_int("u_has_diffuse_tex", 0);
            cube_mesh->draw();
        };

        draw(*floor_e, mat_floor);
        root->for_each([&](pino::Entity& e) {
            if (e.name() == "wall") draw(e, mat_wall);
        });
        for (auto* e : enemies) draw(*e, mat_enemy);
        for (auto* c : coins)  draw(*c, mat_coin);
        draw(*player_e, mat_player);

        if (cw.show_debug)
            debug_draw.draw_aabbs(cw.debug_aabbs(), cam);

        engine.end_frame();
    }

    return 0;
}
