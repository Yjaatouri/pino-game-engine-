#if defined(__ANDROID__)

#include "engine/engine.h"
#include <android_native_app_glue.h>
#include <android/log.h>
#include "engine/renderer/gl_es3.h"
#include "engine/renderer/camera.h"
#include "engine/renderer/light.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

struct AppState {
    pino::Engine*                    engine   = nullptr;
    pino::IGame*                     game     = nullptr;
    pino::AssetHandle<pino::Shader>  shader;
    pino::AssetHandle<pino::Mesh>    cube;
    pino::AssetHandle<pino::Texture> texture;
    pino::Camera                     camera;
    pino::PhongMaterial                   material;

    bool has_window  = false;
    bool initialized = false;
    bool focused     = false;
    bool destroyed   = false;
    float cube_angle = 0.0f;
};

// ─── App command callback ────────────────────────────────────────────────────
static void handle_cmd(struct android_app* app, int32_t cmd) {
    auto* state = static_cast<AppState*>(app->userData);
    if (!state || !state->engine) return;

    auto& win = state->engine->window();

    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            state->has_window = true;
            if (state->initialized) {
                // Surface recreated after TERM_WINDOW — restore EGL surface
                win.recreate(app->window);
            }
            break;

        case APP_CMD_TERM_WINDOW:
            state->has_window = false;
            if (state->initialized) {
                // Destroy EGL surface but keep context (if device allows)
                win.destroy_surface();
            }
            break;

        case APP_CMD_GAINED_FOCUS:
            state->focused = true;
            if (state->initialized) {
                state->engine->resume();
            }
            break;

        case APP_CMD_LOST_FOCUS:
            state->focused = false;
            if (state->initialized) {
                state->engine->pause();
            }
            break;

        case APP_CMD_RESUME:
            // No-op: surface restored in INIT_WINDOW, focus in GAINED_FOCUS
            break;

        case APP_CMD_PAUSE:
            // Input state reset handled by LOST_FOCUS
            break;

        case APP_CMD_DESTROY:
            state->destroyed = true;
            break;

        default:
            break;
    }
}

// ─── Input callback ──────────────────────────────────────────────────────────
static int32_t handle_input(struct android_app* app, AInputEvent* event) {
    auto* state = static_cast<AppState*>(app->userData);
    if (state && state->engine) {
        state->engine->input().process_event(event);
    }
    return 1;
}

// ─── Simple game class ───────────────────────────────────────────────────────
class LitCubeGame : public pino::IGame {
public:
    AppState& state;

    explicit LitCubeGame(AppState& s) : state(s) {}

    bool init() override {
        auto& am = state.engine->assets();

        state.shader = am.get_shader("shaders/lit.vert", "shaders/lit.frag");
        if (!state.shader) return false;

        state.cube = am.get_mesh("models/cube.obj");
        if (!state.cube) return false;

        state.texture = am.get_texture("textures/checker.ppm");
        if (!state.texture) return false;

        state.camera.perspective(
            pino::radians(60.0f),
            state.engine->window().aspect(),
            0.1f, 100.0f
        );
        state.camera.look_at({3, 2, 4}, {0, 0, 0}, {0, 1, 0});

        state.material.ambient   = {0.3f, 0.3f, 0.3f};
        state.material.diffuse   = {0.8f, 0.8f, 0.8f};
        state.material.specular  = {1.0f, 1.0f, 1.0f};
        state.material.shininess = 32.0f;

        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glEnable(GL_DEPTH_TEST);
        return true;
    }

    void update(pino::f32 dt) override {
        state.cube_angle += dt * 0.5f;
    }

