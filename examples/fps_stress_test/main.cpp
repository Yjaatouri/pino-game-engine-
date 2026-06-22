#include "engine/engine.h"
#include "engine/renderer/debug_overlay.h"
#include "engine/renderer/fps_controller.h"
#include "engine/renderer/light.h"
#include "engine/renderer/render_queue.h"
#include "engine/renderer/font.h"
#include "engine/renderer/text_renderer.h"
#include "engine/ecs/ecs_world.h"
#include "engine/ecs/prefab.h"
#include "engine/ecs/scene_graph.h"
#include "engine/physics/collision_world.h"
#include "engine/core/math_utils.h"
#include "engine/scene/entity.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <cstdlib>
#include <ctime>
#include <unordered_map>

using namespace pino;

// ── Custom components ─────────────────────────────────────────────
struct HealthComponent { float hp = 100, max_hp = 100; };

struct EnemyAIComponent {
    float speed = 3;
    float attack_cooldown = 1;
    float attack_timer = 0;
    float attack_range = 1.8f;
    float attack_damage = 10;
};

struct WeaponComponent {
    float fire_rate = 0.15f;
    float last_fire = 0;
    int ammo = 30, max_ammo = 30;
    float reload_time = 1.5f, reload_timer = 0;
    bool reloading = false;
};

struct CollisionProxy {
    std::unique_ptr<Entity> entity;
    EntityId ecs_id;
    bool is_static = false;
};

// ── Constants ─────────────────────────────────────────────────────
static constexpr float ARENA_HALF   = 10;
static constexpr float WALL_T       = 0.4f;
static constexpr float WALL_H       = 3;
static constexpr float ENTITY_R     = 0.5f;
static constexpr float PLAYER_EYE_Y = 0.6f;
static constexpr float GUN_DMG      = 34;
static constexpr float GUN_RANGE    = 60;
static constexpr int   MAX_ENEMIES  = 30;
static constexpr int   WIN_GOAL     = 20;

static float rng() { return static_cast<float>(std::rand()) / RAND_MAX; }

static glm::vec3 rand_pos(float margin = 2.5f) {
    float h = ARENA_HALF - margin;
    return {(rng()*2-1)*h, PLAYER_EYE_Y, (rng()*2-1)*h};
}

static void clamp_arena(glm::vec3& p) {
    float lim = ARENA_HALF - ENTITY_R;
    p.y = glm::clamp(p.y, 0.1f, 1.5f);
    p.x = glm::clamp(p.x, -lim, lim);
    p.z = glm::clamp(p.z, -lim, lim);
}

static void proxy_sync(Entity* e, const glm::mat4& wm) {
    auto& t = e->local_transform();
    t.position = glm::vec3(wm[3]);
    glm::vec3 s;
    s.x = glm::length(glm::vec3(wm[0]));
    s.y = glm::length(glm::vec3(wm[1]));
    s.z = glm::length(glm::vec3(wm[2]));
    t.scale = s;
    if (s.x > 0 && s.y > 0 && s.z > 0) {
        glm::mat3 rm(glm::vec3(wm[0])/s.x, glm::vec3(wm[1])/s.y, glm::vec3(wm[2])/s.z);
        t.rotation = glm::quat_cast(rm);
    }
}

// ── Game ──────────────────────────────────────────────────────────
class FpsGame final : public IGame {
public:
    explicit FpsGame(Engine& e) : m_engine(e) {}

    bool init() override;
    void update(f32 dt) override;
    void render(f32 /*dt*/) override;
    void shutdown() override;
    void on_context_lost() override;
    void on_context_restored() override;

private:
    EntityId spawn_player();
    EntityId spawn_enemy();
    void     kill_entity(EntityId);
    void     do_shoot(const Ray&);
    void     add_render(EntityId);
    void     sync_proxies();
    void     draw_hud(i32 w, i32 h);

    Engine& m_engine;

    EcsWorld m_world;
    RenderQueue m_rq;
    ComponentPool<HealthComponent> m_pool_hp;
    ComponentPool<EnemyAIComponent> m_pool_ai;
    ComponentPool<WeaponComponent> m_pool_wp;

    CollisionWorld m_cw;
    std::vector<CollisionProxy> m_proxies;

    Camera m_cam;
    FpsController m_fps;

