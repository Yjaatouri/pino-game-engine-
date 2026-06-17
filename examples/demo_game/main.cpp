#include "engine/engine.h"
#include "engine/scene/scene.h"
#include "engine/scene/scene_manager.h"
#include "engine/physics/collision_world.h"
#include "engine/renderer/debug_renderer.h"
#include "engine/assets/asset_manager.h"
#include "engine/renderer/shader.h"
#include "engine/renderer/mesh.h"
#include "engine/renderer/camera.h"
#include "engine/renderer/light.h"
#include "engine/core/event_bus.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <cstdio>
#include <cmath>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <memory>
#include <chrono>

// ── Constants ──────────────────────────────────────────────────────────
static constexpr float ARENA_HALF         = 5.0f;
static constexpr float CUBE_SCALE         = 0.45f;
static constexpr float PLAYER_SPEED       = 5.0f;
static constexpr float ENEMY_BASE_SPEED   = 2.0f;
static constexpr float ENEMY_SPEED_INCR   = 0.04f;
static constexpr int   INITIAL_LIVES      = 3;
static constexpr int   MAX_ENEMIES        = 20;
static constexpr float BASE_SPAWN_SEC     = 3.0f;
static constexpr float MIN_SPAWN_SEC      = 0.5f;
static constexpr float SHAKE_DUR_SEC      = 0.35f;
static constexpr float SHAKE_FREQ_HZ      = 25.0f;
static constexpr float SHAKE_AMP          = 0.35f;
static constexpr int   BURST_COUNT        = 10;
static constexpr float PARTICLE_LIFETIME  = 0.8f;
static constexpr float PARTICLE_SPEED     = 2.0f;
static constexpr int   MAX_PARTICLES      = 120;
static constexpr float HIT_RADIUS         = 0.7f;
static constexpr float COIN_RADIUS        = 0.9f;
static constexpr float WALL_T            = 0.4f;
static constexpr int   PROFILE_WINDOW     = 120;

static float rng() { return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX); }
static float rng_r(float lo, float hi) { return lo + rng() * (hi - lo); }

static glm::vec3 arena_pos() {
    float m = CUBE_SCALE * 2.0f;
    float h = ARENA_HALF - m - WALL_T;
    return { rng_r(-h, h), 0.0f, rng_r(-h, h) };
}

static void clamp_arena(glm::vec3& p) {
    float lim = ARENA_HALF - CUBE_SCALE * 0.5f - WALL_T;
    p.x = glm::clamp(p.x, -lim, lim);
    p.z = glm::clamp(p.z, -lim, lim);
}

static int64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

// ── Forward decls ──────────────────────────────────────────────────────
class DemoGame;
enum class GameAction { None, StartGame, GameOver, Restart, MainMenu, Quit };

struct GameContext {
    DemoGame*          game       = nullptr;
    pino::AssetManager* assets    = nullptr;
    pino::Shader*      shader     = nullptr;
    pino::Mesh*        cube       = nullptr;
    pino::Camera*      cam        = nullptr;
    pino::SceneManager* scenes    = nullptr;
    pino::Scene*       world      = nullptr;
    int*               lives      = nullptr;
    int*               score      = nullptr;
    int*               final_score = nullptr;
    GameAction*        action     = nullptr;
};

// ── Camera Shake (render-only) ─────────────────────────────────────────
struct CameraShake {
    float amp = 0, freq = SHAKE_FREQ_HZ, dur = 0, t = 0;
    glm::vec3 off{0};

    void trigger(float a, float f, float d) { amp = a; freq = f; dur = d; t = 0; }

    void update(float dt) {
        if (dur <= 0) { off = {0,0,0}; return; }
        t += dt;
        if (t >= dur) { amp = 0; dur = 0; off = {0,0,0}; return; }
        float decay = 1.0f - t / dur;
        float ph = t * freq * 6.2831853f;
        off.x = std::sin(ph)         * amp * decay;
        off.y = std::cos(ph * 1.3f)  * amp * decay * 0.6f;
        off.z = std::sin(ph * 0.7f)  * amp * decay * 0.4f;
    }

    glm::mat4 shaken_view(const glm::mat4& view) const {
        if (amp <= 0) return view;
        return glm::translate(view, off);
    }
};

