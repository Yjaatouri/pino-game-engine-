#include "engine/engine.h"
#include "engine/renderer/gl_es3.h"
#include "engine/renderer/shader.h"
#include "engine/renderer/mesh.h"
#include "engine/input/input_map.h"
#include "engine/input/gamepad.h"

#include <glm/gtc/matrix_transform.hpp>
#include <cstdio>

class InputTester : public pino::IGame {
public:
    explicit InputTester(pino::Engine& e) : m_engine(e) {}

    bool init() override {
        // Wire gamepad manager to InputMap
        m_input_map.set_gamepad_manager(&m_engine.gamepad());

        // Bind keyboard + gamepad to same actions
        m_input_map.bind_key("left",    pino::Key::A);
        m_input_map.bind_key("right",   pino::Key::D);
        m_input_map.bind_key("up",      pino::Key::W);
        m_input_map.bind_key("down",    pino::Key::S);
        m_input_map.bind_key("color",   pino::Key::Space);
        m_input_map.bind_gamepad_button("color", pino::GamepadButton::A);
        m_input_map.bind_mouse_button("color", pino::MouseButton::Left);

        // Gamepad-only: color via triggers
        m_input_map.bind_gamepad_button("color_pad", pino::GamepadButton::B);

        m_shader = m_engine.assets().get_shader("shaders/lit.vert", "shaders/lit.frag");
        if (!m_shader) return false;
        m_cube = m_engine.assets().get_mesh("models/cube.obj");
        if (!m_cube) return false;

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
        {
            struct V { glm::vec3 p; glm::vec3 n; glm::vec2 u; };
            V verts[] = {
                {{0,0,0},{0,0,1},{0,0}}, {{1,0,0},{0,0,1},{1,0}},
                {{1,1,0},{0,0,1},{1,1}}, {{0,1,0},{0,0,1},{0,1}},
            };
            pino::u32 idx[] = {0,1,2,0,2,3};
            m_text_bg.upload(reinterpret_cast<const pino::Vertex*>(verts), 4, idx, 6);
        }

        m_proj = glm::ortho(-2.0f, 2.0f, -1.5f, 1.5f, -1.0f, 1.0f);
        glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
        glEnable(GL_DEPTH_TEST);
        return true;
    }

    void update(pino::f32) override {
        auto* in = pino::Input::instance();
        if (!in) return;

        // ── Via InputMap (works across keyboard + gamepad) ──
        if (m_input_map.action_pressed("left"))  m_move_x -= 0.02f;
        if (m_input_map.action_pressed("right")) m_move_x += 0.02f;
        if (m_input_map.action_pressed("up"))    m_move_y += 0.02f;
        if (m_input_map.action_pressed("down"))  m_move_y -= 0.02f;

        // Color toggle via InputMap (key Space, gamepad A, or mouse Left)
        if (m_input_map.action_just_pressed("color")) {
            m_color_index = (m_color_index + 1) % 6;
        }

        // Scale with mouse scroll
        int sy = in->scroll_dy();
        if (sy != 0) m_scale *= (sy > 0 ? 1.1f : 0.9f);
        if (m_scale < 0.1f) m_scale = 0.1f;
        if (m_scale > 5.0f) m_scale = 5.0f;

        // Gamepad stick → direct state query
        auto* gs = m_engine.gamepad().get_state(0);
        if (gs) {
            m_move_x += gs->left_stick_x() * 0.02f;
            m_move_y += gs->left_stick_y() * 0.02f;
        }

        // Pinch → scale (mobile)
        float pinch = in->pinch_delta();
        if (pinch != 0) m_scale *= (1.0f + pinch * 2.0f);
        if (m_scale < 0.1f) m_scale = 0.1f;
        if (m_scale > 5.0f) m_scale = 5.0f;

        static const glm::vec3 colors[] = {
            {1,0.2f,0.2f}, {0.2f,1,0.2f}, {0.2f,0.2f,1},
            {1,1,0.2f},    {1,0.2f,1},     {0.2f,1,1},
        };
        m_color = colors[m_color_index];
    }