    AssetHandle<Shader> m_shader;
    AssetHandle<Mesh>   m_cube;
    Mesh m_ground;

    Material m_mf, m_mw, m_mp, m_me, m_mo, m_meh;

    AmbientLight m_amb;
    DirectionalLight m_dir;
    PointLight m_pl[2];

    SoundHandle m_sfx_shoot, m_sfx_hit;

    Font m_font;
    TextRenderer m_txt;

    EntityId m_pid = NullEntity;
    f32 m_spawn_timer = 0, m_spawn_rate = 2.5f;
    int m_kills = 0, m_deaths = 0, m_score = 0;
    bool m_alive = false, m_won = false;
    f32 m_respawn_timer = 0;
    static constexpr float RESPAWN_DELAY = 2;
    f32 m_game_time = 0;
    u32 m_total_spawned = 0;

    DebugOverlay m_dbg;
};

// ── Init ──────────────────────────────────────────────────────────
bool FpsGame::init() {
    std::srand(static_cast<u32>(std::time(nullptr)));

    m_shader = m_engine.assets().get_shader("shaders/lit.vert", "shaders/lit.frag");
    if (!m_shader) return false;
    m_cube = m_engine.assets().get_mesh("models/cube.obj");
    if (!m_cube) return false;

    {
        struct V { glm::vec3 p, n; glm::vec2 u; };
        float s = ARENA_HALF;
        V v[] = {{{-s,0,-s},{0,1,0},{0,0}},{{s,0,-s},{0,1,0},{1,0}},
                 {{s,0,s},{0,1,0},{1,1}},{{-s,0,s},{0,1,0},{0,1}}};
        u32 idx[] = {0,1,2,0,2,3};
        m_ground.upload(reinterpret_cast<const Vertex*>(v), 4, idx, 6);
    }

    auto mat = [&](const glm::vec3& a, const glm::vec3& d, const glm::vec3& s, float sh) {
        Material m; m.set_shader(m_shader);
        m.set_uniform("u_mat_ambient", a); m.set_uniform("u_mat_diffuse", d);
        m.set_uniform("u_mat_specular", s); m.set_uniform("u_mat_shininess", sh);
        m.set_uniform("u_has_diffuse_tex", 0);
        return m;
    };
    m_mf  = mat({.15f,.15f,.15f},{.35f,.35f,.35f},{.1f,.1f,.1f},2);
    m_mw  = mat({.2f,.2f,.22f},{.4f,.4f,.45f},{.3f,.3f,.3f},8);
    m_mp  = mat({.1f,.15f,.25f},{.2f,.5f,.9f},{1,1,1},64);
    m_me  = mat({.25f,.05f,.05f},{.9f,.2f,.15f},{.5f,.5f,.5f},24);
    m_mo  = mat({.1f,.07f,.04f},{.45f,.3f,.12f},{.6f,.6f,.6f},16);
    m_meh = mat({.5f,.1f,.1f},{1,.4f,.3f},{.8f,.8f,.8f},32);

    m_amb = {{.8f,.8f,.9f},.35f};
    m_dir = {{.3f,-1,-.4f},{.7f,.7f,.8f}};
    m_pl[0] = {{-5,3,-5},{1,.6f,.3f},1,.09f,.032f};
    m_pl[1] = {{5,3,5},{.3f,.5f,1},1,.09f,.032f};

    i32 fw = static_cast<i32>(m_engine.window().width());
    i32 fh = static_cast<i32>(m_engine.window().height());
    m_cam.perspective(70, static_cast<float>(fw)/fh, .1f, 60);
    m_fps.attach(&m_cam);
    m_fps.set_speed(8, .12f);

    m_sfx_shoot = m_engine.audio().preload("audio/test_tone.wav");
    m_sfx_hit   = m_engine.audio().preload("audio/test.wav");

    m_world.set_render_queue(&m_rq);
    m_world.set_audio_manager(&m_engine.audio());
    auto& reg = m_world.scene().registry();
    m_pool_hp.set_registry(&reg);
    m_pool_ai.set_registry(&reg);
    m_pool_wp.set_registry(&reg);

    auto wall = [&](const glm::vec3& pos, const glm::vec3& scl) {
        EntityId e = m_world.create_entity();
        auto& sg = m_world.scene().scene_graph();
        sg.attach(e);
        sg.set_position(e, pos);
        sg.set_scale(e, scl);
        auto* rc = m_world.scene().get_component<RenderComponent>(e);
        if (!rc) rc = &m_world.scene().add_component<RenderComponent>(e);
        rc->mesh = m_cube;
        rc->material = &m_mw;
        auto& pc = m_world.scene().add_component<PhysicsComponent>(e);
        pc.is_static = true;
    };
    float w = ARENA_HALF + WALL_T*0.5f;
    wall({-w, WALL_H*0.5f, 0},  {WALL_T, WALL_H, ARENA_HALF*2});
    wall({ w, WALL_H*0.5f, 0},  {WALL_T, WALL_H, ARENA_HALF*2});
    wall({0,  WALL_H*0.5f, -w}, {ARENA_HALF*2, WALL_H, WALL_T});
    wall({0,  WALL_H*0.5f,  w}, {ARENA_HALF*2, WALL_H, WALL_T});

    for (int i = 0; i < 6; ++i) {
        glm::vec3 p = rand_pos(3.5f); p.y = 1.5f;
        EntityId e = m_world.create_entity();
        auto& sg = m_world.scene().scene_graph();
        sg.attach(e);
        sg.set_position(e, p);
        sg.set_scale(e, {0.6f, 3, 0.6f});
        auto* rc = m_world.scene().get_component<RenderComponent>(e);
        if (!rc) rc = &m_world.scene().add_component<RenderComponent>(e);
        rc->mesh = m_cube;
        rc->material = &m_mo;
        auto& pc = m_world.scene().add_component<PhysicsComponent>(e);
        pc.is_static = true;
    }

    m_pid = spawn_player();
    m_alive = true;

    for (int i = 0; i < 5; ++i) spawn_enemy();

    if (!m_font.load_builtin()) PINO_WARN("No font");
    if (!m_txt.init(m_engine.window().width(), m_engine.window().height())) PINO_WARN("No text");

    glClearColor(.06f, .06f, .1f, 1);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    PINO_INFO("── FPS Arena ──");
    PINO_INFO("WASD=move  Mouse=look  LMB=shoot  R=reload");
    PINO_INFO("Kill %d enemies to win!  F3=debug  ESC=exit", WIN_GOAL);
    return true;
}