// ── Particle Pool (no per-frame alloc) ─────────────────────────────────
struct Particle {
    glm::vec3 pos, vel;
    float r = 1, g = 1, b = 1, a = 1, radius = 0.05f, life = 0, max_life = 1;
    bool alive = false;
};

struct ParticlePool {
    Particle pool[MAX_PARTICLES];

    void burst(const glm::vec3& o, int n, float cr, float cg, float cb) {
        for (int i = 0; i < MAX_PARTICLES && n > 0; ++i) {
            if (pool[i].alive) continue;
            --n;
            auto& p = pool[i];
            p.alive = true; p.pos = o; p.radius = 0.05f;
            p.r = cr; p.g = cg; p.b = cb; p.a = 1;
            p.life = 0; p.max_life = PARTICLE_LIFETIME * rng_r(0.6f, 1.0f);
            float th = rng_r(0, 6.2831853f);
            float ph = rng_r(-1.0f, 1.0f);
            float sp = PARTICLE_SPEED * rng_r(0.5f, 1.5f);
            p.vel = glm::vec3{std::cos(th)*std::sqrt(1-ph*ph), ph, std::sin(th)*std::sqrt(1-ph*ph)} * sp;
        }
    }

    void update(float dt) {
        for (int i = 0; i < MAX_PARTICLES; ++i) {
            auto& p = pool[i];
            if (!p.alive) continue;
            p.life += dt;
            if (p.life >= p.max_life) { p.alive = false; continue; }
            float t = p.life / p.max_life;
            p.pos += p.vel * dt;
            p.vel *= 0.97f;
            p.radius = 0.05f + t * PARTICLE_SPEED * 0.3f;
            p.a = 1.0f - t;
        }
    }

    void render(pino::DebugRenderer& dr) {
        for (int i = 0; i < MAX_PARTICLES; ++i) {
            auto& p = pool[i];
            if (!p.alive) continue;
            dr.draw_sphere(p.pos, p.radius, {p.r, p.g, p.b, p.a}, 8);
        }
    }
};

// ── Spawn System ───────────────────────────────────────────────────────
struct SpawnSystem {
    float interval = BASE_SPAWN_SEC, elapsed = 0;
    int max_active = 10, active = 0;

    void rate_for(int score) {
        interval = glm::max(MIN_SPAWN_SEC, BASE_SPAWN_SEC - static_cast<float>(score) * 0.025f);
        max_active = glm::min(MAX_ENEMIES, 10 + score / 5);
    }

    bool ready(float dt) {
        elapsed += dt;
        if (elapsed >= interval && active < max_active) {
            elapsed = 0; return true;
        }
        return false;
    }
};

// ── Profiler ───────────────────────────────────────────────────────────
struct Profiler {
    struct S { float update_ms, render_ms; };
    S samples[PROFILE_WINDOW];
    int idx = 0, count = 0;
    float avg_u = 0, avg_r = 0, max_u = 0, max_f = 0, min_fps = 999, max_fps = 0;

    void record(float u, float r, float frame_dt) {
        samples[idx] = {u, r};
        idx = (idx + 1) % PROFILE_WINDOW;
        if (count < PROFILE_WINDOW) ++count;
        float su = 0, sr = 0;
        for (int i = 0; i < count; ++i) { su += samples[i].update_ms; sr += samples[i].render_ms; }
        avg_u = su / count; avg_r = sr / count;
        max_u = glm::max(max_u, u);
        max_f = glm::max(max_f, u + r);
        float fps = frame_dt > 0 ? 1.0f / frame_dt : 0;
        min_fps = glm::min(min_fps, fps); max_fps = glm::max(max_fps, fps);
    }

    void summary() {
        PINO_INFO("=== Performance ===");
        PINO_INFO("Update avg: %.3fms  max: %.3fms", avg_u, max_u);
        PINO_INFO("Render avg: %.3fms", avg_r);
        PINO_INFO("Frame max: %.3fms", max_f);
        PINO_INFO("FPS min: %.1f  max: %.1f", min_fps, max_fps);
    }
};

// ══════════════════════════════════════════════════════════════════════
//  Scenes
// ══════════════════════════════════════════════════════════════════════

// ── MenuScene ─────────────────────────────────────────────────────────
class MenuScene final : public pino::IScene {
public:
    explicit MenuScene(GameContext& ctx) : m_ctx(ctx) {}
    void init() override { m_sel = 0; }
    void shutdown() override {}

