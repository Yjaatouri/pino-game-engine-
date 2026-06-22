#include "engine/engine.h"
#include "engine/physics/collision_world.h"
#include "engine/physics/debug_draw.h"
#include "engine/assets/asset_manager.h"
#include "engine/renderer/shader.h"
#include "engine/renderer/mesh.h"
#include "engine/renderer/camera.h"
#include "engine/renderer/light.h"
#include "engine/renderer/font.h"
#include "engine/renderer/text_renderer.h"
#include "engine/scene/entity.h"
#include "engine/scene/scene.h"
#include "engine/ecs/ecs_scene.h"
#include "engine/audio/audio_manager.h"
#include "engine/serialization/save_game_serializer.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/constants.hpp>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <unordered_map>
#include <string>
#include <algorithm>

static constexpr float ARENA_HALF      = 6.0f;
static constexpr float CUBE_SIZE       = 0.4f;
static constexpr float PLAYER_SPEED    = 5.0f;
static constexpr float ENEMY_SPEED     = 2.4f;
static constexpr float ATTACK_RANGE    = 4.0f;
static constexpr float ATTACK_ANGLE    = 0.4f;
static constexpr float ATTACK_COOLDOWN = 0.3f;
static constexpr int   PLAYER_MAX_HP   = 10;
static constexpr int   BASE_ENEMIES    = 3;
static constexpr int   ENEMIES_PER_WAVE = 2;
static constexpr float WAVE_INTERVAL   = 8.0f;
static constexpr float PLAYER_BODY_RADIUS = CUBE_SIZE * 0.8f;
static constexpr float ENEMY_BODY_RADIUS  = CUBE_SIZE * 0.8f;
static constexpr float DAMAGE_INTERVAL    = 1.0f;

static float randf() { return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX); }

static glm::vec3 random_edge_pos() {
    float h = ARENA_HALF - 0.5f;
    int side = std::rand() % 4;
    float x = (randf() * 2.0f - 1.0f) * h;
    float z = (randf() * 2.0f - 1.0f) * h;
    switch (side) {
        case 0: return { x, 0, -h - 1.5f };
        case 1: return { x, 0,  h + 1.5f };
        case 2: return { -h - 1.5f, 0, z };
        default: return { h + 1.5f, 0, z };
    }
}

struct Enemy {
    pino::EntityId ecs_id;
    int hp = 1;
};

struct Player {
    pino::EntityId ecs_id;
    int hp = PLAYER_MAX_HP;
    int score = 0;
    float shoot_cooldown = 0.0f;
    float damage_cooldown = 0.0f;
    int wave = 0;
    float wave_timer = WAVE_INTERVAL;
};