    void render(pino::f32) override {
        auto* in = pino::Input::instance();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ---- Main cube ----
        glm::mat4 model = glm::translate(glm::mat4(1.0f),
                                         glm::vec3(m_move_x, m_move_y, 0));
        model = glm::scale(model, glm::vec3(m_scale * 0.5f));
        model = glm::rotate(model, m_engine.elapsed_time(), glm::vec3(0, 1, 0));

        m_shader->bind();
        m_shader->set_mat4("u_view_proj", m_proj);
        m_shader->set_mat4("u_model", model);
        m_shader->set_mat3("u_normal_matrix", glm::mat3(1.0f));
        m_shader->set_vec3("u_camera_pos", {0,0,1});
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

        // ---- Marker quad at mouse ----
        if (in) {
            int mx = in->mouse_x(), my = in->mouse_y();
            int win_w = static_cast<int>(m_engine.config().window_width);
            int win_h = static_cast<int>(m_engine.config().window_height);
            float nx = 2.0f * (static_cast<float>(mx) / win_w) - 1.0f;
            float ny = 1.0f - 2.0f * (static_cast<float>(my) / win_h);
            float ox = nx * 2.0f;
            float oy = ny * 1.5f;

            glm::mat4 mm = glm::translate(glm::mat4(1.0f), {ox, oy, 0});
            mm = glm::scale(mm, {0.1f, 0.1f, 1.0f});
            m_shader->set_mat4("u_model", mm);
            m_shader->set_vec3("u_mat_diffuse",
                in->is_mouse_pressed(pino::MouseButton::Left)
                    ? glm::vec3{1,0,0} : glm::vec3{1,1,0});
            m_text_bg.draw();
        }

        // ---- Gamepad state info (top-left HUD) ----
        auto* gs = m_engine.gamepad().get_state(0);
        if (gs) {
            // Show axis values via color-coded corner quads
            auto draw_corner = [&](float x, float y, glm::vec3 col) {
                glm::mat4 m = glm::translate(glm::mat4(1.0f), {x, y, 0});
                m = glm::scale(m, {0.1f, 0.1f, 1.0f});
                m_shader->set_mat4("u_model", m);
                m_shader->set_vec3("u_mat_diffuse", col);
                m_text_bg.draw();
            };
            draw_corner(-1.8f,  1.3f, {gs->left_stick_x(), 0, 0});
            draw_corner(-1.8f,  1.1f, {0, gs->left_stick_y(), 0});
            draw_corner(-1.8f,  0.9f, {gs->right_stick_x(), 0, gs->right_stick_y()});
            draw_corner(-1.8f,  0.7f, {gs->left_trigger(), gs->right_trigger(), 0});
        }

        // Can't easily render text without a font system in this example,
        // so we use color-coded quads to indicate gamepad state.
    }

    void shutdown() override {}

private:
    pino::Engine&                m_engine;
    pino::AssetHandle<pino::Shader> m_shader;
    pino::AssetHandle<pino::Mesh>   m_cube;
    pino::Mesh           m_quad;
    pino::Mesh           m_text_bg;
    glm::mat4            m_proj;
    pino::InputMap       m_input_map;
    float m_move_x = 0, m_move_y = 0;
    float m_scale  = 1.0f;
    int   m_color_index = 0;
    glm::vec3 m_color{1, 0.2f, 0.2f};
};

int main(int, char**) {
    pino::Engine engine;
    pino::EngineConfig cfg;
    cfg.app_title     = "Input Test — keys/mouse/gamepad/InputMap";
    cfg.window_width  = 800;
    cfg.window_height = 600;

    if (!engine.init(cfg)) return 1;

    InputTester game(engine);
    engine.run(game);
    return 0;
}
