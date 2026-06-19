// clear — font foundation test: unit validation + visual text rendering
#include "engine/engine.h"
#include "engine/renderer/gl_es3.h"
#include "engine/renderer/font.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cstdio>
#include <chrono>
#include <thread>
using namespace pino;

// ─── Embedded text shaders (GLES 3.0) ──────────────────────────
static const char* kTextVS = R"(
#version 300 es
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;
uniform mat4 u_mvp;
out vec2 v_uv;
void main() {
    gl_Position = u_mvp * vec4(a_pos, 0.0, 1.0);
    v_uv = a_uv;
}
)";

static const char* kTextFS = R"(
#version 300 es
precision mediump float;
in vec2 v_uv;
uniform sampler2D u_tex;
uniform vec4 u_color;
out vec4 frag_color;
void main() {
    float alpha = texture(u_tex, v_uv).r;
    frag_color = vec4(u_color.rgb, u_color.a * alpha);
}
)";

static GLuint compile_shader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[512];
        glGetShaderInfoLog(s, sizeof(buf), nullptr, buf);
        PINO_ERROR("Shader compile error (%s): %s",
                   type == GL_VERTEX_SHADER ? "VS" : "FS", buf);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static GLuint link_program(GLuint vs, GLuint fs) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[512];
        glGetProgramInfoLog(p, sizeof(buf), nullptr, buf);
        PINO_ERROR("Program link error: %s", buf);
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

// ─── Batched text renderer ─────────────────────────────────────
struct TextBatcher {
    GLuint vao = 0, vbo = 0, ibo = 0, program = 0;
    GLint u_mvp = -1, u_tex = -1, u_color = -1;
    glm::mat4 ortho{1.0f};

    static constexpr int MAX_QUADS = 512;

    struct Vertex { float x, y, u, v; };
    Vertex verts[MAX_QUADS * 4];
    int quad_count = 0;

    bool init(int w, int h) {
        GLuint vs = compile_shader(GL_VERTEX_SHADER, kTextVS);
        GLuint fs = compile_shader(GL_FRAGMENT_SHADER, kTextFS);
        if (!vs || !fs) { glDeleteShader(vs); glDeleteShader(fs); return false; }
        program = link_program(vs, fs);
        glDeleteShader(vs); glDeleteShader(fs);
        if (!program) return false;

        u_mvp   = glGetUniformLocation(program, "u_mvp");
        u_tex   = glGetUniformLocation(program, "u_tex");
        u_color = glGetUniformLocation(program, "u_color");

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);

        // Pre-fill index buffer data (referenced by VAO later)
        GLushort idx[MAX_QUADS * 6];
        for (int i = 0; i < MAX_QUADS; ++i) {
            idx[i * 6 + 0] = static_cast<GLushort>(i * 4 + 0);
            idx[i * 6 + 1] = static_cast<GLushort>(i * 4 + 1);
            idx[i * 6 + 2] = static_cast<GLushort>(i * 4 + 2);
            idx[i * 6 + 3] = static_cast<GLushort>(i * 4 + 0);
            idx[i * 6 + 4] = static_cast<GLushort>(i * 4 + 2);
            idx[i * 6 + 5] = static_cast<GLushort>(i * 4 + 3);
        }

        // Vertex array and buffer setup (IBO must be bound while VAO is active)
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ibo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), nullptr, GL_STREAM_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)8);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);
        glBindVertexArray(0);

        ortho = glm::ortho(0.0f, static_cast<f32>(w),
                           static_cast<f32>(h), 0.0f, -1.0f, 1.0f);
        return true;
    }

    void begin_frame() {
        quad_count = 0;
    }

    void draw_string(Font& font, const char* text, float x, float y,
                     float scale) {
        float cx = x, cy = y;
        for (const char* p = text; *p; ++p) {
            if (quad_count >= MAX_QUADS) break;
            if (*p == '\n') { cx = x; cy += font.line_height() * scale; continue; }
            const auto& glyph = font.glyph(*p);
            if (glyph.width == 0 && glyph.height == 0) { cx += 6.0f * scale; continue; }

            float x0 = cx + glyph.bearing_x * scale;
            float y0 = cy;
            float x1 = x0 + glyph.width * scale;
            float y1 = y0 + glyph.height * scale;

            Vertex* v = &verts[quad_count * 4];
            v[0] = {x0, y0, glyph.u0, glyph.v0};
            v[1] = {x1, y0, glyph.u1, glyph.v0};
            v[2] = {x1, y1, glyph.u1, glyph.v1};
            v[3] = {x0, y1, glyph.u0, glyph.v1};
            ++quad_count;

            cx += glyph.advance * scale;
        }
    }

    void flush(float r, float g, float b, float a) {
        if (quad_count == 0) return;
        glUseProgram(program);
        glUniformMatrix4fv(u_mvp, 1, GL_FALSE, glm::value_ptr(ortho));
        glUniform1i(u_tex, 0);
        glUniform4f(u_color, r, g, b, a);

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(quad_count * 4 * sizeof(Vertex)),
                     verts, GL_STREAM_DRAW);

        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, quad_count * 6, GL_UNSIGNED_SHORT, nullptr);
        glBindVertexArray(0);
        glUseProgram(0);
    }

    void destroy() {
        if (vao) glDeleteVertexArrays(1, &vao);
        if (vbo) glDeleteBuffers(1, &vbo);
        if (ibo) glDeleteBuffers(1, &ibo);
        if (program) glDeleteProgram(program);
        vao = vbo = ibo = program = 0;
    }
};