    void update(pino::f32) override {
        auto* in = pino::Input::instance();
        if (in->is_key_just_pressed(pino::Key::Down) || in->is_key_just_pressed(pino::Key::Up) ||
            in->is_key_just_pressed(pino::Key::S) || in->is_key_just_pressed(pino::Key::W))
            m_sel = (m_sel + 1) % 2;
        if (in->is_key_just_pressed(pino::Key::Enter) || in->is_key_just_pressed(pino::Key::Space)) {
            if (m_sel == 0) *m_ctx.action = GameAction::StartGame;
            else *m_ctx.action = GameAction::Quit;
        }
    }

    void render(pino::f32) override {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        auto* cam = m_ctx.cam;
        cam->look_at({0, 6, 10}, {0, 0, 0}, {0, 1, 0});

        auto& sh = *m_ctx.shader;
        sh.bind();
        sh.set_mat4("u_view_proj", cam->view_proj());
        sh.set_vec3("u_camera_pos", cam->position());
        pino::AmbientLight amb{{1,1,1}, 0.5f};
        pino::DirectionalLight dir{{0.3f, -0.8f, -0.4f}, {0.8f, 0.8f, 0.9f}};
        pino::upload_ambient_light(sh, amb);
        pino::upload_directional_light(sh, dir);
        sh.set_int("u_num_point_lights", 0);

        auto& mesh = *m_ctx.cube;
        auto draw = [&](const glm::vec3& pos, const glm::vec3& scl, const pino::Material& mat) {
            glm::mat4 m = glm::translate(glm::mat4(1), pos) * glm::scale(glm::mat4(1), scl);
            sh.set_mat4("u_model", m);
            sh.set_mat3("u_normal_matrix", glm::inverseTranspose(glm::mat3(m)));
            pino::upload_material(sh, mat);
            sh.set_int("u_has_diffuse_tex", 0);
            mesh.draw();
        };

        // Title cubes
        pino::Material colors[] = {
            {{0,0.15f,0.15f},{0,0.8f,0.9f},{1,1,1},{0,0,0},32},
            {{0.15f,0.15f,0.15f},{0.85f,0.85f,0.85f},{1,1,1},{0.05f,0.05f,0.05f},16},
            {{0.15f,0.08f,0},{0.9f,0.5f,0},{1,1,1},{0,0,0},32},
            {{0,0.15f,0},{0.2f,0.9f,0.3f},{1,1,1},{0,0,0},32},
        };
        const char* title = "PINO";
        for (int i = 0; title[i]; ++i) {
            int idx = (title[i] == 'P') ? 0 : (title[i] == 'I') ? 1 : (title[i] == 'N') ? 2 : 3;
            draw({-2.4f + i * 1.6f, 2.5f, -2.0f}, {1.2f, 1.2f, 1.2f}, colors[idx]);
        }

        // Menu options
        pino::Material sel{{0.1f,0.05f,0.1f},{0.3f,0.2f,0.8f},{0.8f,0.8f,1},{0,0,0},32};
        pino::Material norm{{0.08f,0.08f,0.08f},{0.25f,0.25f,0.3f},{0.2f,0.2f,0.2f},{0,0,0},8};

        // Start Game button
        draw({0.5f, -0.3f, -2.0f}, {2.0f, 0.5f, 0.4f}, m_sel == 0 ? sel : norm);
        // Quit button
        draw({0.5f, -1.3f, -2.0f}, {2.0f, 0.5f, 0.4f}, m_sel == 1 ? sel : norm);

        // Selection arrow
        float arrow_y = (m_sel == 0) ? -0.3f : -1.3f;
        draw({-1.8f, arrow_y, -1.6f}, {0.2f, 0.2f, 0.2f},
             pino::Material{{0.15f,0.15f,0},{0.9f,0.9f,0.1f},{1,1,1},{0.05f,0.05f,0},32});
    }

private:
    GameContext& m_ctx;
    int m_sel = 0;
};

// ── GameScene ──────────────────────────────────────────────────────────
class GameScene final : public pino::IScene {
public:
    GameScene(GameContext& ctx, CameraShake& shake, ParticlePool& particles,
              SpawnSystem& spawner, pino::CollisionWorld& cw, pino::DebugRenderer& dr)
        : m_ctx(ctx), m_shake(shake), m_part(particles), m_spawn(spawner), m_cw(cw), m_dr(dr) {}

