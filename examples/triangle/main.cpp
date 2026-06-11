// triangle — uses Shader + Mesh + Camera classes.

#include "engine/engine.h"
#include "engine/renderer/shader.h"
#include "engine/renderer/mesh.h"
#include "engine/renderer/camera.h"
#include <chrono>
#include <thread>

static const char* vs_es   = "#version 300 es\nlayout(location=0)in vec3 aPos;void main(){gl_Position=vec4(aPos,1.0);}";
static const char* vs_core = "#version 330 core\nlayout(location=0)in vec3 aPos;void main(){gl_Position=vec4(aPos,1.0);}";
static const char* fs_es   = "#version 300 es\nprecision mediump float;out vec4 c;void main(){c=vec4(1.0,0.5,0.0,1.0);}";
static const char* fs_core = "#version 330 core\nout vec4 c;void main(){c=vec4(1.0,0.5,0.0,1.0);}";

int main() {
    pino::EngineConfig cfg;
    cfg.app_title    = "Pino Triangle";
    cfg.window_width = 800;
    cfg.window_height= 600;
    cfg.resizable    = true;

    pino::Engine eng;
    if (!eng.init(cfg)) return 1;

    bool es = cfg.gl_es;
    pino::Shader shader;
    if (!shader.load(es ? vs_es : vs_core, es ? fs_es : fs_core)) return 1;

    pino::Vertex verts[3] = {
        {{ 0.0f,  0.5f, 0.0f}, {0,0,1}, {0,0}},
        {{-0.5f, -0.5f, 0.0f}, {0,0,1}, {0,0}},
        {{ 0.5f, -0.5f, 0.0f}, {0,0,1}, {0,0}},
    };
    pino::Mesh mesh;
    mesh.upload(verts, 3, nullptr, 0);

    pino::Camera cam;
    cam.perspective(60, eng.window().aspect(), 0.1f, 100.0f);
    cam.look_at({0,0,2}, {0,0,0}, {0,1,0});
    glm::mat4 model(1.0f);

    while (eng.is_running()) {
        eng.begin_frame();

        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        shader.bind();
        shader.set_mat4("uMVP", cam.view_proj());
        mesh.draw();

        eng.end_frame();
        if (eng.input().key_pressed(pino::Key::Escape)) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    return 0;
}
