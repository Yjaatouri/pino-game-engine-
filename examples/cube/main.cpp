// cube — textured, rotating cube with orbiting camera.
// Uses Shader + Mesh + Texture + Camera classes.

#include "engine/engine.h"
#include "engine/renderer/shader.h"
#include "engine/renderer/mesh.h"
#include "engine/renderer/texture.h"
#include "engine/renderer/camera.h"
#include "engine/core/transform.h"

#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <chrono>
#include <thread>

using pino::u32;
using pino::f32;

// ---------------------------------------------------------------------------
// Shader sources
// ---------------------------------------------------------------------------
static const char* vs_es = R"(#version 300 es
layout(location=0)in vec3 aPos;
layout(location=1)in vec3 aNormal;
layout(location=2)in vec2 aUV;
uniform mat4 uMVP;
out vec2 vUV;
void main(){gl_Position=uMVP*vec4(aPos,1.0);vUV=aUV;}
)";

static const char* vs_core = R"(#version 330 core
layout(location=0)in vec3 aPos;
layout(location=1)in vec3 aNormal;
layout(location=2)in vec2 aUV;
uniform mat4 uMVP;
out vec2 vUV;
void main(){gl_Position=uMVP*vec4(aPos,1.0);vUV=aUV;}
)";

static const char* fs_es = R"(#version 300 es
precision mediump float;
in vec2 vUV;
uniform sampler2D uTex;
out vec4 fragColor;
void main(){fragColor=texture(uTex,vUV);}
)";

static const char* fs_core = R"(#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
out vec4 fragColor;
void main(){fragColor=texture(uTex,vUV);}
)";

// ---------------------------------------------------------------------------
// Cube geometry builder
// ---------------------------------------------------------------------------
struct CubeFace {
    glm::vec3 normal;
    glm::vec3 pos[4];
};

static void build_cube(std::vector<pino::Vertex>& verts, std::vector<u32>& indices) {
    const CubeFace faces[6] = {
        {{ 1, 0, 0}, {{ 1,-1, 1}, { 1,-1,-1}, { 1, 1,-1}, { 1, 1, 1}}},
        {{-1, 0, 0}, {{-1,-1,-1}, {-1,-1, 1}, {-1, 1, 1}, {-1, 1,-1}}},
        {{ 0, 1, 0}, {{-1, 1, 1}, {-1, 1,-1}, { 1, 1,-1}, { 1, 1, 1}}},
        {{ 0,-1, 0}, {{-1,-1,-1}, {-1,-1, 1}, { 1,-1, 1}, { 1,-1,-1}}},
        {{ 0, 0, 1}, {{-1,-1, 1}, { 1,-1, 1}, { 1, 1, 1}, {-1, 1, 1}}},
        {{ 0, 0,-1}, {{-1,-1,-1}, {-1, 1,-1}, { 1, 1,-1}, { 1,-1,-1}}},
    };
    const glm::vec2 uvs[4] = {{0,0},{1,0},{1,1},{0,1}};

    for (auto& f : faces) {
        u32 base = static_cast<u32>(verts.size());
        for (int i = 0; i < 4; ++i) {
            verts.push_back({f.pos[i], f.normal, uvs[i]});
        }
        indices.push_back(base+0); indices.push_back(base+1); indices.push_back(base+2);
        indices.push_back(base+0); indices.push_back(base+2); indices.push_back(base+3);
    }
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int main() {
    pino::EngineConfig cfg;
    cfg.app_title    = "Pino Cube";
    cfg.window_width = 800;
    cfg.window_height= 600;
    cfg.resizable    = true;

    pino::Engine eng;
    if (!eng.init(cfg)) return 1;

    // ---- Shader ----
    bool es = cfg.gl_es;
    pino::Shader shader;
    if (!shader.load(es ? vs_es : vs_core, es ? fs_es : fs_core)) return 1;

    // ---- Mesh ----
    std::vector<pino::Vertex> verts;
    std::vector<u32>          idxs;
    build_cube(verts, idxs);

    pino::Mesh mesh;
    mesh.upload(verts.data(), static_cast<u32>(verts.size()),
                idxs.data(),  static_cast<u32>(idxs.size()));

    // ---- Texture ----
    pino::Texture tex;
    tex.make_checkerboard(64, 8);

    // ---- Camera ----
    pino::Camera cam;
    cam.perspective(60.0f, eng.window().aspect(), 0.1f, 100.0f);

    PINO_INFO("Cube example running — ESC to exit");

    while (eng.is_running()) {
        eng.begin_frame();

        // Orbit camera
        f32 t = eng.elapsed_time();
        glm::vec3 eye(cos(t * 0.4f) * 3.5f, 1.5f + sin(t * 0.2f) * 0.5f,
                      sin(t * 0.4f) * 3.5f);
        cam.look_at(eye, glm::vec3(0.0f), glm::vec3(0, 1, 0));

        // Model matrix — slowly rotate cube via Transform
        pino::Transform model;
        model.rotate(t * 0.3f, {0, 1, 0});
        model.rotate(t * 0.15f, {1, 0, 0});

        // Render
        glClearColor(0.15f, 0.15f, 0.20f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        shader.bind();
        shader.set_mat4("uMVP", cam.view_proj() * model.matrix());
        shader.set_int("uTex", 0);
        tex.bind(0);
        mesh.draw();

        eng.end_frame();

        if (eng.input().key_pressed(pino::Key::Escape)) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    return 0;
}
