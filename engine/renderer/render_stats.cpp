#include "render_stats.h"
#include "engine/core/log.h"

namespace pino {

RenderStats& RenderStats::instance() {
    static RenderStats s;
    return s;
}

void RenderStats::begin_frame() {
    draw_calls      = 0;
    triangles       = 0;
    shader_switches = 0;
    state_changes   = 0;
    uniform_calls   = 0;
}

void RenderStats::end_frame() {
    PINO_INFO("--- Frame stats: %u draws, %u triangles, %u shader switches, %u state changes, %u uniform calls ---",
              draw_calls, triangles, shader_switches, state_changes, uniform_calls);
}

} // namespace pino