// ── Spawning ──────────────────────────────────────────────────────
void FpsGame::add_render(EntityId e) {
    auto* rc = m_world.scene().get_component<RenderComponent>(e);
    if (!rc) rc = &m_world.scene().add_component<RenderComponent>(e);
    rc->mesh = m_cube;
}

EntityId FpsGame::spawn_player() {
    Prefab pf;
    pf.set_transform({0, PLAYER_EYE_Y, 0});
    pf.set_mesh("models/cube.obj");
    PhysicsComponent pc;
    pc.local_min = {-ENTITY_R, -ENTITY_R, -ENTITY_R};
    pc.local_max = { ENTITY_R,  ENTITY_R,  ENTITY_R};
    pc.is_static = false;
    pf.set_component(pc);
    EntityId e = pf.instantiate(m_world, m_engine.assets());
    add_render(e);
    auto* rc = m_world.scene().get_component<RenderComponent>(e);
    if (rc) rc->material = &m_mp;
    m_pool_hp.add(e) = {100, 100};
    m_pool_wp.add(e) = {};
    return e;
}

EntityId FpsGame::spawn_enemy() {
    Prefab pf;
    pf.set_transform(rand_pos(3));
    pf.set_mesh("models/cube.obj");
    PhysicsComponent pc;
    pc.local_min = {-ENTITY_R, -ENTITY_R, -ENTITY_R};
    pc.local_max = { ENTITY_R,  ENTITY_R,  ENTITY_R};
    pc.is_static = false;
    pf.set_component(pc);
    EntityId e = pf.instantiate(m_world, m_engine.assets());
    add_render(e);
    auto* rc = m_world.scene().get_component<RenderComponent>(e);
    if (rc) rc->material = &m_me;
    m_pool_hp.add(e) = {50, 50};
    m_pool_ai.add(e) = {};
    return e;
}

void FpsGame::kill_entity(EntityId e) {
    m_pool_hp.remove(e);
    m_pool_ai.remove(e);
    m_pool_wp.remove(e);
    m_world.destroy_entity(e);
}

