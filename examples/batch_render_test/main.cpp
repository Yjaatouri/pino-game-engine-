#include "engine/engine.h"
#include "engine/renderer/shader.h"
#include "engine/renderer/mesh.h"
#include "engine/renderer/material.h"
#include "engine/renderer/render_queue.h"
#include "engine/renderer/render_stats.h"
#include "engine/renderer/camera.h"
#include "engine/core/transform.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/constants.hpp>
#include <cstdio>
#include <vector>
#include <cmath>
#include <memory>

using pino::u32;
using pino::f32;

static const char* vs_src = R"(#version 300 es
precision highp float;
layout(location=0)in vec3 a_pos;
layout(location=1)in vec3 a_normal;
layout(location=2)in vec2 a_uv;
layout(location=3)in vec4 a_inst_m0;
layout(location=4)in vec4 a_inst_m1;
layout(location=5)in vec4 a_inst_m2;
layout(location=6)in vec4 a_inst_m3;
uniform mat4 u_view_proj;
uniform mat4 u_model;
uniform mat3 u_normal_matrix;
uniform int u_instanced;
out vec3 v_world_pos;
out vec3 v_normal;
void main() {
    mat4 model = u_instanced != 0
        ? mat4(a_inst_m0, a_inst_m1, a_inst_m2, a_inst_m3)
        : u_model;
    vec4 wp = model * vec4(a_pos, 1.0);
    gl_Position = u_view_proj * wp;
    v_world_pos = wp.xyz;
    v_normal = normalize(mat3(transpose(inverse(model))) * a_normal);
}
)";

static const char* fs_src = R"(#version 300 es
precision highp float;
in vec3 v_world_pos;
in vec3 v_normal;
out vec4 frag_color;
uniform vec3 u_mat_diffuse;
uniform vec3 u_camera_pos;
void main() {
    vec3 n = normalize(v_normal);
    vec3 l = normalize(vec3(1, 1, 0));
    float d = max(dot(n, l), 0.0);
    vec3 c = u_mat_diffuse * (0.2 + 0.8 * d);
    frag_color = vec4(c, 1.0);
}
)";

static pino::Mesh make_cube_mesh() {
    struct V { glm::vec3 pos; glm::vec3 nrm; glm::vec2 uv; };
    std::vector<V> verts;
    std::vector<u32> idx;

    struct Face { glm::vec3 n; glm::vec3 p[4]; };
    Face faces[6] = {
        {{ 1, 0, 0}, {{ 1,-1, 1}, { 1,-1,-1}, { 1, 1,-1}, { 1, 1, 1}}},
        {{-1, 0, 0}, {{-1,-1,-1}, {-1,-1, 1}, {-1, 1, 1}, {-1, 1,-1}}},
        {{ 0, 1, 0}, {{-1, 1, 1}, {-1, 1,-1}, { 1, 1,-1}, { 1, 1, 1}}},
        {{ 0,-1, 0}, {{-1,-1,-1}, {-1,-1, 1}, { 1,-1, 1}, { 1,-1,-1}}},
        {{ 0, 0, 1}, {{-1,-1, 1}, { 1,-1, 1}, { 1, 1, 1}, {-1, 1, 1}}},
        {{ 0, 0,-1}, {{-1,-1,-1}, {-1, 1,-1}, { 1, 1,-1}, { 1,-1,-1}}},
    };
    glm::vec2 uvs[4] = {{0,0},{1,0},{1,1},{0,1}};

    for (auto& f : faces) {
        u32 base = static_cast<u32>(verts.size());
        for (int i = 0; i < 4; ++i)
            verts.push_back({f.p[i], f.n, uvs[i]});
        idx.push_back(base+0); idx.push_back(base+1); idx.push_back(base+2);
        idx.push_back(base+0); idx.push_back(base+2); idx.push_back(base+3);
    }

    pino::Mesh m;
    m.upload(reinterpret_cast<const pino::Vertex*>(verts.data()),
             static_cast<u32>(verts.size()),
             idx.data(), static_cast<u32>(idx.size()));
    return m;
}

static constexpr u32 NUM_ENTITIES = 128;

