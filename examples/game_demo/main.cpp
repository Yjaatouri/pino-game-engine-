#include "engine/engine.h"
#include "engine/assets/asset_manager.h"
#include "engine/renderer/mesh.h"
#include "engine/renderer/camera.h"
#include "engine/renderer/light.h"
#include "engine/core/transform.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/constants.hpp>
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

class Game final : public pino::IGame {
public:
    explicit Game(pino::Engine& engine) : m_engine(engine), m_assets(engine.filesystem()) {}

    bool init() override;
    void update(pino::f32 dt) override;
    void render(pino::f32 dt) override;
    void shutdown() override;

private:
    pino::Engine&        m_engine;
    pino::AssetManager   m_assets;
    pino::Shader*        m_shader  = nullptr;
    pino::Mesh*          m_cube    = nullptr;
    pino::Mesh           m_sphere;
    pino::Mesh           m_floor;
    pino::Camera         m_cam;
    pino::Transform      m_t_cube, m_t_sphere, m_t_floor;
    pino::Material       m_mat_cube, m_mat_sphere, m_mat_floor;
    pino::PointLight     m_plight;
    pino::DirectionalLight m_dlight;
    pino::AmbientLight   m_ambient;
};

bool Game::init() {
    const char* dir = PINO_ASSET_DIR;

    m_shader = m_assets.load_shader(
        (std::string(dir) + "shaders/lit.vert").c_str(),
        (std::string(dir) + "shaders/lit.frag").c_str());
    if (!m_shader) return false;

    m_cube = m_assets.load_mesh((std::string(dir) + "models/cube.obj").c_str());
    if (!m_cube) return false;

    m_sphere = pino::Mesh::create_sphere(0.5f, 32, 24);
    m_floor  = make_floor();

    m_cam.perspective(45.0f, 1024.0f / 768.0f, 0.1f, 100.0f);

    m_ambient.color     = {1, 1, 1};
    m_ambient.intensity = 0.25f;

    m_dlight.direction = {0.2f, -1.0f, -0.3f};
    m_dlight.color     = {0.6f, 0.6f, 0.7f};

    m_plight.color     = {1.0f, 0.7f, 0.3f};
    m_plight.constant  = 1.0f;
    m_plight.linear    = 0.09f;
    m_plight.quadratic = 0.032f;

    m_mat_floor.ambient   = {0.3f, 0.3f, 0.3f};
    m_mat_floor.diffuse   = {0.6f, 0.6f, 0.6f};
    m_mat_floor.specular  = {0.0f, 0.0f, 0.0f};
    m_mat_floor.shininess = 1.0f;

    m_mat_cube.ambient   = {0.2f, 0.1f, 0.1f};
    m_mat_cube.diffuse   = {0.9f, 0.3f, 0.2f};
    m_mat_cube.specular  = {0.8f, 0.8f, 0.8f};
    m_mat_cube.shininess = 64.0f;

    m_mat_sphere.ambient   = {0.1f, 0.2f, 0.1f};
    m_mat_sphere.diffuse   = {0.2f, 0.7f, 0.3f};
    m_mat_sphere.specular  = {1.0f, 1.0f, 1.0f};
    m_mat_sphere.shininess = 32.0f;

    m_t_floor.position = {0, -0.5f, 0};
    m_t_cube.position  = {-2.0f, 0.0f, 0.0f};
    m_t_cube.scale     = {0.8f, 0.8f, 0.8f};
    m_t_sphere.position = {2.0f, 0.0f, 0.0f};

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);

    return true;
}

void Game::update(pino::f32 dt) {
    // Physics / logic at fixed 60 Hz
    // Rotations are time-based, not dt-based, for smoothness
}

void Game::render(pino::f32 dt) {
    pino::f32 t = m_engine.elapsed_time();

    // Orbit point light
    float r = 3.5f;
    m_plight.position = {r * std::cos(t), 2.5f, r * std::sin(t)};

    // Rotate objects
    m_t_cube.rotation   = glm::angleAxis(t * 0.8f, glm::normalize(glm::vec3{0, 1, 0}));
    m_t_sphere.rotation = glm::angleAxis(t * 0.4f, glm::normalize(glm::vec3{0, 1, 0}));

    // Orbit camera
    glm::vec3 cam_pos{r * 1.5f * std::cos(t * 0.15f), 3.0f, r * 1.5f * std::sin(t * 0.15f)};
    m_cam.look_at(cam_pos, {0, 0, 0}, {0, 1, 0});

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_shader->bind();
    m_shader->set_mat4("u_view_proj", m_cam.view_proj());
    m_shader->set_vec3("u_camera_pos", cam_pos);

    pino::upload_ambient_light(*m_shader, m_ambient);
    pino::upload_directional_light(*m_shader, m_dlight);
    pino::upload_point_lights(*m_shader, &m_plight, 1);

    struct { pino::Mesh* m; pino::Transform* t; pino::Material* mat; } drawables[] = {
        {&m_floor,  &m_t_floor,  &m_mat_floor},
        { m_cube,   &m_t_cube,   &m_mat_cube},
        {&m_sphere, &m_t_sphere, &m_mat_sphere},
    };

    for (auto& d : drawables) {
        glm::mat4 model = d.t->matrix();
        glm::mat3 nm = glm::transpose(glm::inverse(glm::mat3(model)));
        m_shader->set_mat4("u_model", model);
        m_shader->set_mat3("u_normal_matrix", nm);
        pino::upload_material(*m_shader, *d.mat);
        d.m->draw();
    }
}

void Game::shutdown() {
    // Assets are owned by AssetManager, sphere/floor by Game — automatic cleanup
}

// ---- Entry point ----
int main(int, char**) {
    pino::Engine engine;
    pino::EngineConfig cfg;
    cfg.app_title     = "Game Demo (IGame + fixed timestep)";
    cfg.window_width  = 1024;
    cfg.window_height = 768;

    if (!engine.init(cfg)) return 1;

    Game game(engine);
    engine.run(game);
    return 0;
}