    void init() override {
        m_score = 0; m_lives = INITIAL_LIVES; m_game_over = false;
        m_spawn = SpawnSystem{};
        m_part = ParticlePool{};
        m_cw.clear();
        m_ctx.world->clear();
        m_enemies.clear();
        m_coins.clear();

        auto* root = m_ctx.world->root();
        auto& mesh = *m_ctx.cube;
        auto mn = mesh.local_min(), mx = mesh.local_max();

        // Floor
        m_floor = root->create_child("floor");
        m_floor->local_transform().position = {0, -CUBE_SCALE * 0.5f, 0};
        m_floor->local_transform().scale = {ARENA_HALF * 2 - WALL_T, 0.1f, ARENA_HALF * 2 - WALL_T};
        { pino::ColliderComponent cc; cc.local_min = mn; cc.local_max = mx; cc.is_static = true;
          m_cw.register_collider(*m_floor, cc); }

        // Walls
        float w = ARENA_HALF + WALL_T * 0.5f - CUBE_SCALE * 0.5f;
        auto wpos = [&](const glm::vec3& p, const glm::vec3& s) {
            auto* e = root->create_child("wall");
            e->local_transform().position = p;
            e->local_transform().scale = s;
            pino::ColliderComponent cc; cc.local_min = mn; cc.local_max = mx; cc.is_static = true;
            m_cw.register_collider(*e, cc);
            m_walls.push_back(e);
        };
        wpos({ -w, 0.2f, 0 },  { WALL_T, 0.5f, ARENA_HALF * 2 });
        wpos({  w, 0.2f, 0 },  { WALL_T, 0.5f, ARENA_HALF * 2 });
        wpos({ 0, 0.2f, -w },  { ARENA_HALF * 2, 0.5f, WALL_T });
        wpos({ 0, 0.2f,  w },  { ARENA_HALF * 2, 0.5f, WALL_T });

        // Player
        m_player = root->create_child("player");
        m_player->local_transform().position = {0, 0, 0};
        m_player->local_transform().scale = {CUBE_SCALE, CUBE_SCALE, CUBE_SCALE};
        { pino::ColliderComponent cc; cc.local_min = mn; cc.local_max = mx; cc.is_static = false;
          m_cw.register_collider(*m_player, cc); }

        // Initial enemies
        for (int i = 0; i < 3; ++i) spawn_enemy();
        // Coins
        for (int i = 0; i < 5; ++i) spawn_coin();
    }