int main() {
    pino::Engine engine;
    pino::EngineConfig cfg;
    cfg.app_title     = "Batch Render Test";
    cfg.window_width  = 800;
    cfg.window_height = 600;
    if (!engine.init(cfg)) return 1;

    // Shader from source (not via AssetManager — avoids file I/O)
    auto shader_ptr = std::make_shared<pino::Shader>();
    if (!shader_ptr->load(vs_src, fs_src)) {
        PINO_ERROR("Failed to compile batch_render_test shader");
        return 1;
    }
    pino::AssetHandle<pino::Shader> shader(shader_ptr);

    // Mesh
    pino::Mesh cube_mesh = make_cube_mesh();

    // Material
    pino::Material mat;
    mat.set_shader(shader);
    mat.set_uniform("u_mat_diffuse", glm::vec3(0.2f, 0.6f, 0.8f));

    // Camera
    pino::Camera cam;
    cam.perspective(45.0f, cfg.window_width / static_cast<f32>(cfg.window_height), 0.1f, 100.0f);

    // Build transforms for entities
    struct EntityData { glm::mat4 model; glm::mat3 normal_matrix; };
    std::vector<EntityData> entities(NUM_ENTITIES);
    float grid = std::ceil(std::sqrt(static_cast<float>(NUM_ENTITIES)));
    for (u32 i = 0; i < NUM_ENTITIES; ++i) {
        float x = (static_cast<float>(i % static_cast<u32>(grid)) - grid * 0.5f) * 2.5f;
        float z = (static_cast<float>(i / static_cast<u32>(grid)) - grid * 0.5f) * 2.5f;

        pino::Transform t;
        t.position = {x, 0.0f, z};
        t.rotation = glm::angleAxis(static_cast<f32>(i) * 0.3f,
                                    glm::normalize(glm::vec3{0.2f, 1.0f, 0.1f}));
        entities[i].model = t.matrix();
        entities[i].normal_matrix = glm::mat3(glm::transpose(glm::inverse(t.matrix())));
    }

    pino::RenderQueue queue;

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);

    auto& stats = pino::RenderStats::instance();

    // Warm-up frames (allow GPU state to settle)
    for (int warmup = 0; warmup < 5; ++warmup) {
        engine.begin_frame();

        glm::vec3 eye(0, 8, 12);
        cam.look_at(eye, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader->bind();
        shader->set_mat4("u_view_proj", cam.view_proj());
        shader->set_vec3("u_camera_pos", eye);

        queue.clear();
        for (u32 i = 0; i < NUM_ENTITIES; ++i) {
            queue.submit({&cube_mesh, &mat,
                          entities[i].model, entities[i].normal_matrix,
                          false, 0.0f, false, 0});
        }

        queue.sort();
        stats.begin_frame();
        queue.flush();
        stats.end_frame();

        engine.end_frame();
    }

    // Measure one frame
    engine.begin_frame();
    {
        glm::vec3 eye(0, 8, 12);
        cam.look_at(eye, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader->bind();
        shader->set_mat4("u_view_proj", cam.view_proj());
        shader->set_vec3("u_camera_pos", eye);

        queue.clear();
        for (u32 i = 0; i < NUM_ENTITIES; ++i) {
            queue.submit({&cube_mesh, &mat,
                          entities[i].model, entities[i].normal_matrix,
                          false, 0.0f, false, 0});
        }

        queue.sort();
        stats.begin_frame();
        queue.flush();
        stats.end_frame();
    }
    engine.end_frame();

    // Report results
    u32 dc = stats.draw_calls;
    u32 uc = stats.uniform_calls;
    // Without batching: 2 (frame uniforms) + NUM_ENTITIES * (3 per-entity: u_instanced,
    // u_model, u_normal_matrix) + 1 (material: u_mat_diffuse)
    u32 naive_uc = 2 + 1 + NUM_ENTITIES * 3;

    printf("\n=== BATCH RENDER TEST RESULTS ===\n");
    printf("Entities rendered : %u\n", NUM_ENTITIES);
    printf("Draw calls        : %u\n", dc);
    printf("Uniform calls     : %u\n", uc);
    printf("Naive (unbatched) : ~%u\n", naive_uc);
    printf("Uniform/entity    : %.2f\n", static_cast<f32>(uc) / static_cast<f32>(NUM_ENTITIES));
    printf("Batching ratio    : %.1fx\n",
           static_cast<f32>(naive_uc) / static_cast<f32>(uc > 0 ? uc : 1));

    // With batching: 2 (frame) + 2 (u_instanced=1, u_normal_matrix=identity)
    // + material apply (via material calling upload_uniform → 1 call)
    // Total ≈ 5 uniform calls regardless of entity count (all batched into one instanced draw)
    bool sub_linear = uc < static_cast<u32>(naive_uc * 0.25f);
    bool draw_reduced = dc < NUM_ENTITIES / 4;
    printf("Sub-linear check  : %s (uc=%u < 25%% of %u=%u)\n",
           sub_linear ? "PASS" : "FAIL", uc, naive_uc, naive_uc / 4);
    printf("Draws reduced     : %s (dc=%u < %u/4=%u)\n",
           draw_reduced ? "PASS" : "FAIL", dc, NUM_ENTITIES, NUM_ENTITIES / 4);

    engine.shutdown();
    return (sub_linear && draw_reduced) ? 0 : 1;
}
