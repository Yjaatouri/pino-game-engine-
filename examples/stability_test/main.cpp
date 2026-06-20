#include "engine/engine.h"
#include "engine/core/log.h"
#include "engine/renderer/shader.h"
#include "engine/renderer/mesh.h"
#include "engine/renderer/camera.h"
#include "engine/renderer/light.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <cstdio>
#include <cmath>
#include <chrono>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
static SIZE_T current_mem_kb() {
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return pmc.WorkingSetSize / 1024;
    return 0;
}
#else
static size_t current_mem_kb() { return 0; }
#endif

static double now_sec() {
    return std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

class IdleGame final : public pino::IGame {
public:
    IdleGame(pino::Engine& engine, float duration) : m_eng(engine), m_duration(duration) {}

    bool init() override {
        m_elapsed = 0.0f;
        m_shader = m_eng.assets().get_shader("shaders/lit.vert",
                                             "shaders/lit.frag");
        if (!m_shader) return false;
        m_cube = m_eng.assets().get_mesh("models/cube.obj");
        if (!m_cube) return false;
        m_cam.perspective(45.0f, 640.0f / 480.0f, 0.1f, 50.0f);
        glEnable(GL_DEPTH_TEST);
        glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
        return true;
    }

    void update(pino::f32 dt) override {
        m_elapsed += dt;
        if (m_elapsed >= m_duration)
            m_eng.request_quit();
    }

    void render(pino::f32) override {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        m_shader->bind();
        m_shader->set_mat4("u_view_proj", m_cam.view_proj());
        m_shader->set_vec3("u_camera_pos", m_cam.position());
        pino::AmbientLight amb{{1,1,1}, 0.5f};
        pino::DirectionalLight dir{{0.3f, -0.8f, -0.4f}, {0.7f, 0.7f, 0.8f}};
        pino::upload_ambient_light(*m_shader, amb);
        pino::upload_directional_light(*m_shader, dir);
        m_shader->set_int("u_num_point_lights", 0);
        m_angle += 0.02f;
        glm::mat4 model = glm::rotate(glm::mat4(1.0f), m_angle, glm::vec3{0,1,0});
        m_shader->set_mat4("u_model", model);
        m_shader->set_mat3("u_normal_matrix", glm::inverseTranspose(glm::mat3(model)));
        pino::Material mat{{0.1f,0.1f,0.1f},{0.5f,0.5f,0.5f},{0.5f,0.5f,0.5f},{0,0,0},16};
        pino::upload_material(*m_shader, mat);
        m_shader->set_int("u_has_diffuse_tex", 0);
        m_cube->draw();
    }

    void shutdown() override {}

private:
    pino::Engine&                m_eng;
    pino::AssetHandle<pino::Shader> m_shader;
    pino::AssetHandle<pino::Mesh>   m_cube;
    pino::Camera      m_cam;
    float m_elapsed = 0.0f;
    float m_duration;
    float m_angle = 0.0f;
};

int main(int argc, char** argv) {
    int iterations = 100;
    float idle_sec = 10.0f;
    if (argc > 1) iterations = atoi(argv[1]);
    if (argc > 2) idle_sec  = static_cast<float>(atof(argv[2]));

    pino::EngineConfig cfg;
    cfg.app_title     = "Stability Test";
    cfg.window_width  = 640;
    cfg.window_height = 480;
    cfg.log_level     = pino::LogLevel::Warn;

    // Turn off vsync to run as fast as possible
    cfg.vsync = false;

    SIZE_T base_mem = current_mem_kb();
    SIZE_T peak_mem = 0;
    SIZE_T final_mem = 0;
    int failures = 0;
    double total_start = now_sec();

    PINO_INFO("=== Engine Lifecycle Stability Test ===");
    PINO_INFO("Iterations: %d  Idle duration: %.1f sec/iteration", iterations, idle_sec);
    PINO_INFO("Base memory: %llu KB", (unsigned long long)base_mem);

    FILE* report = std::fopen("stability_report.txt", "w");
    if (report) {
        std::fprintf(report, "=== Engine Lifecycle Stability Test ===\n");
        std::fprintf(report, "Iterations: %d  Idle duration: %.1f sec/iteration\n", iterations, idle_sec);
        std::fprintf(report, "Base memory: %llu KB\n\n", (unsigned long long)base_mem);
    }

    for (int i = 0; i < iterations; ++i) {
        double iter_start = now_sec();

        pino::Engine engine;
        if (!engine.init(cfg)) {
            PINO_ERROR("ITER %d: engine.init() FAILED", i);
            ++failures; continue;
        }

        IdleGame game(engine, idle_sec);
        engine.run(game);

        SIZE_T mem = current_mem_kb();
        if (mem > peak_mem) peak_mem = mem;

        if ((i + 1) % 10 == 0 || i == 0) {
            double elapsed = now_sec() - total_start;
            double ms_per = (now_sec() - iter_start) * 1000.0;
            char buf[256];
            std::snprintf(buf, sizeof(buf), "ITER %4d/%-4d mem=%llu KB (peak=%llu)  iter=%.0fms  total=%.1fs",
                          i + 1, iterations, (unsigned long long)mem, (unsigned long long)peak_mem, ms_per, elapsed);
            PINO_INFO("%s", buf);
            if (report) std::fprintf(report, "%s\n", buf);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        std::fflush(stderr);
    }

    final_mem = current_mem_kb();
    double total_elapsed = now_sec() - total_start;

    if (report) {
        std::fprintf(report, "\n=== Stability Test Complete ===\n");
        std::fprintf(report, "Duration:    %.1f seconds\n", total_elapsed);
        std::fprintf(report, "Iterations:  %d\n", iterations);
        std::fprintf(report, "Failures:    %d\n", failures);
        std::fprintf(report, "\nMemory (KB):\n");
        std::fprintf(report, "  Base:  %llu\n", (unsigned long long)base_mem);
        std::fprintf(report, "  Peak:  %llu\n", (unsigned long long)peak_mem);
        std::fprintf(report, "  Final: %llu\n", (unsigned long long)final_mem);
        std::fprintf(report, "  Growth: %lld  (%+.1f%%)\n",
                     (long long)(final_mem - base_mem),
                     base_mem > 0 ? 100.0 * (double)(final_mem - base_mem) / (double)base_mem : 0.0);
        std::fprintf(report, "\n%s\n", failures == 0 ? "RESULT: PASS (no failures)" : "RESULT: FAIL");
        std::fclose(report);
    }

    PINO_INFO("=== Stability Test Complete ===");
    PINO_INFO("Duration: %.1fs  Iterations: %d  Failures: %d", total_elapsed, iterations, failures);
    PINO_INFO("Memory: base=%llu peak=%llu final=%llu growth=%lld KB",
              (unsigned long long)base_mem, (unsigned long long)peak_mem,
              (unsigned long long)final_mem, (long long)(final_mem - base_mem));
    if (failures == 0)
        PINO_INFO("RESULT: PASS");
    else
        PINO_ERROR("RESULT: FAIL (%d failures)", failures);
    std::fflush(stderr);

    return failures;
}
