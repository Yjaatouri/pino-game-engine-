#pragma once

#include "engine/core/types.h"

namespace pino {

struct EngineStats {
    static constexpr u32 WINDOW = 60;

    f32 fps            = 60.0f;
    f32 frame_time_ms  = 16.667f;
    f32 update_time_ms = 0.0f;
    f32 render_time_ms = 0.0f;

    void tick(f32 dt, f32 update_duration, f32 render_duration) {
        m_frame_samples[m_idx]  = dt;
        m_update_samples[m_idx] = update_duration;
        m_render_samples[m_idx] = render_duration;
        m_idx = (m_idx + 1) % WINDOW;

        f32 frame_sum  = 0;
        f32 update_sum = 0;
        f32 render_sum = 0;
        for (u32 i = 0; i < WINDOW; ++i) {
            frame_sum  += m_frame_samples[i];
            update_sum += m_update_samples[i];
            render_sum += m_render_samples[i];
        }

        f32 avg_dt = frame_sum / static_cast<f32>(WINDOW);
        fps            = avg_dt > 0.0f ? 1.0f / avg_dt : 0.0f;
        frame_time_ms  = avg_dt * 1000.0f;
        update_time_ms = (update_sum / static_cast<f32>(WINDOW)) * 1000.0f;
        render_time_ms = (render_sum / static_cast<f32>(WINDOW)) * 1000.0f;
    }

private:
    f32  m_frame_samples[WINDOW]  = {};
    f32  m_update_samples[WINDOW] = {};
    f32  m_render_samples[WINDOW] = {};
    u32  m_idx = 0;
};

} // namespace pino
