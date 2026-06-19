// clear — font foundation test: unit validation + visual text rendering
#include "engine/engine.h"
#include "engine/renderer/gl_es3.h"
#include "engine/renderer/font.h"
#include "engine/renderer/text_renderer.h"
#include <cstdio>
#include <chrono>
#include <thread>
using namespace pino;

// ─── Font validation (unit test) ───────────────────────────────
static int run_font_tests(Font& font) {
    int errors = 0;
    auto check = [&](bool cond, const char* msg) {
        if (!cond) { PINO_ERROR("  FAIL: %s", msg); ++errors; }
    };

    check(font.is_valid(), "is_valid() after load_builtin()");
    check(font.atlas().is_valid(), "atlas texture is_valid()");
    check(font.atlas().handle() != 0, "atlas GL handle non-zero");
    check(font.atlas().width() > 0, "atlas width > 0");
    check(font.atlas().height() > 0, "atlas height > 0");

    check(font.font_size() == 13.0f, "font_size == 13");
    check(font.line_height() == 15.0f, "line_height == 15");

    check(font.glyph('A').width == 8.0f, "glyph(A).width == 8");
    check(font.glyph('A').height == 13.0f, "glyph(A).height == 13");
    check(font.glyph('A').advance == 8.0f, "glyph(A).advance == 8");
    check(font.glyph('A').u1 > font.glyph('A').u0, "glyph(A).u1 > u0");
    check(font.glyph('A').v0 < font.glyph('A').v1, "glyph(A).v0 < v1 (no flip)");

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

    for (int c = 32; c <= 126; ++c) {
        const auto& g = font.glyph(static_cast<char>(c));
        check(g.width > 0, "glyph(width>0) for all printable");
        check(g.height > 0, "glyph(height>0) for all printable");
        check(g.u1 > g.u0, "glyph(u1>u0) for all printable");
        check(g.advance > 0, "glyph(advance>0) for all printable");
    }

    const auto& null_g = font.glyph(static_cast<char>(200));
    check(null_g.width == 0 && null_g.height == 0 &&
          null_g.u0 == 0 && null_g.v0 == 0, "glyph(out-of-range) == null_glyph");

    PINO_INFO("  %d errors, 0 warnings", errors);
    return errors;
}

int main() {
    pino::EngineConfig cfg;
    cfg.app_title     = "Pino Font Test";
    cfg.window_width  = 800;
    cfg.window_height = 600;
    cfg.resizable     = true;

    pino::Engine eng;
    if (!eng.init(cfg)) return 1;

    Font font;
    bool loaded = font.load_builtin();
    if (!loaded) { PINO_ERROR("Font::load_builtin() failed"); return 1; }

    PINO_INFO("═══════════ Font Unit Validation ═══════════");
    PINO_INFO("  load_builtin(): %s", loaded ? "true" : "false");
    int errs = run_font_tests(font);
    PINO_INFO("═══════════════════════════════════════════");

    TextRenderer tr;
    if (!tr.init(cfg.window_width, cfg.window_height)) return 1;
    i32 fw = cfg.window_width, fh = cfg.window_height;

    auto t0 = std::chrono::steady_clock::now();
    int frames = 0;
    float fps = 60.0f;
    float fps_timer = 0;

    while (eng.is_running()) {
        eng.begin_frame();
        glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        tr.begin_frame();

        f32 y = 30;
        tr.draw_text(font, "Hello, Pino Engine!", 30, y, 1.0f, 1,1,1,1); y += 22;
        tr.draw_text(font, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", 30, y, 1.0f, 0,1,1,1); y += 22;
        tr.draw_text(font, "abcdefghijklmnopqrstuvwxyz", 30, y, 1.0f, 0,1,0,1); y += 22;
        tr.draw_text(font, "0123456789", 30, y, 1.0f, 1,1,0,1); y += 22;
        tr.draw_text(font, "!@#$%^&*()_+-=[]{}|;:',.<>?/~`", 30, y, 1.0f, 1,0,1,1); y += 22;
        y += 10;
        tr.draw_text(font, "Scale 0.5x", 30, y, 0.5f, 0.7f,0.7f,1,1); y += 12;
        tr.draw_text(font, "Scale 2.0x", 30, y, 2.0f, 1,0.5f,0.5f,1); y += 32;
        tr.draw_text(font, "ASCII 32-126:", 30, y, 1.0f, 0.7f,0.7f,0.7f,1); y += 20;

        char full[95];
        for (int i = 0; i < 94; ++i) full[i] = static_cast<char>(i + 33);
        full[93] = 0;
        tr.draw_text(font, full, 30, y, 0.7f, 0.6f,0.6f,0.6f,1); y += 16;

        ++frames;
        auto t1 = std::chrono::steady_clock::now();
        f32 dt = std::chrono::duration<f32>(t1 - t0).count();
        fps_timer += dt;
        if (fps_timer >= 0.5f) { fps = frames / fps_timer; frames = 0; fps_timer = 0; }
        t0 = t1;

        char fps_buf[64];
        std::snprintf(fps_buf, sizeof(fps_buf), "FPS: %.1f  Errors: %d", fps, errs);
        tr.draw_text(font, fps_buf, static_cast<f32>(fw - 200), static_cast<f32>(fh - 24),
                     0.8f, 0.4f,0.8f,0.4f,1);

        tr.draw_text(font, errs == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED",
                     30.0f, static_cast<f32>(fh - 24), 0.9f,
                     errs == 0 ? 0.2f : 1.0f,
                     errs == 0 ? 0.9f : 0.2f,
                     errs == 0 ? 0.2f : 0.2f, 1);

        tr.render(fw, fh);

        eng.end_frame();

        if (eng.input().key_pressed(pino::Key::Escape)) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    tr.destroy();
    font.destroy();
    PINO_INFO("Exiting cleanly");
    return 0;
}