int main(int, char**) {
    std::srand(static_cast<unsigned>(12345));

    pino::Engine engine;
    pino::EngineConfig cfg;
    cfg.app_title     = "Micro Arena Survival";
    cfg.window_width  = 1024;
    cfg.window_height = 768;
    cfg.fixed_update_rate = 60;
    cfg.vsync         = true;
    if (!engine.init(cfg)) return 1;

    int fb_w = static_cast<int>(engine.window().width());
    int fb_h = static_cast<int>(engine.window().height());

    auto lit_shader = engine.assets().get_shader("shaders/lit.vert",
                                                  "shaders/lit.frag");
    if (!lit_shader) return 1;

    auto cube_mesh = engine.assets().get_mesh("models/cube.obj");
    if (!cube_mesh) return 1;
    glm::vec3 mesh_min = cube_mesh->local_min();
    glm::vec3 mesh_max = cube_mesh->local_max();

    // ── ECS scene (all game entities) ──────────────────────────────
    pino::EcsScene scene;
    auto& sg = scene.scene_graph();

    // ── Collision world (physics) ──────────────────────────────────
    pino::CollisionWorld cw;

    // ── Static environment (tree scene + CW colliders) ────────────
    pino::Scene env_scene;
    pino::Entity* env_root = env_scene.root();

    auto add_wall = [&](const glm::vec3& pos, const glm::vec3& scl) {
        pino::Entity* e = env_root->create_child("wall");
        e->local_transform().position = pos;
        e->local_transform().scale    = scl;
        pino::ColliderComponent cc;
        cc.local_min = mesh_min;
        cc.local_max = mesh_max;
        cc.is_static = true;
        cc.collision_layer = 3;
        cc.collision_mask  = 3;
        cw.register_collider(*e, cc);
    };
    float w = ARENA_HALF + 0.3f;
    float t = 0.3f;
    add_wall({ -w, 0.3f, 0 },  { t, 0.6f, w * 2 });
    add_wall({  w, 0.3f, 0 },  { t, 0.6f, w * 2 });
    add_wall({ 0,  0.3f, -w }, { w * 2, 0.6f, t });
    add_wall({ 0,  0.3f,  w }, { w * 2, 0.6f, t });

    pino::Entity* floor_e = env_root->create_child("floor");
    floor_e->local_transform().position = { 0, -0.5f, 0 };
    floor_e->local_transform().scale    = { ARENA_HALF * 2, 0.1f, ARENA_HALF * 2 };
    {
        pino::ColliderComponent cc;
        cc.local_min = mesh_min;
        cc.local_max = mesh_max;
        cc.is_static = true;
        cw.register_collider(*floor_e, cc);
    }

    // ── Player (ECS) ──────────────────────────────────────────────
    Player player;
    player.ecs_id = scene.create_entity();
    sg.attach(player.ecs_id);
    sg.set_position(player.ecs_id, {0, 0, 0});
    sg.set_scale(player.ecs_id, {CUBE_SIZE, CUBE_SIZE, CUBE_SIZE});

    pino::RenderComponent& prc = scene.add_component<pino::RenderComponent>(player.ecs_id);
    prc.mesh = engine.assets().get_mesh("models/cube.obj");

    pino::PhysicsComponent& ppc = scene.add_component<pino::PhysicsComponent>(player.ecs_id);
    ppc.local_min = mesh_min;
    ppc.local_max = mesh_max;
    ppc.is_static = false;
    ppc.collision_layer = 1;
    ppc.collision_mask  = 2 | 3;

    // ── Enemies ────────────────────────────────────────────────────
    std::vector<Enemy> enemies;

    auto spawn_enemy = [&]() -> bool {
        Enemy e;
        e.ecs_id = scene.create_entity();
        if (!sg.attach(e.ecs_id)) { scene.destroy_entity(e.ecs_id); return false; }
        glm::vec3 p = random_edge_pos();
        sg.set_position(e.ecs_id, p);
        sg.set_scale(e.ecs_id, {CUBE_SIZE, CUBE_SIZE, CUBE_SIZE});

        pino::RenderComponent& rc = scene.add_component<pino::RenderComponent>(e.ecs_id);
        rc.mesh = engine.assets().get_mesh("models/cube.obj");

        pino::PhysicsComponent& ec = scene.add_component<pino::PhysicsComponent>(e.ecs_id);
        ec.local_min = mesh_min;
        ec.local_max = mesh_max;
        ec.is_static = false;
        ec.collision_layer = 2;
        ec.collision_mask  = 1 | 3;

        e.hp = 1 + player.wave / 5;
        enemies.push_back(e);
        return true;
    };

    auto destroy_enemy = [&](size_t idx) {
        scene.destroy_entity(enemies[idx].ecs_id);
        enemies.erase(enemies.begin() + static_cast<ptrdiff_t>(idx));
    };

    for (int i = 0; i < BASE_ENEMIES; ++i) spawn_enemy();

    // ── Camera ─────────────────────────────────────────────────────
    pino::Camera cam;
    cam.perspective(45.0f, static_cast<float>(fb_w) / static_cast<float>(fb_h), 0.1f, 30.0f);
    cam.look_at({ 0, 12, 12 }, { 0, 0, 0 }, { 0, 1, 0 });

    // ── Lights ─────────────────────────────────────────────────────
    pino::AmbientLight ambient = { {1,1,1}, 0.4f };
    pino::DirectionalLight dir_light = { {0.3f, -1, -0.4f}, {0.7f, 0.7f, 0.8f} };

    // ── Materials ──────────────────────────────────────────────────
    pino::PhongMaterial mat_floor  = { {0.1f,0.1f,0.1f}, {0.2f,0.2f,0.2f}, {0.1f,0.1f,0.1f}, {0,0,0}, 2.0f };
    pino::PhongMaterial mat_wall   = { {0.15f,0.15f,0.18f}, {0.25f,0.25f,0.3f}, {0.2f,0.2f,0.2f}, {0,0,0}, 8.0f };
    pino::PhongMaterial mat_player = { {0.08f,0.08f,0.3f}, {0.2f,0.4f,1.0f}, {1,1,1}, {0,0,0}, 32.0f };
    pino::PhongMaterial mat_enemy  = { {0.25f,0.05f,0.05f}, {1.0f,0.15f,0.15f}, {0.7f,0.7f,0.7f}, {0,0,0}, 24.0f };
    pino::PhongMaterial mat_hurt   = { {0.5f,0.0f,0.0f}, {1.0f,0.0f,0.0f}, {0.8f,0.8f,0.8f}, {0,0,0}, 24.0f };

    // ── Audio ──────────────────────────────────────────────────────
    auto& audio = engine.audio();
    pino::SoundHandle shoot_sound = audio.preload("audio/test.wav");
    pino::SoundHandle hit_sound   = audio.preload("audio/test_tone.wav");

    // ── Debug draw ─────────────────────────────────────────────────
    pino::DebugDraw debug_draw;
    debug_draw.init();

    // ── Profiler ───────────────────────────────────────────────────
    auto& profiler = engine.profiler();
    pino::u32 prof_zone_game   = profiler.register_zone("Game Logic");
    pino::u32 prof_zone_phys   = profiler.register_zone("Physics (CW)");
    pino::u32 prof_zone_render = profiler.register_zone("Render");

    // ── GL state ───────────────────────────────────────────────────
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // ── Font & text renderer ───────────────────────────────────────
    pino::Font font;
    pino::TextRenderer text_renderer;
    bool hud_ok = font.load_builtin() && text_renderer.init(fb_w, fb_h);
    if (!hud_ok) PINO_WARN("HUD unavailable");

    // ── Game loop ───────────────────────────────────────────────────
    bool game_over = false;
    float game_time = 0.0f;

    while (engine.is_running()) {
        engine.begin_frame();
        float dt = engine.delta_time();
        (void)dt;

        // ── Game Logic ─────────────────────────────────────────
        profiler.begin(prof_zone_game);

        if (!game_over) {
            auto* in = pino::Input::instance();

            // Player movement (WASD)
            float mx = 0, mz = 0;
            if (in->is_key_pressed(pino::Key::W) || in->is_key_pressed(pino::Key::Up))    mz -= 1;
            if (in->is_key_pressed(pino::Key::S) || in->is_key_pressed(pino::Key::Down))  mz += 1;
            if (in->is_key_pressed(pino::Key::A) || in->is_key_pressed(pino::Key::Left))  mx -= 1;
            if (in->is_key_pressed(pino::Key::D) || in->is_key_pressed(pino::Key::Right)) mx += 1;

            glm::vec3 ppos = sg.world_position(player.ecs_id);
            if (mx != 0 || mz != 0) {
                float len = 1.0f / std::sqrt(mx * mx + mz * mz);
                mx *= len; mz *= len;
                ppos.x += mx * PLAYER_SPEED * engine.delta_time();
                ppos.z += mz * PLAYER_SPEED * engine.delta_time();
            }
            sg.set_position(player.ecs_id, ppos);

            // Shooting (area attack, left mouse)
            player.shoot_cooldown -= engine.delta_time();
            if (in->is_mouse_just_pressed(pino::MouseButton::Left) && player.shoot_cooldown <= 0.0f) {
                player.shoot_cooldown = ATTACK_COOLDOWN;
                audio.play_one_shot(shoot_sound, 0.5f);

                glm::vec3 fwd = glm::normalize(glm::vec3(cam.position() - ppos));
                fwd.y = 0; fwd = glm::normalize(fwd);

                for (size_t i = 0; i < enemies.size(); ) {
                    glm::vec3 epos = sg.world_position(enemies[i].ecs_id);
                    glm::vec3 to_enemy = epos - ppos;
                    float dist = glm::length(to_enemy);
                    if (dist < ATTACK_RANGE && dist > 0.01f) {
                        to_enemy /= dist;
                        if (glm::dot(to_enemy, fwd) > ATTACK_ANGLE) {
                            enemies[i].hp--;
                            if (enemies[i].hp <= 0) {
                                player.score++;
                                audio.play_one_shot(hit_sound, 0.4f);
                                destroy_enemy(i);
                                continue;
                            }
                        }
                    }
                    ++i;
                }
            }

            // Enemy AI
            glm::vec3 ppos2 = sg.world_position(player.ecs_id);
            for (auto& e : enemies) {
                if (!scene.alive(e.ecs_id)) continue;
                glm::vec3 epos = sg.world_position(e.ecs_id);
                glm::vec3 dir = ppos2 - epos;
                float dist = glm::length(dir);
                if (dist > 0.1f) {
                    epos += (dir / dist) * ENEMY_SPEED * engine.delta_time();
                    sg.set_position(e.ecs_id, epos);
                }
            }

            // Player damage from enemy contact
            player.damage_cooldown -= engine.delta_time();
            if (player.damage_cooldown <= 0.0f) {
                glm::vec3 ppos3 = sg.world_position(player.ecs_id);
                for (auto& e : enemies) {
                    if (!scene.alive(e.ecs_id)) continue;
                    glm::vec3 epos3 = sg.world_position(e.ecs_id);
                    float dx = ppos3.x - epos3.x;
                    float dz = ppos3.z - epos3.z;
                    float contact = PLAYER_BODY_RADIUS + ENEMY_BODY_RADIUS;
                    if (dx * dx + dz * dz < contact * contact) {
                        player.hp--;
                        player.damage_cooldown = DAMAGE_INTERVAL;
                        if (player.hp <= 0) {
                            game_over = true;
                            PINO_INFO("Player died! Score: %d", player.score);
                        }
                        break;
                    }
                }
            }

            // Wave spawning
            player.wave_timer -= engine.delta_time();
            if (player.wave_timer <= 0.0f) {
                player.wave++;
                int target = BASE_ENEMIES + player.wave * ENEMIES_PER_WAVE;
                if (target > 50) target = 50;
                for (int i = static_cast<int>(enemies.size()); i < target; ++i) {
                    spawn_enemy();
                }
                player.wave_timer = WAVE_INTERVAL;
                PINO_INFO("Wave %d — %zu enemies", player.wave, enemies.size());
            }

            game_time += engine.delta_time();
        }

        profiler.end(prof_zone_game);

        // ── Physics ────────────────────────────────────────────
        profiler.begin(prof_zone_phys);
        scene.flush_destroyed_entities();
        scene.update_physics(cw, engine.delta_time());
        profiler.end(prof_zone_phys);

        // ── Render ─────────────────────────────────────────────
        profiler.begin(prof_zone_render);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        lit_shader->bind();

        auto vp = cam.view_proj();
        lit_shader->set_mat4("u_view_proj", vp);
        lit_shader->set_vec3("u_camera_pos", cam.position());
        pino::upload_ambient_light(*lit_shader, ambient);
        pino::upload_directional_light(*lit_shader, dir_light);
        lit_shader->set_int("u_num_point_lights", 0);

        auto draw_cube = [&](const pino::PhongMaterial& mat, const glm::vec3& pos,
                              const glm::vec3& scl, float rot_y = 0.0f) {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
            if (rot_y != 0.0f) model = glm::rotate(model, rot_y, glm::vec3{0,1,0});
            model = glm::scale(model, scl);
            lit_shader->set_mat4("u_model", model);
            lit_shader->set_mat3("u_normal_matrix", glm::inverseTranspose(glm::mat3(model)));
            pino::upload_material(*lit_shader, mat);
            lit_shader->set_int("u_has_diffuse_tex", 0);
            cube_mesh->draw();
        };

        // Floor
        draw_cube(mat_floor, floor_e->local_transform().position, floor_e->local_transform().scale);

        // Walls
        env_root->for_each([&](pino::Entity& e) {
            if (e.name() == "wall")
                draw_cube(mat_wall, e.local_transform().position, e.local_transform().scale);
        });

        // Player
        if (scene.alive(player.ecs_id)) {
            glm::vec3 p_pos = sg.world_position(player.ecs_id);
            bool flash = (player.damage_cooldown > DAMAGE_INTERVAL - 0.1f)
                      && (std::fmod(game_time * 10.0f, 2.0f) < 1.0f);
            draw_cube(flash ? mat_hurt : mat_player, p_pos, {CUBE_SIZE, CUBE_SIZE, CUBE_SIZE});
        }

        // Enemies
        for (auto& e : enemies) {
            if (!scene.alive(e.ecs_id)) continue;
            glm::vec3 pos = sg.world_position(e.ecs_id);
            float bob = 0.02f * std::sin(game_time * 4.0f + static_cast<float>(e.ecs_id.index));
            pos.y += bob;
            draw_cube(mat_enemy, pos, {CUBE_SIZE, CUBE_SIZE, CUBE_SIZE});
        }

        // Debug visualization
        if (cw.show_debug) {
            debug_draw.draw_aabbs(cw.debug_aabbs(), cam);
        }

        profiler.end(prof_zone_render);

        // ── HUD ────────────────────────────────────────────────
        if (hud_ok) {
            char hud[256];
            std::snprintf(hud, sizeof(hud),
                "HP: %d/%d  Score: %d  Wave: %d  Enemies: %zu",
                player.hp, PLAYER_MAX_HP, player.score, player.wave,
                enemies.size());
            text_renderer.begin_frame();
            text_renderer.draw_text(font, hud, 10.0f, static_cast<float>(fb_h - 30),
                                    0.5f, 1.0f, 1.0f, 1.0f, 1.0f);
            if (game_over) {
                text_renderer.draw_text(font, "GAME OVER",
                    static_cast<float>(fb_w / 2 - 80), static_cast<float>(fb_h / 2),
                    1.0f, 1.0f, 0.0f, 0.0f, 1.0f);
            }
            text_renderer.render(fb_w, fb_h);
            text_renderer.end_frame();
        }

        // ── Profiler ───────────────────────────────────────────
        profiler.end_frame();

        engine.end_frame();
    }

    // ── Serialization save ─────────────────────────────────────────
    {
        PINO_INFO("Saving game state (score=%d wave=%d)...", player.score, player.wave);
        pino::TypeRegistry types;
        pino::VersionRegistry versions;
        pino::StringTable strings;
        pino::SaveGameSerializer::registerTypes(types);
        pino::SaveGameSerializer::registerVersions(versions);
        pino::SaveGameSerializer save_ser(types, versions, strings);

        pino::BinaryChunkWriter chunk_writer;
        pino::Serializer ser(chunk_writer);
        save_ser.serialize(ser, scene);
        const auto& buf = chunk_writer.getBuffer();
        FILE* f = fopen("micro_arena_save.pino", "wb");
        if (f) {
            fwrite(buf.data(), 1, buf.size(), f);
            fclose(f);
            PINO_INFO("Saved to micro_arena_save.pino (%zu bytes)", buf.size());
        } else {
            PINO_WARN("Save failed");
        }
    }

    engine.shutdown();
    return 0;
}