// ─── Font validation (unit test) ───────────────────────────────
static int run_font_tests(Font& font) {
    int errors = 0;
    auto check = [&](bool cond, const char* msg) {
        if (!cond) { PINO_ERROR("  FAIL: %s", msg); ++errors; }
    };

    // ── Atlas loaded ────────────────────────────────────────────
    check(font.is_valid(), "is_valid() after load_builtin()");
    check(font.atlas().is_valid(), "atlas texture is_valid()");
    check(font.atlas().handle() != 0, "atlas GL handle non-zero");
    check(font.atlas().width() > 0, "atlas width > 0");
    check(font.atlas().height() > 0, "atlas height > 0");

    // ── Metadata ────────────────────────────────────────────────
    check(font.font_size() == 13.0f, "font_size == 13");
    check(font.line_height() == 15.0f, "line_height == 15");

    // ── Specific glyph queries ──────────────────────────────────
    check(font.glyph('A').width == 8.0f, "glyph(A).width == 8");
    check(font.glyph('A').height == 13.0f, "glyph(A).height == 13");
    check(font.glyph('A').advance == 8.0f, "glyph(A).advance == 8");
    check(font.glyph('A').u1 > font.glyph('A').u0, "glyph(A).u1 > u0");
    check(font.glyph('A').v0 > font.glyph('A').v1, "glyph(A).v0 > v1 (V flip)");

    check(font.glyph('B').width == 8.0f, "glyph(B).width == 8");
    check(font.glyph('B').u1 > font.glyph('B').u0, "glyph(B).u1 > u0");

    check(font.glyph('0').width == 8.0f, "glyph(0).width == 8");
    check(font.glyph('0').height == 13.0f, "glyph(0).height == 13");
    check(font.glyph('0').advance == 8.0f, "glyph(0).advance == 8");

    check(font.glyph('?').width == 8.0f, "glyph(?).width == 8");
    check(font.glyph('?').height == 13.0f, "glyph(?).height == 13");
    check(font.glyph('?').advance == 8.0f, "glyph(?).advance == 8");

    check(font.glyph(' ').width == 8.0f, "glyph(space).width == 8");
    check(font.glyph('z').u1 > font.glyph('z').u0, "glyph(z).u1 > u0");
    check(font.glyph('9').advance == 8.0f, "glyph(9).advance == 8");

    // ── All printable ASCII (32–126) have valid metrics ─────────
    for (int c = 32; c <= 126; ++c) {
        const auto& g = font.glyph(static_cast<char>(c));
        check(g.width > 0, "glyph(width>0) for all printable");
        check(g.height > 0, "glyph(height>0) for all printable");
        check(g.u1 > g.u0, "glyph(u1>u0) for all printable");
        check(g.advance > 0, "glyph(advance>0) for all printable");
    }

    // ── Out-of-range returns null glyph ─────────────────────────
    const auto& null_g = font.glyph(static_cast<char>(200));
    check(null_g.width == 0 && null_g.height == 0 &&
          null_g.u0 == 0 && null_g.v0 == 0, "glyph(out-of-range) == null_glyph");

    PINO_INFO("  %d errors, 0 warnings", errors);
    return errors;
}

