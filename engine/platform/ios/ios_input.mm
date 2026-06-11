#include "ios_input.h"
#include <cmath>
#include <cstring>
#import <UIKit/UIKit.h>

namespace pino {

Input* Input::s_instance = nullptr;

IOSInput::IOSInput() {
    set_instance(this);
}

IOSInput::~IOSInput() {
    if (instance() == this) set_instance(nullptr);
}

void IOSInput::begin_frame() {
    m_prev               = m_state;
    m_state.mouse_dx     = 0;
    m_state.mouse_dy     = 0;
    m_state.scroll_dx    = 0;
    m_state.scroll_dy    = 0;
    std::memset(m_state.keys_down, 0, sizeof(m_state.keys_down));
    std::memset(m_state.keys_up,   0, sizeof(m_state.keys_up));

    m_mouse_dx    = 0;
    m_mouse_dy    = 0;
    m_scroll_dx   = 0;
    m_scroll_dy   = 0;
    m_tap         = false;
    m_pinch_delta = 0;
    m_swipe_dx    = 0;
    m_swipe_dy    = 0;

    for (auto& t : m_touches) {
        if (t.just_released) t.active = false;
        t.just_released = false;
    }

    m_touch_count = 0;
    for (auto& t : m_touches)
        if (t.active) ++m_touch_count;
}

void IOSInput::process_event(const void*) {
    // Not used on iOS — touch events arrive via touch_began/moved/ended
}

bool IOSInput::quit_requested() const {
    return m_quit;
}

// ── State access ────────────────────────────────────────────────

void IOSInput::reset_state() {
    std::memset(&m_state, 0, sizeof(m_state));
    std::memset(&m_prev, 0, sizeof(m_prev));
    m_touch_count = 0;
    for (auto& t : m_touches) {
        t.active = false;
        t.just_released = false;
    }
    m_mouse_x = m_mouse_y = 0;
    m_mouse_dx = m_mouse_dy = 0;
    m_swipe_dx = m_swipe_dy = 0;
    m_pinch_delta = 0;
    m_tap = false;
    m_quit = false;
}

void IOSInput::apply_state(const InputState& state) {
    m_state = state;
}

void IOSInput::capture_state(InputState& out_state) const {
    out_state = m_state;
}

// ── Touch API ───────────────────────────────────────────────────

u32 IOSInput::touch_index_for_ptr(u64 ptr_id) {
    for (u32 i = 0; i < MAX_TOUCH; ++i) {
        if (m_touches[i].active && m_touches[i].ptr_id == ptr_id)
            return i;
    }
    for (u32 i = 0; i < MAX_TOUCH; ++i) {
        if (!m_touches[i].active)
            return i;
    }
    return MAX_TOUCH;
}

void IOSInput::finish_touch(u32 idx) {
    if (idx >= MAX_TOUCH) return;
    auto& t = m_touches[idx];
    f32 dist = std::sqrt((t.x - t.start_x) * (t.x - t.start_x) +
                         (t.y - t.start_y) * (t.y - t.start_y));
    if (dist < 0.05f)
        m_tap = true;
    m_swipe_dx = t.x - t.start_x;
    m_swipe_dy = t.y - t.start_y;
    t.just_released = true;
}

void IOSInput::touch_began(f32 x, f32 y, u64 ptr_id) {
    i32 sx = static_cast<i32>(x);
    i32 sy = static_cast<i32>(y);
    m_mouse_dx = sx - m_mouse_x;
    m_mouse_dy = sy - m_mouse_y;
    m_mouse_x  = sx;
    m_mouse_y  = sy;

    u32 idx = touch_index_for_ptr(ptr_id);
    if (idx >= MAX_TOUCH) return;

    auto& t = m_touches[idx];
    t.active           = true;
    t.x = t.start_x   = x;
    t.y = t.start_y   = y;
    t.ptr_id          = ptr_id;
    t.just_released   = false;
}

void IOSInput::touch_moved(f32 x, f32 y, u64 ptr_id) {
    i32 sx = static_cast<i32>(x);
    i32 sy = static_cast<i32>(y);
    m_mouse_dx = sx - m_mouse_x;
    m_mouse_dy = sy - m_mouse_y;
    m_mouse_x  = sx;
    m_mouse_y  = sy;

    u32 idx = touch_index_for_ptr(ptr_id);
    if (idx >= MAX_TOUCH) return;

    auto& t = m_touches[idx];
    if (!t.active) {
        t.active   = true;
        t.start_x  = t.x;
        t.start_y  = t.y;
        t.ptr_id   = ptr_id;
    }
    f32 prev_x = t.x, prev_y = t.y;
    t.x = x;
    t.y = y;

    m_swipe_dx = t.x - t.start_x;
    m_swipe_dy = t.y - t.start_y;

    // Pinch detection
    u32 active_count = 0;
    u32 other_idx = 0;
    for (u32 i = 0; i < MAX_TOUCH; ++i) {
        if (m_touches[i].active) {
            ++active_count;
            if (i != idx) other_idx = i;
        }
    }
    if (active_count == 2) {
        auto& o = m_touches[other_idx];
        f32 prev_dx = prev_x - o.x;
        f32 prev_dy = prev_y - o.y;
        f32 cur_dx  = t.x - o.x;
        f32 cur_dy  = t.y - o.y;
        f32 prev_dist = std::sqrt(prev_dx*prev_dx + prev_dy*prev_dy);
        f32 cur_dist  = std::sqrt(cur_dx*cur_dx + cur_dy*cur_dy);
        m_pinch_delta = cur_dist - prev_dist;
    }
}

void IOSInput::touch_ended(f32 x, f32 y, u64 ptr_id) {
    u32 idx = touch_index_for_ptr(ptr_id);
    if (idx >= MAX_TOUCH) return;

    auto& t = m_touches[idx];
    t.x = x;
    t.y = y;
    finish_touch(idx);
}

void IOSInput::touch_cancelled() {
    for (auto& t : m_touches) {
        if (t.active) t.just_released = true;
    }
}

// ── Key state ───────────────────────────────────────────────────
bool IOSInput::is_key_pressed(Key) const { return false; }
bool IOSInput::is_key_just_pressed(Key) const { return false; }
bool IOSInput::is_key_just_released(Key) const { return false; }

// ── Mouse state (simulated from touch) ──────────────────────────
bool IOSInput::is_mouse_pressed(MouseButton) const {
    return m_touch_count > 0;
}
bool IOSInput::is_mouse_just_pressed(MouseButton) const {
    return false;
}

} // namespace pino