// ── Collision ─────────────────────────────────────────────────────
void FpsGame::sync_proxies() {
    for (auto& p : m_proxies)
        if (p.entity) m_cw.unregister_collider(*p.entity);
    m_proxies.clear();

    auto& sg = m_world.scene().scene_graph();
    m_world.scene().physics_components().each([&](EntityId e, PhysicsComponent& pc) {
        if (!sg.has(e)) return;
        auto proxy = std::make_unique<Entity>("proxy");
        ColliderComponent cc;
        cc.local_min = pc.local_min;
        cc.local_max = pc.local_max;
        cc.is_static = pc.is_static;
        proxy_sync(proxy.get(), sg.world_matrix(e));
        cc.update_world_aabb(proxy->local_transform());
        m_cw.register_collider(*proxy, cc);
        m_proxies.push_back({std::move(proxy), e, pc.is_static});
    });
}

// ── Shooting ──────────────────────────────────────────────────────
void FpsGame::do_shoot(const Ray& ray) {
    RaycastResult hit = m_cw.raycast(ray, GUN_RANGE);
    if (!hit.entity) return;

    EntityId target = NullEntity;
    for (auto& p : m_proxies)
        if (p.entity.get() == hit.entity) { target = p.ecs_id; break; }
    if (target == NullEntity || target == m_pid) return;

    HealthComponent* hp = m_pool_hp.get(target);
    if (!hp) return;
    hp->hp -= GUN_DMG;

    if (m_sfx_hit.is_valid())
        m_engine.audio().play_one_shot_3d("audio/test.wav", hit.point, 0.5f);

    auto* rc = m_world.scene().get_component<RenderComponent>(target);
    if (rc) rc->material = &m_meh;

    if (hp->hp <= 0) {
        m_kills++; m_score += 100;
        if (m_kills >= WIN_GOAL) m_won = true;
        kill_entity(target);
    }
}