    void update(pino::f32 dt) override {
        if (m_game_over) return;

        auto* in = pino::Input::instance();
        float mx = 0, mz = 0;
        if (in->is_key_pressed(pino::Key::W) || in->is_key_pressed(pino::Key::Up))    mz -= 1;
        if (in->is_key_pressed(pino::Key::S) || in->is_key_pressed(pino::Key::Down))  mz += 1;
        if (in->is_key_pressed(pino::Key::A) || in->is_key_pressed(pino::Key::Left))  mx -= 1;
        if (in->is_key_pressed(pino::Key::D) || in->is_key_pressed(pino::Key::Right)) mx += 1;
        if (in->touch_count() > 0) {
            float sx = in->swipe_delta_x(), sy = in->swipe_delta_y();
            if (std::fabs(sx) > 0.02f) mx += sx > 0 ? 1 : -1;
            if (std::fabs(sy) > 0.02f) mz -= sy > 0 ? 1 : -1;
        }
        if (mx || mz) {
            float len = std::sqrt(mx*mx + mz*mz); mx /= len; mz /= len;
            auto p = m_player->local_transform().position;
            p.x += mx * PLAYER_SPEED * dt; p.z += mz * PLAYER_SPEED * dt;
            clamp_arena(p);
            m_player->local_transform().position = p;
        }

        // Spawn
        m_spawn.rate_for(m_score);
        if (m_spawn.ready(dt)) spawn_enemy();

        // Enemy AI — snapshot for safe iteration
        auto ppos = m_player->local_transform().position;
        int ec = static_cast<int>(m_enemies.size());
        std::vector<glm::vec3> epos(ec);
        for (int i = 0; i < ec; ++i)
            epos[i] = m_enemies[i]->local_transform().position;

        for (int i = 0; i < ec; ++i) {
            auto* e = m_enemies[i];
            if (!e->is_active()) continue;
            glm::vec3 d = ppos - epos[i];
            float dist = glm::length(d);
            if (dist > 0.01f) {
                float sp = ENEMY_BASE_SPEED + static_cast<float>(m_score) * ENEMY_SPEED_INCR;
                epos[i] += (d / dist) * sp * dt;
                clamp_arena(epos[i]);
                e->local_transform().position = epos[i];
            }
        }

        // Collision: player vs walls (push-out)
        m_cw.update(dt);

        // Manual checks: player vs enemies, player vs coins
        auto pp = m_player->local_transform().position;

        // Enemies
        for (int i = ec - 1; i >= 0; --i) {
            auto* e = m_enemies[i];
            auto ep = e->local_transform().position;
            if (std::fabs(pp.x - ep.x) < HIT_RADIUS && std::fabs(pp.z - ep.z) < HIT_RADIUS) {
                --m_lives;
                m_shake.trigger(SHAKE_AMP, SHAKE_FREQ_HZ, SHAKE_DUR_SEC);
                m_part.burst(ep, BURST_COUNT, 1.0f, 0.2f, 0.1f);
                // Respawn enemy elsewhere
                e->local_transform().position = arena_pos();
                if (m_lives <= 0) {
                    m_game_over = true;
                    *m_ctx.final_score = m_score;
                    *m_ctx.action = GameAction::GameOver;
                    return;
                }
            }
        }

        // Coins
        for (auto* c : m_coins) {
            auto cp = c->local_transform().position;
            if (std::fabs(pp.x - cp.x) < COIN_RADIUS && std::fabs(pp.z - cp.z) < COIN_RADIUS) {
                ++m_score;
                m_part.burst(cp, 5, 0.85f, 0.75f, 0.1f);
                c->local_transform().position = arena_pos();
            }
        }

        // Camera
        m_ctx.cam->look_at(pp + glm::vec3{0, 6, 9}, pp, {0, 1, 0});
        m_shake.update(dt);
        m_part.update(dt);
    }

