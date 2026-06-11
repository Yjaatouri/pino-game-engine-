// scene_graph — 3 cubes in a parent-child hierarchy.
// Root rotates → child orbits → grandchild orbits child.

#include "engine/engine.h"
#include "engine/renderer/shader.h"
#include "engine/renderer/mesh.h"
#include "engine/renderer/texture.h"
#include "engine/renderer/camera.h"
#include "engine/renderer/fps_controller.h"
#include "engine/core/transform.h"
#include "engine/scene/scene.h"

#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <chrono>
#include <thread>

using pino::u32, pino::f32;

// ---------------------------------------------------------------------------
// Shaders (same as cube example)
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
    cfg.app_title    = "Pino Scene Graph";
    cfg.window_width = 1024;
    cfg.window_height= 768;
    cfg.resizable    = true;

    pino::Engine eng;
    if (!eng.init(cfg)) return 1;

    // ---- Shader ----
    bool es = cfg.gl_es;
    pino::Shader shader;
    if (!shader.load(es ? vs_es : vs_core, es ? fs_es : fs_core)) return 1;

    // ---- Cube mesh ----
    std::vector<pino::Vertex> cv;
    std::vector<u32>          ci;
    make_cube(cv, ci);
    pino::Mesh cube_mesh;
    cube_mesh.upload(cv.data(), static_cast<u32>(cv.size()),
                     ci.data(), static_cast<u32>(ci.size()));

    // ---- White texture (so color comes purely from uColor) ----
    pino::Texture white_tex;
    { const unsigned char px[] = {255,255,255,255}; white_tex.upload_rgba(px, 1, 1); }

    // ---- Scene graph ----
    pino::Scene scene;

    // Root — at origin, rotates on Y
    pino::Entity* root = scene.root()->create_child("root");
    root->local_transform().scale = {0.8f, 0.8f, 0.8f};

    // Child — 2.5 units right of root
    pino::Entity* child = root->create_child("child");
    child->local_transform().position = {2.5f, 0.0f, 0.0f};
    child->local_transform().scale = {0.6f, 0.6f, 0.6f};

    // Grandchild — 1.8 units right of child
    pino::Entity* grandchild = child->create_child("grandchild");
    grandchild->local_transform().position = {1.8f, 0.0f, 0.0f};
    grandchild->local_transform().scale = {0.5f, 0.5f, 0.5f};

    // Colors per entity
    struct { pino::Entity* e; glm::vec4 c; } renderables[] = {
        {root,       {1.0f, 0.3f, 0.3f, 1.0f}},
        {child,      {0.3f, 1.0f, 0.3f, 1.0f}},
        {grandchild, {0.3f, 0.5f, 1.0f, 1.0f}},
    };

    // ---- Camera ----
    pino::Camera cam;
    cam.perspective(50.0f, eng.window().aspect(), 0.1f, 100.0f);
    cam.look_at({0, 3, 6}, {0, 0, 0}, {0, 1, 0});

    PINO_INFO("Scene graph — 3 nested rotating cubes, ESC to exit");

    while (eng.is_running()) {
        eng.begin_frame();

        f32 t = eng.elapsed_time();

        // ---- Update transforms ----
        // Root spins on Y
        root->local_transform().rotation = glm::angleAxis(t * 0.6f, glm::vec3(0,1,0));

        // Child spins on Y relative to root
        child->local_transform().rotation = glm::angleAxis(t * 1.0f, glm::vec3(0,1,0));

        // Grandchild spins on all axes
        grandchild->local_transform().rotation =
            glm::angleAxis(t * 1.5f, glm::normalize(glm::vec3(1,1,0)));

        // ---- Render ----
        glClearColor(0.12f, 0.12f, 0.18f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        shader.bind();
        shader.set_int("uTex", 0);
        white_tex.bind(0);

        for (auto& r : renderables) {
            glm::mat4 mvp = cam.view_proj() * r.e->world_matrix();
            shader.set_mat4("uMVP", mvp);
            shader.set_vec4("uColor", r.c);
            cube_mesh.draw();
        }

        eng.end_frame();

        if (eng.input().key_pressed(pino::Key::Escape)) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }

    return 0;
}