// ─── main ──────────────────────────────────────────────────────
int main() {
    pino::EngineConfig cfg;
    cfg.app_title     = "Pino Font Test";
    cfg.window_width  = 800;
    cfg.window_height = 600;
    cfg.resizable     = true;

    pino::Engine eng;
    if (!eng.init(cfg)) return 1;

    // ── Load font ──────────────────────────────────────────────
    Font font;
    bool loaded = font.load_builtin();
    if (!loaded) { PINO_ERROR("Font::load_builtin() failed"); return 1; }

    // ── Unit validation ────────────────────────────────────────
    PINO_INFO("═══════════ Font Unit Validation ═══════════");
    PINO_INFO("  load_builtin(): %s", loaded ? "true" : "false");
    int errs = run_font_tests(font);
    PINO_INFO("═══════════════════════════════════════════");

    // ── Init text renderer ─────────────────────────────────────
    TextBatcher tb;
    if (!tb.init(cfg.window_width, cfg.window_height)) return 1;
    int fw = cfg.window_width, fh = cfg.window_height;

    // ── Game loop ──────────────────────────────────────────────
    auto t0 = std::chrono::steady_clock::now();
    int frames = 0;
    float fps = 60.0f;
    float fps_timer = 0;

    while (eng.is_running()) {
        eng.begin_frame();
        glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Bind font atlas
        font.atlas().bind(0);

        // Enable alpha blending, disable depth test for screen-space text
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);

        // Draw each line with its own color (separate begin/flush per color batch)

        float y = 30;
        tb.begin_frame();
        tb.draw_string(font, "Hello, Pino Engine!", 30, y, 1.0f);
        tb.flush(1,1,1,1); y += 22;

        tb.begin_frame();
        tb.draw_string(font, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", 30, y, 1.0f);
        tb.flush(0,1,1,1); y += 22;

        tb.begin_frame();
        tb.draw_string(font, "abcdefghijklmnopqrstuvwxyz", 30, y, 1.0f);
        tb.flush(0,1,0,1); y += 22;

        tb.begin_frame();
        tb.draw_string(font, "0123456789", 30, y, 1.0f);
        tb.flush(1,1,0,1); y += 22;

        tb.begin_frame();
        tb.draw_string(font, "!@#$%^&*()_+-=[]{}|;:',.<>?/~`", 30, y, 1.0f);
        tb.flush(1,0,1,1); y += 22;
        y += 10;

        tb.begin_frame();
        tb.draw_string(font, "Scale 0.5x", 30, y, 0.5f);
        tb.flush(0.7f,0.7f,1,1); y += 12;

        tb.begin_frame();
        tb.draw_string(font, "Scale 2.0x", 30, y, 2.0f);
        tb.flush(1,0.5f,0.5f,1); y += 32;

        tb.begin_frame();
        tb.draw_string(font, "ASCII 32-126:", 30, y, 1.0f);
        tb.flush(0.7f,0.7f,0.7f,1); y += 20;

        // Full printable ASCII range
        char full[95];
        for (int i = 0; i < 94; ++i) full[i] = static_cast<char>(i + 33);
        full[93] = 0;
        tb.begin_frame();
        tb.draw_string(font, full, 30, y, 0.7f);
        tb.flush(0.6f,0.6f,0.6f,1); y += 16;

        // FPS counter
        ++frames;
        auto t1 = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(t1 - t0).count();
        fps_timer += dt;
        if (fps_timer >= 0.5f) { fps = frames / fps_timer; frames = 0; fps_timer = 0; }
        t0 = t1;

        char fps_buf[64];
        std::snprintf(fps_buf, sizeof(fps_buf), "FPS: %.1f  Errors: %d", fps, errs);
        tb.begin_frame();
        tb.draw_string(font, fps_buf, static_cast<float>(fw - 200), static_cast<float>(fh - 24), 0.8f);
        tb.flush(0.4f,0.8f,0.4f,1);

        // Validation results banner
        tb.begin_frame();
        tb.draw_string(font, errs == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED",
                       30.0f, static_cast<float>(fh - 24), 0.9f);
        tb.flush(errs == 0 ? 0.2f : 1.0f,
                 errs == 0 ? 0.9f : 0.2f,
                 errs == 0 ? 0.2f : 0.2f, 1);

        glDisable(GL_BLEND);

        eng.end_frame();

        if (eng.input().key_pressed(pino::Key::Escape)) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    tb.destroy();
    font.destroy();
    PINO_INFO("Exiting cleanly");
    return 0;
}
