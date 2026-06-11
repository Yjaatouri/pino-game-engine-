#include "engine/engine.h"
#include "engine/assets/asset_manager.h"
#include "engine/renderer/camera.h"
#include "engine/core/transform.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

int main(int, char**) {
    pino::Engine engine;
    pino::EngineConfig cfg;
    cfg.app_title     = "Asset Demo — .obj + shaders + textures";
    cfg.window_width  = 1024;
    cfg.window_height = 768;

    if (!engine.init(cfg)) return 1;

    pino::AssetManager assets(engine.filesystem());

    const char* dir = PINO_ASSET_DIR;

    auto* mesh   = assets.load_mesh(  (std::string(dir) + "models/cube.obj").c_str());
    auto* shader = assets.load_shader((std::string(dir) + "shaders/textured.vert").c_str(),
                                      (std::string(dir) + "shaders/textured.frag").c_str());
    auto* tex    = assets.load_texture((std::string(dir) + "textures/checker.ppm").c_str());

    if (!mesh || !shader || !tex) {
        PINO_ERROR("Failed to load assets");
        return 1;
    }

    pino::Camera cam;
    cam.perspective(45.0f, 1024.0f / 768.0f, 0.1f, 100.0f);
    cam.look_at({0, 0, 3}, {0, 0, 0}, {0, 1, 0});

    pino::Transform xform;
    xform.position = {0, 0, 0};
    xform.scale    = {1, 1, 1};

    while (engine.is_running()) {
        engine.begin_frame();

        float t = engine.elapsed_time();
        xform.rotation = glm::angleAxis(t, glm::normalize(glm::vec3{0, 1, 0}));

        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        shader->bind();
        shader->set_mat4("u_mvp", cam.view_proj() * xform.matrix());
        shader->set_int("u_tex", 0);

        tex->bind(0);
        mesh->draw();

        engine.end_frame();
    }

    engine.shutdown();
    return 0;
}
