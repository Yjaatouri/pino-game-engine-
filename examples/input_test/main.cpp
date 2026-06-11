#include "engine/engine.h"
#include "engine/renderer/gl_es3.h"
#include "engine/core/log.h"
#include "engine/assets/asset_manager.h"
#include "engine/renderer/shader.h"
#include "engine/renderer/mesh.h"

#include <glm/gtc/matrix_transform.hpp>

class InputTester : public pino::IGame {
public:
    explicit InputTester(pino::Engine& e) : m_engine(e), m_assets(e.filesystem()) {}

    bool init() override {
        const char* dir = PINO_ASSET_DIR;

        std::string dir_str = dir;
        std::string vert_path = dir_str + "shaders/lit.vert";
        std::string frag_path = dir_str + "shaders/lit.frag";
        m_shader = m_assets.load_shader(vert_path.c_str(), frag_path.c_str());
        if (!m_shader) return false;

        // Cube mesh from .obj
        m_cube = m_assets.load_mesh((std::string(dir) + "models/cube.obj").c_str());
        if (!m_cube) return false;

        // Floor quad
        {
            struct V { glm::vec3 p; glm::vec3 n; glm::vec2 u; };
            V verts[] = {
                {{-1, -1, 0}, {0, 0, 1}, {0, 0}},
                {{ 1, -1, 0}, {0, 0, 1}, {1, 0}},
                {{ 1,  1, 0}, {0, 0, 1}, {1, 1}},
                {{-1,  1, 0}, {0, 0, 1}, {0, 1}},
            };
            pino::u32 idx[] = {0,1,2, 0,2,3};
            m_quad.upload(reinterpret_cast<const pino::Vertex*>(verts), 4, idx, 6);
        }

        m_proj = glm::ortho(-2.0f, 2.0f, -1.5f, 1.5f, -1.0f, 1.0f);
        glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
        glEnable(GL_DEPTH_TEST);
        return true;
    }

    void update(pino::f32) override {
        auto* in = pino::Input::instance();
        if (!in) return;

        // Movement (held keys) — accumulate position each tick
        if (in->is_key_pressed(pino::Key::A)) m_move_x -= 0.02f;
        if (in->is_key_pressed(pino::Key::D)) m_move_x += 0.02f;
        if (in->is_key_pressed(pino::Key::W)) m_move_y += 0.02f;
        if (in->is_key_pressed(pino::Key::S)) m_move_y -= 0.02f;

        // Scale with scroll
        int sy = in->scroll_dy();
        if (sy != 0) m_scale *= (sy > 0 ? 1.1f : 0.9f);
        if (m_scale < 0.1f) m_scale = 0.1f;
        if (m_scale > 5.0f) m_scale = 5.0f;

        // Color toggle on just-pressed
        if (in->is_key_just_pressed(pino::Key::Space)) {
            m_color_index = (m_color_index + 1) % 6;
        }

        // Color on mouse click
        if (in->is_mouse_just_pressed(pino::MouseButton::Left)) {
            m_color_index = (m_color_index + 1) % 6;
        }

        // Color schemes
        static const glm::vec3 colors[] = {
            {1,0.2f,0.2f}, {0.2f,1,0.2f}, {0.2f,0.2f,1},
            {1,1,0.2f},    {1,0.2f,1},     {0.2f,1,1},
        };
        m_color = colors[m_color_index];

        // Pinch → scale (mobile)
        float pinch = in->pinch_delta();
        if (pinch != 0) m_scale *= (1.0f + pinch * 2.0f);
        if (m_scale < 0.1f) m_scale = 0.1f;
        if (m_scale > 5.0f) m_scale = 5.0f;
    }

    void render(pino::f32) override {
        auto* in = pino::Input::instance();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ---- Draw the main cube (responds to WASD + scroll + click) ----
        glm::mat4 model = glm::translate(glm::mat4(1.0f),
                                         glm::vec3(m_move_x, m_move_y, 0));
        model = glm::scale(model, glm::vec3(m_scale * 0.5f));
        model = glm::rotate(model, m_engine.elapsed_time(),
                            glm::vec3(0, 1, 0));

        glm::mat4 vp = glm::mat4(1.0f);
        m_shader->bind();
        m_shader->set_mat4("u_view_proj", m_proj);
        m_shader->set_mat4("u_model", model);
        m_shader->set_mat3("u_normal_matrix", glm::mat3(1.0f));
        m_shader->set_vec3("u_camera_pos", {0,0,1});

        // Simple material
        m_shader->set_vec3("u_ambient.color", {1,1,1});
        m_shader->set_float("u_ambient.intensity", 0.3f);
        m_shader->set_vec3("u_dir_light.direction", {0,0,-1});
        m_shader->set_vec3("u_dir_light.color", {0.3f,0.3f,0.4f});
        m_shader->set_int("u_num_point_lights", 0);
        m_shader->set_vec3("u_mat_ambient", m_color * 0.3f);
        m_shader->set_vec3("u_mat_diffuse", m_color);
        m_shader->set_vec3("u_mat_specular", {1,1,1});
        m_shader->set_float("u_mat_shininess", 32.0f);
        m_shader->set_int("u_has_diffuse_tex", 0);
        m_cube->draw();

        // ---- Draw marker quad at mouse position ----
        if (in) {
            int mx = in->mouse_x(), my = in->mouse_y();
            int win_w = static_cast<int>(m_engine.config().window_width);
            int win_h = static_cast<int>(m_engine.config().window_height);

            // Map mouse to ortho coords [-2,2] x [-1.5, 1.5]
            float nx = 2.0f * (static_cast<float>(mx) / win_w) - 1.0f;
            float ny = 1.0f - 2.0f * (static_cast<float>(my) / win_h);
            float ox = nx * 2.0f;   // ortho X range is [-2, 2]
            float oy = ny * 1.5f;   // ortho Y range is [-1.5, 1.5]

            glm::mat4 mm = glm::translate(glm::mat4(1.0f), {ox, oy, 0});
            mm = glm::scale(mm, {0.1f, 0.1f, 1.0f});
            m_shader->set_mat4("u_model", mm);
            m_shader->set_vec3("u_mat_ambient", {0.2f, 0.2f, 0.2f});
            m_shader->set_vec3("u_mat_diffuse",
                in->is_mouse_pressed(pino::MouseButton::Left)
                    ? glm::vec3{1,0,0} : glm::vec3{1,1,0});
            m_quad.draw();
        }
    }

    void shutdown() override {}

private:
    pino::Engine&      m_engine;
    pino::AssetManager m_assets;
    pino::Shader*      m_shader = nullptr;
    pino::Mesh*    m_cube   = nullptr;
    pino::Mesh     m_quad;
    glm::mat4      m_proj;

    float m_move_x = 0, m_move_y = 0;
    float m_scale  = 1.0f;
    int   m_color_index = 0;
    glm::vec3 m_color{1, 0.2f, 0.2f};
};

int main(int, char**) {
    pino::Engine engine;
    pino::EngineConfig cfg;
    cfg.app_title     = "Input Test — press keys / move mouse / scroll";
    cfg.window_width  = 800;
    cfg.window_height = 600;

    if (!engine.init(cfg)) return 1;

    InputTester game(engine);
    engine.run(game);
    return 0;
}