    void render(pino::f32) override {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        auto cam = m_ctx.cam;
        // Manually compute shaken view-projection
        glm::mat4 view_proj = cam->projection() * m_shake.shaken_view(cam->view());

        auto& sh = *m_ctx.shader;
        sh.bind();
        sh.set_mat4("u_view_proj", view_proj);
        sh.set_vec3("u_camera_pos", cam->position());

        pino::AmbientLight amb{{1,1,1}, 0.5f};
        pino::DirectionalLight dir{{0.3f, -0.8f, -0.4f}, {0.7f, 0.7f, 0.8f}};
        pino::upload_ambient_light(sh, amb);
        pino::upload_directional_light(sh, dir);
        sh.set_int("u_num_point_lights", 0);

        auto& mesh = *m_ctx.cube;
        auto draw = [&](pino::Entity& e, const pino::Material& m) {
            auto wm = e.world_matrix();
            sh.set_mat4("u_model", wm);
            sh.set_mat3("u_normal_matrix", glm::inverseTranspose(glm::mat3(wm)));
            pino::upload_material(sh, m);
            sh.set_int("u_has_diffuse_tex", 0);
            mesh.draw();
        };

        pino::Material fmat{{0.1f,0.1f,0.1f},{0.22f,0.22f,0.22f},{0.2f,0.2f,0.2f},{0,0,0},4};
        pino::Material wmat{{0.18f,0.18f,0.2f},{0.3f,0.3f,0.35f},{0.3f,0.3f,0.3f},{0,0,0},8};
        pino::Material pmat{{0.05f,0.05f,0.15f},{0.15f,0.5f,0.9f},{1,1,1},{0,0,0},48};
        pino::Material emat{{0.18f,0.03f,0.03f},{0.85f,0.1f,0.1f},{0.7f,0.7f,0.7f},{0,0,0},24};
        pino::Material cmat{{0.12f,0.1f,0.02f},{0.8f,0.7f,0.05f},{1,1,1},{0,0,0},48};

        draw(*m_floor, fmat);
        for (auto* w : m_walls) if (w->is_active()) draw(*w, wmat);
        if (m_player->is_active()) draw(*m_player, pmat);
        for (auto* e : m_enemies) if (e->is_active()) draw(*e, emat);
        for (auto* c : m_coins) if (c->is_active()) draw(*c, cmat);

        // Particles
        m_dr.begin_frame();
        m_part.render(m_dr);
        m_dr.render(view_proj);
        m_dr.end_frame();

        // UI overlay (no depth test)
        glDisable(GL_DEPTH_TEST);
        pino::Material live_mat{{0.12f,0.02f,0.02f},{0.75f,0.1f,0.1f},{0.8f,0.8f,0.8f},{0,0,0},8};
        pino::Material ded_mat{{0.04f,0.04f,0.04f},{0.12f,0.12f,0.12f},{0.2f,0.2f,0.2f},{0,0,0},4};
        pino::Material s_mat{{0.02f,0.05f,0.02f},{0.1f,0.55f,0.2f},{0.5f,0.5f,0.5f},{0,0,0},8};
        glm::mat4 ui_mtx = glm::translate(glm::mat4(1), {-ARENA_HALF, ARENA_HALF + 0.8f, 0});
        for (int i = 0; i < glm::min(m_score, 40); ++i) {
            ui_mtx = glm::translate(glm::mat4(1), {-ARENA_HALF + 0.3f + i * 0.25f, ARENA_HALF + 0.5f, 0});
            glm::mat4 sm = glm::scale(ui_mtx, {0.12f, 0.12f, 0.12f});
            sh.set_mat4("u_model", sm);
            sh.set_mat3("u_normal_matrix", glm::mat3(1));
            pino::upload_material(sh, s_mat);
            sh.set_int("u_has_diffuse_tex", 0);
            mesh.draw();
        }
        for (int i = 0; i < INITIAL_LIVES; ++i) {
            glm::mat4 lm = glm::translate(glm::mat4(1), {ARENA_HALF - 0.8f - i * 0.35f, ARENA_HALF + 0.5f, 0});
            lm = glm::scale(lm, {0.15f, 0.15f, 0.15f});
            sh.set_mat4("u_model", lm);
            sh.set_mat3("u_normal_matrix", glm::mat3(1));
            pino::upload_material(sh, i < m_lives ? live_mat : ded_mat);
            sh.set_int("u_has_diffuse_tex", 0);
            mesh.draw();
        }
        glEnable(GL_DEPTH_TEST);
    }

    void shutdown() override {
        m_cw.clear();
        m_ctx.world->clear();
        m_enemies.clear();
        m_coins.clear();
        m_walls.clear();
        m_player = nullptr; m_floor = nullptr;
    }

private:
    GameContext& m_ctx;
    CameraShake& m_shake;
    ParticlePool& m_part;
    SpawnSystem& m_spawn;
    pino::CollisionWorld& m_cw;
    pino::DebugRenderer& m_dr;

    pino::Entity* m_player = nullptr, * m_floor = nullptr;
    std::vector<pino::Entity*> m_walls, m_enemies, m_coins;
    int m_score = 0, m_lives = INITIAL_LIVES;
    bool m_game_over = false;

    void spawn_enemy() {
        auto* e = m_ctx.world->root()->create_child("enemy");
        e->local_transform().position = arena_pos();
        e->local_transform().scale = {CUBE_SCALE, CUBE_SCALE, CUBE_SCALE};
        // Note: enemies NOT registered in CollisionWorld (manual checks only)
        m_enemies.push_back(e);
        m_spawn.active = static_cast<int>(m_enemies.size());
    }

    void spawn_coin() {
        auto* c = m_ctx.world->root()->create_child("coin");
        c->local_transform().position = arena_pos();
        c->local_transform().scale = {CUBE_SCALE * 0.7f, CUBE_SCALE * 0.7f, CUBE_SCALE * 0.7f};
        m_coins.push_back(c);
    }
};

// ── GameOverScene ──────────────────────────────────────────────────────
class GameOverScene final : public pino::IScene {
public:
    explicit GameOverScene(GameContext& ctx) : m_ctx(ctx) {}
    void init() override { m_sel = 0; }
    void shutdown() override {}

