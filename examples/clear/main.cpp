// clear — minimal test of the platform abstraction layer.
// Creates a window + OpenGL ES 3.0 context, clears to dark blue, waits for ESC or close.

#include "engine/engine.h"
#include "engine/renderer/gl_es3.h"
#include <chrono>
#include <thread>

int main() {
    pino::EngineConfig cfg;
    cfg.app_title    = "Pino Clear";
    cfg.window_width = 800;
    cfg.window_height= 600;
    cfg.resizable    = true;

    pino::Engine eng;
    if (!eng.init(cfg)) return 1;

    PINO_INFO("Clear example running — press ESC or close window to exit");

    while (eng.is_running()) {
        eng.begin_frame();

        glClearColor(0.05f, 0.05f, 0.20f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        eng.end_frame();

        if (eng.input().key_pressed(pino::Key::Escape)) break;

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    PINO_INFO("Exiting cleanly");
    return 0;
}
