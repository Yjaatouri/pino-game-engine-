#pragma once

#include "engine/core/types.h"

namespace pino {

class RenderStats {
public:
    static RenderStats& instance();

    void begin_frame();
    void end_frame();

    void add_draw_call()       { ++draw_calls; }
    void add_triangles(u32 n)  { triangles += n; }
    void add_shader_switch()   { ++shader_switches; }
    void add_state_change()    { ++state_changes; }

    u32 draw_calls      = 0;
    u32 triangles       = 0;
    u32 shader_switches = 0;
    u32 state_changes   = 0;

private:
    RenderStats() = default;
};

} // namespace pino