    void update(pino::f32) override {
        auto* in = pino::Input::instance();
        if (in->is_key_just_pressed(pino::Key::Down) || in->is_key_just_pressed(pino::Key::Up) ||
            in->is_key_just_pressed(pino::Key::S) || in->is_key_just_pressed(pino::Key::W))
            m_sel = (m_sel + 1) % 2;
        if (in->is_key_just_pressed(pino::Key::Enter) || in->is_key_just_pressed(pino::Key::Space)) {
            *m_ctx.action = (m_sel == 0) ? GameAction::Restart : GameAction::MainMenu;
        }
    }

    void render(pino::f32) override {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        auto* cam = m_ctx.cam;
        cam->look_at({0, 4, 2}, {0, 0, -1}, {0, 1, 0});

        auto& sh = *m_ctx.shader;
        sh.bind();
        sh.set_mat4("u_view_proj", cam->view_proj());
        sh.set_vec3("u_camera_pos", cam->position());
        pino::AmbientLight amb{{1,1,1}, 0.5f};
        pino::DirectionalLight dir{{0, -0.5f, -0.5f}, {0.8f, 0.8f, 1.0f}};
        pino::upload_ambient_light(sh, amb);
        pino::upload_directional_light(sh, dir);
        sh.set_int("u_num_point_lights", 0);

        auto& mesh = *m_ctx.cube;
        auto draw = [&](const glm::vec3& p, const glm::vec3& s, const pino::Material& m) {
            glm::mat4 wm = glm::translate(glm::mat4(1), p) * glm::scale(glm::mat4(1), s);
            sh.set_mat4("u_model", wm);
            sh.set_mat3("u_normal_matrix", glm::inverseTranspose(glm::mat3(wm)));
            pino::upload_material(sh, m);
            sh.set_int("u_has_diffuse_tex", 0);
            mesh.draw();
        };

        // Game Over title (colored cubes)
        pino::Material red{{0.18f,0.02f,0.02f},{0.85f,0.1f,0.1f},{0.8f,0.8f,0.8f},{0,0,0},32};
        const char* go = "GAMEOVER";
        for (int i = 0; go[i]; ++i)
            draw({-2.8f + i * 0.7f, 1.8f, -1.5f}, {0.4f, 0.4f, 0.4f}, red);

        // Score
        int fs = *m_ctx.final_score;
        pino::Material smat{{0.04f,0.04f,0.08f},{0.3f,0.65f,0.95f},{1,1,1},{0,0,0},32};
        for (int i = 0; i < glm::min(fs, 30); ++i)
            draw({-2.5f + i * 0.2f, 0.8f, -1.5f}, {0.1f, 0.1f, 0.1f}, smat);

        // Buttons
        pino::Material sel{{0.1f,0.05f,0.1f},{0.3f,0.2f,0.8f},{0.8f,0.8f,1},{0,0,0},32};
        pino::Material norm{{0.08f,0.08f,0.08f},{0.25f,0.25f,0.3f},{0.2f,0.2f,0.2f},{0,0,0},8};
        draw({0.3f, -0.5f, -1.5f}, {1.8f, 0.4f, 0.3f}, m_sel == 0 ? sel : norm);
        draw({0.3f, -1.3f, -1.5f}, {1.8f, 0.4f, 0.3f}, m_sel == 1 ? sel : norm);
        // Arrow
        float ay = (m_sel == 0) ? -0.5f : -1.3f;
        draw({-1.6f, ay, -1.2f}, {0.15f, 0.15f, 0.15f},
             pino::Material{{0.12f,0.12f,0},{0.85f,0.85f,0.1f},{1,1,1},{0.05f,0.05f,0},32});
    }

private:
    GameContext& m_ctx;
    int m_sel = 0;
};

// ══════════════════════════════════════════════════════════════════════
//  DemoGame
// ══════════════════════════════════════════════════════════════════════

class DemoGame final : public pino::IGame {
public:
    explicit DemoGame(pino::Engine& engine) : m_eng(engine), m_assets(engine.filesystem()) {}

