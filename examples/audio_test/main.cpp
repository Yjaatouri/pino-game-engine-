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
#include <cmath>
#include <SDL.h>
#undef main

using pino::u32;
using pino::u64;
using pino::f32;

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

int main() {
    pino::EngineConfig cfg;
    cfg.app_title    = "Pino Audio Test";
    cfg.window_width = 1024;
    cfg.window_height= 768;
    cfg.resizable    = true;

    pino::Engine eng;
    if (!eng.init(cfg)) return 1;

    bool es = cfg.gl_es;
    pino::Shader shader;
    if (!shader.load(es ? vs_es : vs_core, es ? fs_es : fs_core)) return 1;

    std::vector<pino::Vertex> cv;
    std::vector<u32>          ci;
    make_cube(cv, ci);
    pino::Mesh cube_mesh;
    cube_mesh.upload(cv.data(), static_cast<u32>(cv.size()),
                     ci.data(), static_cast<u32>(ci.size()));

    pino::Texture color_tex;
    {
        const unsigned char pixels[] = {255,128,0,255, 0,200,255,255, 200,0,255,255, 255,255,0,255};
        color_tex.upload_rgba(pixels, 2, 2);
    }

    std::vector<pino::Vertex> gv;
    std::vector<u32>          gi;
    {
        const glm::vec3 gpos[4] = {{-10,0,-10}, {10,0,-10}, {10,0,10}, {-10,0,10}};
        const glm::vec3 gn(0,1,0);
        const glm::vec2 guvs[4] = {{0,0},{10,0},{10,10},{0,10}};
        for (int i = 0; i < 4; ++i) gv.push_back({gpos[i], gn, guvs[i]});
        gi = {0,1,2, 0,2,3};
    }
    pino::Mesh ground_mesh;
    ground_mesh.upload(gv.data(), static_cast<u32>(gv.size()),
                       gi.data(), static_cast<u32>(gi.size()));

    pino::Texture check_tex;
    check_tex.make_checkerboard(64, 4);

    struct SceneObj {
        pino::Transform xf;
        const pino::Texture* tex = nullptr;
        glm::vec4 color = {1,1,1,1};
    };
    std::vector<SceneObj> objects;

    {
        SceneObj obj;
        obj.xf.position = {0, -1, 0};
        obj.xf.rotation = glm::angleAxis(glm::radians(-90.0f), glm::vec3(1,0,0));
        obj.tex = &check_tex;
        objects.push_back(obj);
    }

    // Audio source position (cube)
    glm::vec3 source_pos = {3, 0, 0};
    pino::Transform source_xf;
    source_xf.position = source_pos;
    source_xf.scale    = {0.8f, 0.8f, 0.8f};

    // Manual FPS camera (bypasses FpsController to fix Y inversion)
    pino::Camera cam;
    cam.perspective(60.0f, eng.window().aspect(), 0.1f, 100.0f);

    glm::vec3 cam_pos = {0, 1, 5};
    f32 yaw   = -90.0f;
    f32 pitch = 0.0f;
    const f32 sensitivity = 0.12f;
    const f32 move_speed  = 5.0f;

    SDL_SetRelativeMouseMode(SDL_TRUE);

    // Play looping ambient on startup
    u64 bgm = eng.audio().play("audio/test_tone.wav", true, 1.0f);

    PINO_INFO("Audio test — WASD move, mouse look, SPACE = play SFX, ESC = exit");
    PINO_INFO("Cube at (3,0,0) is the sound source — volume changes with distance");

    while (eng.is_running()) {
        eng.begin_frame();
        f32 dt = eng.delta_time();

        // ---- Manual mouse look (negate dy for natural FPS feel) ----
        yaw   += static_cast<f32>(eng.input().mouse_dx()) * sensitivity;
        pitch -= static_cast<f32>(eng.input().mouse_dy()) * sensitivity;
        pitch = glm::clamp(pitch, -89.0f, 89.0f);

        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front   = glm::normalize(front);

        glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0, 1, 0)));
        glm::vec3 up    = glm::normalize(glm::cross(right, front));

        // ---- WASD movement ----
        f32 spd = move_speed * dt;
        if (eng.input().key_down(pino::Key::W)) cam_pos += front * spd;
        if (eng.input().key_down(pino::Key::S)) cam_pos -= front * spd;
        if (eng.input().key_down(pino::Key::A)) cam_pos -= right * spd;
        if (eng.input().key_down(pino::Key::D)) cam_pos += right * spd;

        cam.look_at(cam_pos, cam_pos + front, up);

        // ---- Tab = toggle mouse grab ----
        if (eng.input().key_pressed(pino::Key::Tab)) {
            bool grabbed = SDL_GetRelativeMouseMode();
            SDL_SetRelativeMouseMode(grabbed ? SDL_FALSE : SDL_TRUE);
        }

        if (eng.input().key_pressed(pino::Key::Escape)) break;

        // ---- Distance-based audio attenuation ----
        f32 dist  = glm::distance(cam_pos, source_pos);
        f32 vol   = 1.0f / (1.0f + 1.5f * dist);
        vol       = glm::clamp(vol, 0.0f, 1.0f);

        // Update ambient volume based on distance to source
        eng.audio().set_volume(bgm, vol);

        // Space = one-shot SFX at distance-based volume
        if (eng.input().key_pressed(pino::Key::Space)) {
            eng.audio().play_one_shot("audio/test_tone.wav", vol);
        }

        // 1-5 keys = master volume
        if (eng.input().key_pressed(pino::Key::_1)) eng.audio().set_master_volume(0.0f);
        if (eng.input().key_pressed(pino::Key::_2)) eng.audio().set_master_volume(0.25f);
        if (eng.input().key_pressed(pino::Key::_3)) eng.audio().set_master_volume(0.5f);
        if (eng.input().key_pressed(pino::Key::_4)) eng.audio().set_master_volume(0.75f);
        if (eng.input().key_pressed(pino::Key::_5)) eng.audio().set_master_volume(1.0f);

        // ---- Render ----
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        shader.bind();
        shader.set_int("uTex", 0);

        shader.set_vec4("uColor", glm::vec4(1));
        shader.set_mat4("uMVP", cam.view_proj() * objects[0].xf.matrix());
        objects[0].tex->bind(0);
        ground_mesh.draw();

        f32 pulse = 0.5f + 0.5f * static_cast<f32>(std::sin(eng.elapsed_time() * 2.0));
        shader.set_vec4("uColor", glm::vec4(1, pulse, 0, 1));
        shader.set_mat4("uMVP", cam.view_proj() * source_xf.matrix());
        color_tex.bind(0);
        cube_mesh.draw();

        eng.end_frame();
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }

    eng.audio().stop(bgm);
    SDL_SetRelativeMouseMode(SDL_FALSE);
    return 0;
}
