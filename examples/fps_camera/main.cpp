// fps_camera — WASD movement + mouse look with FpsController.
// Scene: checkerboard ground + scattered cubes.

#include "engine/engine.h"
#include "engine/renderer/shader.h"
#include "engine/renderer/mesh.h"
#include "engine/renderer/texture.h"
#include "engine/renderer/camera.h"
#include "engine/renderer/fps_controller.h"
#include "engine/core/transform.h"

#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <chrono>
#include <thread>
#include <SDL.h>
#undef main

using pino::u32;
using pino::f32;

// ---------------------------------------------------------------------------
// Shaders
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
uniform vec4 uColor;
out vec4 fragColor;
void main(){fragColor=texture(uTex,vUV)*uColor;}
)";
static const char* fs_core = R"(#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
uniform vec4 uColor;
out vec4 fragColor;
void main(){fragColor=texture(uTex,vUV)*uColor;}
)";

// ---------------------------------------------------------------------------
// Cube geometry
// ---------------------------------------------------------------------------
static void make_cube(std::vector<pino::Vertex>& verts, std::vector<u32>& idxs) {
    struct Face { glm::vec3 n; glm::vec3 p[4]; };
    const Face faces[6] = {
        {{ 1,0,0}, {{ 1,-1, 1},{ 1,-1,-1},{ 1, 1,-1},{ 1, 1, 1}}},
        {{-1,0,0}, {{-1,-1,-1},{-1,-1, 1},{-1, 1, 1},{-1, 1,-1}}},
        {{ 0,1,0}, {{-1, 1, 1},{-1, 1,-1},{ 1, 1,-1},{ 1, 1, 1}}},
        {{ 0,-1,0},{{-1,-1,-1},{-1,-1, 1},{ 1,-1, 1},{ 1,-1,-1}}},
        {{ 0,0,1}, {{-1,-1, 1},{ 1,-1, 1},{ 1, 1, 1},{-1, 1, 1}}},
        {{ 0,0,-1},{{-1,-1,-1},{-1, 1,-1},{ 1, 1,-1},{ 1,-1,-1}}},
    };
    const glm::vec2 uvs[4] = {{0,0},{1,0},{1,1},{0,1}};
    for (auto& f : faces) {
        u32 base = static_cast<u32>(verts.size());
        for (int i = 0; i < 4; ++i) verts.push_back({f.p[i], f.n, uvs[i]});
        idxs.push_back(base+0); idxs.push_back(base+1); idxs.push_back(base+2);
        idxs.push_back(base+0); idxs.push_back(base+2); idxs.push_back(base+3);
    }
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int main() {
    pino::EngineConfig cfg;
    cfg.app_title    = "Pino FPS Camera";
    cfg.window_width = 1024;
    cfg.window_height= 768;
    cfg.resizable    = true;

    pino::Engine eng;
    if (!eng.init(cfg)) return 1;

    // ---- Shader ----
    bool es = cfg.gl_es;
    pino::Shader shader;
    if (!shader.load(es ? vs_es : vs_core, es ? fs_es : fs_core)) return 1;

    // ---- Cube mesh (reused) ----
    std::vector<pino::Vertex> cv;
    std::vector<u32>          ci;
    make_cube(cv, ci);
    pino::Mesh cube_mesh;
    cube_mesh.upload(cv.data(), static_cast<u32>(cv.size()),
                     ci.data(), static_cast<u32>(ci.size()));

    // ---- Textures ----
    pino::Texture check_tex;
    check_tex.make_checkerboard(32, 4);

    pino::Texture color_tex;
    {
        const unsigned char pixels[] = {255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,0,255};
        color_tex.upload_rgba(pixels, 2, 2);
    }

    // ---- Scene objects ----
    struct SceneObj {
        pino::Transform xf;
        const pino::Texture* tex = nullptr;
        glm::vec4 color = {1,1,1,1};
    };
    std::vector<SceneObj> objects;

    // Ground — a flat 1x1 quad stretched to 20x20
    {
        SceneObj obj;
        obj.xf.position = {0, -2, 0};
        obj.xf.scale    = {20, 1, 20};
        obj.xf.rotation = glm::angleAxis(glm::radians(-90.0f), glm::vec3(1,0,0));
        obj.tex = &check_tex;
        objects.push_back(obj);
    }

    // Scattered cubes
    struct { f32 x, z; glm::vec4 c; } cubes[] = {
        { 0,  0, {1,0.5f,0,1}},
        { 3,  0, {0,1,0.5f,1}},
        {-3,  0, {0.5f,0,1,1}},
        { 0,  3, {1,1,0,1}},
        { 0, -3, {0,1,1,1}},
        { 2,  2, {1,0,0,1}},
        {-2, -2, {0,0,1,1}},
    };
    for (auto& c : cubes) {
        SceneObj obj;
        obj.xf.position = {c.x, -1, c.z};
        obj.xf.scale    = {0.8f, 0.8f, 0.8f};
        obj.tex = &color_tex;
        obj.color = c.c;
        objects.push_back(obj);
    }

    // ---- Camera + FPS controller ----
    pino::Camera cam;
    cam.perspective(60.0f, eng.window().aspect(), 0.1f, 100.0f);

    pino::FpsController fps;
    fps.attach(&cam);
    fps.set_speed(6.0f, 0.15f);
    fps.set_position({0, 1, 5});

    // Grab mouse pointer
    SDL_SetRelativeMouseMode(SDL_TRUE);

    PINO_INFO("FPS camera — WASD to move, mouse to look, ESC to exit");

    while (eng.is_running()) {
        eng.begin_frame();

        // ---- FPS update ----
        fps.update(eng.input(), eng.delta_time());

        // Toggle mouse grab on Tab
        if (eng.input().key_pressed(pino::Key::Tab)) {
            bool grabbed = SDL_GetRelativeMouseMode();
            SDL_SetRelativeMouseMode(grabbed ? SDL_FALSE : SDL_TRUE);
        }

        // ---- Render ----
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        shader.bind();
        shader.set_int("uTex", 0);

        for (auto& obj : objects) {
            shader.set_mat4("uMVP", cam.view_proj() * obj.xf.matrix());
            shader.set_vec4("uColor", obj.color);
            if (obj.tex) obj.tex->bind(0);
            cube_mesh.draw();
        }

        eng.end_frame();

        if (eng.input().key_pressed(pino::Key::Escape)) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }

    SDL_SetRelativeMouseMode(SDL_FALSE);
    return 0;
}