    void render(pino::f32) override {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        auto* sh = state.shader;
        sh->bind();

        sh->set_mat4("u_view_proj", state.camera.view_proj());

        auto model = glm::rotate(glm::mat4(1.0f), state.cube_angle, glm::vec3(0, 1, 0));
        sh->set_mat4("u_model", model);

        auto normal = glm::mat3(glm::transpose(glm::inverse(model)));
        sh->set_mat3("u_normal_matrix", normal);

        sh->set_vec3("u_camera_pos", state.camera.position());

        pino::upload_ambient(*sh, {1, 1, 1}, 0.3f);
        pino::upload_directional(*sh, {0, -1, -1}, {0.8f, 0.8f, 1.0f});
        sh->set_int("u_num_point_lights", 0);

        pino::upload_material(*sh, state.material);

        state.texture->bind(0);
        sh->set_int("u_diffuse_tex", 0);
        sh->set_int("u_has_diffuse_tex", 1);

        state.cube->draw();
    }

    void shutdown() override {}

    void on_context_lost() override {
        PINO_INFO("Context lost — invalidating GPU resources");
        state.engine->assets().invalidate_all();
        state.shader = {};
        state.cube   = {};
        state.texture = {};
    }

    void on_context_restored() override {
        PINO_INFO("Context restored — re-uploading GPU resources");
        auto& am = state.engine->assets();

        state.shader = am.get_shader("shaders/lit.vert", "shaders/lit.frag");
        state.cube   = am.get_mesh("models/cube.obj");
        state.texture = am.get_texture("textures/checker.ppm");

        state.camera.perspective(
            pino::radians(60.0f),
            state.engine->window().aspect(),
            0.1f, 100.0f
        );

        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glEnable(GL_DEPTH_TEST);
    }
};

// ─── Entry point ─────────────────────────────────────────────────────────────
void android_main(struct android_app* app) {
    app_dummy();

    AppState state;
    app->userData     = &state;
    app->onAppCmd     = handle_cmd;
    app->onInputEvent = handle_input;

    pino::Engine engine;
    state.engine = &engine;

    LitCubeGame game(state);
    state.game = &game;

    // Frame timing: clamp delta to handle thermal throttling / slow frames
    constexpr float MAX_DT   = 0.25f;   // 4 FPS minimum
    constexpr float MIN_DT   = 0.0001f; // 10,000 FPS cap

    // Event loop
    while (!state.destroyed) {
        // Process all pending events — use 0 timeout when we have a window
        // (so we render every frame), -1 when we don't (block until event)
        int timeout = (state.has_window && state.focused) ? 0 : -1;
        int events;
        struct android_poll_source* source;
        while (ALooper_pollAll(timeout, nullptr, &events,
                               reinterpret_cast<void**>(&source)) >= 0) {
            if (source) source->process(app, source);
            if (state.destroyed) break;
        }

        if (state.destroyed) break;

        // Initialize once we have a window
        if (state.has_window && !state.initialized) {
            pino::EngineConfig cfg;
            cfg.app_title        = "Pino Engine";
            cfg.window_width     = 0;
            cfg.window_height    = 0;
            cfg.native_window    = app->window;
            cfg.asset_manager    = app->activity->assetManager;
            cfg.fixed_update_rate = 60;
            cfg.vsync            = true;

            if (!engine.init(cfg)) {
                __android_log_print(ANDROID_LOG_ERROR, "PinoEngine",
                                    "Engine init failed");
                state.destroyed = true;
                break;
            }

            if (!game.init()) {
                __android_log_print(ANDROID_LOG_ERROR, "PinoEngine",
                                    "Game init failed");
                state.destroyed = true;
                break;
            }

            state.initialized = true;
        }

        // Handle context loss detection — some devices lose the EGL
        // context even with our surface-preserving approach.
        if (state.initialized && state.has_window && engine.window().needs_context_restore()) {
            game.on_context_lost();
            game.on_context_restored();
        }

        // Run game step while we have window and focus
        if (state.initialized && state.has_window) {
            // Thermal-throttle-safe frame step
            engine.step_game(game);
    }
}

    // Cleanup
    if (state.initialized) {
        game.shutdown();
        engine.shutdown();
    }
}

#endif // __ANDROID__