// ── Update ────────────────────────────────────────────────────────
void FpsGame::update(f32 dt) {
    m_world.flush_destroyed();
    m_game_time += dt;
    auto& in = m_engine.input();
    auto& sg = m_world.scene().scene_graph();
    auto& au = m_engine.audio();

    if (in.key_pressed(Key::F3)) m_dbg.toggle();
    if (in.key_pressed(Key::Escape)) { m_engine.request_quit(); return; }

    // Win state — freeze
    if (m_won) { sync_proxies(); m_cw.update(dt); return; }

    // Respawn
    if (!m_alive) {
        m_respawn_timer -= dt;
        if (m_respawn_timer <= 0) {
            m_pid = spawn_player(); m_alive = true;
            m_fps.set_position({0, PLAYER_EYE_Y, 0});
            m_cam.set_position({0, PLAYER_EYE_Y, 0});
        }
        sync_proxies(); m_cw.update(dt); return;
    }

    // Player movement
    in.set_cursor_locked(true);
    m_fps.update(in, dt);

    // Clamp position — no flying, no diving
    glm::vec3 front, pos;
    if (m_fps.yaw != -90 || m_fps.pitch != 0) {
        front.x = cos(glm::radians(m_fps.yaw)) * cos(glm::radians(m_fps.pitch));
        front.y = sin(glm::radians(m_fps.pitch));
        front.z = sin(glm::radians(m_fps.yaw)) * cos(glm::radians(m_fps.pitch));
        front = glm::normalize(front);
    } else {
        front = {0, 0, -1};
    }
    pos = m_cam.position();
    clamp_arena(pos);
    m_cam.look_at(pos, pos + front, {0, 1, 0});
    if (sg.has(m_pid)) sg.set_position(m_pid, pos);

    // Audio listener
    au.set_listener_position(m_cam.position());
    au.set_listener_orientation(front, {0, 1, 0});

    // Weapon
    WeaponComponent* wp = m_pool_wp.get(m_pid);
    if (wp) {
        if (wp->reloading) {
            wp->reload_timer -= dt;
            if (wp->reload_timer <= 0) { wp->ammo = wp->max_ammo; wp->reloading = false; }
        }
        if (in.is_mouse_pressed(MouseButton::Left) && !wp->reloading) {
            if (m_game_time - wp->last_fire >= wp->fire_rate && wp->ammo > 0) {
                wp->last_fire = m_game_time; wp->ammo--;
                Ray r; r.origin = m_cam.position();
                r.direction = front;
                do_shoot(r);
                if (m_sfx_shoot.is_valid())
                    au.play_one_shot("audio/test_tone.wav", 0.3f);
            }
        }
        if (in.key_pressed(Key::R) && !wp->reloading && wp->ammo < wp->max_ammo) {
            wp->reloading = true; wp->reload_timer = wp->reload_time;
        }
    }

    // Enemy AI
    m_pool_ai.each([&](EntityId e, EnemyAIComponent& ai) {
        if (!sg.has(e) || !m_alive || !sg.has(m_pid)) return;

        glm::vec3 epos = sg.world_position(e);
        glm::vec3 tpos = sg.world_position(m_pid);
        glm::vec3 dir = tpos - epos;
        float dist = glm::length(dir);
        if (dist < 0.01f) return;
        dir /= dist;

        epos += dir * (ai.speed * dt);
        epos.y = PLAYER_EYE_Y;
        clamp_arena(epos);
        sg.set_position(e, epos);

        float yaw = atan2(dir.x, dir.z);
        sg.set_rotation(e, glm::angleAxis(yaw, glm::vec3(0, 1, 0)));

        ai.attack_timer -= dt;
        if (dist < ai.attack_range && ai.attack_timer <= 0) {
            ai.attack_timer = ai.attack_cooldown;
            HealthComponent* php = m_pool_hp.get(m_pid);
            if (php) {
                php->hp -= ai.attack_damage;
                if (m_sfx_hit.is_valid())
                    au.play_one_shot("audio/test.wav", 0.5f);
                if (php->hp <= 0) {
                    kill_entity(m_pid);
                    m_alive = false;
                    m_deaths++;
                    m_respawn_timer = RESPAWN_DELAY;
                }
            }
        }
        auto* rc = m_world.scene().get_component<RenderComponent>(e);
        if (rc && rc->material == &m_meh) rc->material = &m_me;
    });

    // Spawner
    u32 alive = 0;
    m_pool_ai.each([&](EntityId, EnemyAIComponent&) { alive++; });
    m_spawn_timer -= dt;
    if (m_spawn_timer <= 0 && alive < static_cast<u32>(MAX_ENEMIES)) {
        spawn_enemy(); m_total_spawned++;
        m_spawn_timer = m_spawn_rate;
        if (m_spawn_rate > 0.8f) m_spawn_rate -= 0.02f;
    }

    sync_proxies();
    m_cw.update(dt);
}

// ── Render ────────────────────────────────────────────────────────
void FpsGame::render(f32 /*dt*/) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_shader->bind();
    m_shader->set_mat4("u_view_proj", m_cam.view_proj());
    m_shader->set_vec3("u_camera_pos", m_cam.position());
    upload_ambient_light(*m_shader, m_amb);
    upload_directional_light(*m_shader, m_dir);
    upload_point_lights(*m_shader, m_pl, 2);

    {
        glm::mat4 model = glm::translate(glm::mat4(1), {0, -0.05f, 0});
        glm::mat3 nm = glm::transpose(glm::inverse(glm::mat3(model)));
        m_shader->set_mat4("u_model", model);
        m_shader->set_mat3("u_normal_matrix", nm);
        m_mf.apply();
        m_ground.draw();
    }

    m_rq.clear();
    m_world.scene().update_render(m_rq);
    m_rq.sort();
    m_rq.flush();

    i32 w = static_cast<i32>(m_engine.window().width());
    i32 h = static_cast<i32>(m_engine.window().height());
    draw_hud(w, h);

    if (m_dbg.is_visible()) {
        m_dbg.set_frame_stats(1/m_engine.delta_time(), m_engine.delta_time()*1000, 0, 0);
        m_dbg.set_entity_count(m_world.entity_count());
        m_dbg.set_render_stats(m_rq.command_count(), m_cube->index_count()*m_rq.command_count()/2);
        m_dbg.set_physics_stats(m_cw.collider_count(), 0, 0);
        m_dbg.render(m_txt, m_font, w, h);
    }
    auto& prof = m_engine.profiler();
    if (prof.is_visible()) prof.render(m_txt, m_font, w, h);
}