    bool init() override {
        m_shader = m_assets.load_shader("shaders/lit.vert",
                                        "shaders/lit.frag");
        if (!m_shader) return false;
        m_cube = m_assets.load_mesh("models/cube.obj");
        if (!m_cube) return false;
        if (!m_dr.init()) return false;

        int w = static_cast<int>(m_eng.window().width());
        int h = static_cast<int>(m_eng.window().height());
        m_cam.perspective(45.0f, static_cast<float>(w) / static_cast<float>(h), 0.1f, 50.0f);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glClearColor(0.05f, 0.05f, 0.08f, 1.0f);

        m_ctx.game        = this;
        m_ctx.assets      = &m_assets;
        m_ctx.shader      = m_shader;
        m_ctx.cube        = m_cube;
        m_ctx.cam         = &m_cam;
        m_ctx.scenes      = &m_scenes;
        m_ctx.world       = &m_world;
        m_ctx.lives       = &m_lives;
        m_ctx.score       = &m_score;
        m_ctx.final_score = &m_final_score;
        m_ctx.action      = &m_action;

        m_scenes.push(std::make_unique<MenuScene>(m_ctx));
        m_scenes.flush();
        return true;
    }

    void update(pino::f32 dt) override {
        m_eng.timers().update(dt, !m_eng.is_paused());

        // Profile: start update timer
        auto t0 = now_ns();
        m_scenes.update(dt);
        m_scenes.flush();
        handle_action();
        auto t1 = now_ns();
        float update_ms = static_cast<float>(static_cast<double>(t1 - t0) / 1e6);
        m_update_ms = update_ms;
    }

    void render(pino::f32 dt) override {
        auto t0 = now_ns();
        m_scenes.render(dt);
        auto t1 = now_ns();
        float render_ms = static_cast<float>(static_cast<double>(t1 - t0) / 1e6);
        m_profiler.record(m_update_ms, render_ms, dt);
    }

    void shutdown() override {
        m_profiler.summary();
        m_scenes.clear();
        m_world.clear();
        m_cw.clear();
        m_dr.destroy();
    }

private:
    pino::Engine&         m_eng;
    pino::AssetManager    m_assets;
    pino::Shader*         m_shader  = nullptr;
    pino::Mesh*           m_cube    = nullptr;
    pino::Camera          m_cam;
    pino::SceneManager    m_scenes;
    pino::Scene           m_world;
    pino::CollisionWorld  m_cw;
    pino::DebugRenderer   m_dr;

    CameraShake     m_shake;
    ParticlePool    m_particles;
    SpawnSystem     m_spawner;
    Profiler        m_profiler;

    int  m_lives       = INITIAL_LIVES;
    int  m_score       = 0;
    int  m_final_score = 0;
    float m_update_ms  = 0;

    GameAction m_action = GameAction::None;
    GameContext m_ctx;

    void handle_action() {
        auto a = m_action;
        m_action = GameAction::None;
        switch (a) {
            case GameAction::StartGame:
                m_score = 0; m_lives = INITIAL_LIVES;
                m_scenes.push(std::make_unique<GameScene>(m_ctx, m_shake, m_particles, m_spawner, m_cw, m_dr));
                m_scenes.flush();
                break;
            case GameAction::GameOver:
                m_final_score = m_score;
                m_scenes.push(std::make_unique<GameOverScene>(m_ctx));
                m_scenes.flush();
                break;
            case GameAction::Restart:
                reset_all();
                m_scenes.push(std::make_unique<GameScene>(m_ctx, m_shake, m_particles, m_spawner, m_cw, m_dr));
                m_scenes.flush();
                break;
            case GameAction::MainMenu:
                reset_all();
                m_scenes.push(std::make_unique<MenuScene>(m_ctx));
                m_scenes.flush();
                break;
            case GameAction::Quit:
                m_eng.request_quit();
                break;
            default: break;
        }
    }

    void reset_all() {
        m_scenes.clear();
        m_cw.clear();
        m_world.clear();
        m_shake = CameraShake{};
        m_particles = ParticlePool{};
        m_spawner = SpawnSystem{};
        m_score = 0; m_lives = INITIAL_LIVES; m_final_score = 0;
        m_update_ms = 0;
    }

    // Quit not needed — user closes window
};

int main(int, char**) {
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    pino::Engine engine;
    pino::EngineConfig cfg;
    cfg.app_title = "Pino Demo Game";
    cfg.window_width = 1024; cfg.window_height = 768;
    cfg.resizable = true;
    if (!engine.init(cfg)) return 1;
    DemoGame game(engine);
    engine.run(game);
    return 0;
}