void FpsGame::draw_hud(i32 w, i32 h) {
    if (!m_font.is_valid() || !m_txt.is_valid()) return;
    m_txt.begin_frame();

    char buf[192];

    // Win message
    if (m_won) {
        std::snprintf(buf, sizeof(buf), "YOU WIN!  Score: %d  (ESC to exit)", m_score);
        m_txt.draw_text(m_font, buf, w*0.5f-160, h*0.5f-20, 0.7f, 0.3f, 1, 0.3f, 1);
        m_txt.render(w, h);
        m_txt.end_frame();
        return;
    }

    // HP bar
    HealthComponent* php = m_pool_hp.get(m_pid);
    if (php && m_alive) {
        float pct = php->hp / php->max_hp;
        std::snprintf(buf, sizeof(buf), "HP: %.0f/%.0f  [", php->hp, php->max_hp);
        int bars = static_cast<int>(pct * 20);
        for (int i = 0; i < 20; ++i) buf[12+i] = (i < bars) ? '|' : '.';
        buf[32] = ']'; buf[33] = 0;
        m_txt.draw_text(m_font, buf, w*0.5f-120, h-40, 0.35f, 1-pct, pct, 0.05f, 1);
    }

    // Weapon
    WeaponComponent* wp = m_pool_wp.get(m_pid);
    if (wp && m_alive) {
        if (wp->reloading)
            std::snprintf(buf, sizeof(buf), "RELOADING...");
        else
            std::snprintf(buf, sizeof(buf), "Ammo: %d/%d", wp->ammo, wp->max_ammo);
        m_txt.draw_text(m_font, buf, 20, h-40, 0.35f, 1, 1, 1, 1);
    }

    // Score board (top-left)
    float kd = (m_deaths > 0) ? static_cast<float>(m_kills) / m_deaths : static_cast<float>(m_kills);
    u32 ec = m_pool_ai.count();
    std::snprintf(buf, sizeof(buf), "SCORE: %d  KILLS: %d/%d  DEATHS: %d  K/D: %.1f  ENEMIES: %u",
                  m_score, m_kills, WIN_GOAL, m_deaths, kd, ec);
    m_txt.draw_text(m_font, buf, 20, 20, 0.35f, 1, 1, 0.5f, 1);

    // Goal
    std::snprintf(buf, sizeof(buf), "GOAL: Kill %d enemies to win!", WIN_GOAL);
    m_txt.draw_text(m_font, buf, 20, 44, 0.25f, 0.7f, 0.7f, 0.7f, 1);

    // Death overlay
    if (!m_alive) {
        std::snprintf(buf, sizeof(buf), "YOU DIED  -  Respawn in %.1f", m_respawn_timer);
        m_txt.draw_text(m_font, buf, w*0.5f-130, h*0.5f, 0.6f, 1, 0.2f, 0.2f, 1);
    }

    m_txt.render(w, h);
    m_txt.end_frame();
}

void FpsGame::shutdown() {
    m_world.destroy();
    m_txt.destroy();
    m_font.destroy();
    m_ground.destroy();
}

void FpsGame::on_context_lost() {
    m_txt.destroy(); m_font.destroy(); m_ground.destroy();
}

void FpsGame::on_context_restored() {
    if (!m_font.load_builtin()) PINO_WARN("No font on restore");
    if (!m_txt.init(m_engine.window().width(), m_engine.window().height())) PINO_WARN("No text on restore");
    struct V { glm::vec3 p, n; glm::vec2 u; };
    float s = ARENA_HALF;
    V v[] = {{{-s,0,-s},{0,1,0},{0,0}},{{s,0,-s},{0,1,0},{1,0}},
             {{s,0,s},{0,1,0},{1,1}},{{-s,0,s},{0,1,0},{0,1}}};
    u32 idx[] = {0,1,2,0,2,3};
    m_ground.upload(reinterpret_cast<const Vertex*>(v), 4, idx, 6);
}

int main(int, char**) {
    Engine engine;
    EngineConfig cfg;
    cfg.app_title = "Pino FPS Arena";
    cfg.window_width = 1280; cfg.window_height = 720;
    cfg.resizable = true; cfg.vsync = true; cfg.fixed_update_rate = 60;
    if (!engine.init(cfg)) return 1;
    FpsGame game(engine);
    engine.run(game);
    return 0;
}
