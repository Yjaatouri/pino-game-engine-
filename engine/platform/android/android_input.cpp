#include "android_input.h"
#include <cmath>
#include <cstring>

#if defined(__ANDROID__)

namespace pino {

Input* Input::s_instance = nullptr;

AndroidInput::AndroidInput() {
    set_instance(this);
}

AndroidInput::~AndroidInput() {
    if (instance() == this) set_instance(nullptr);
}

void AndroidInput::begin_frame() {
    m_prev               = m_state;
    m_state.mouse_dx     = 0;
    m_state.mouse_dy     = 0;
    m_state.scroll_dx    = 0;
    m_state.scroll_dy    = 0;
    std::memset(m_state.keys_down, 0, sizeof(m_state.keys_down));
    std::memset(m_state.keys_up,   0, sizeof(m_state.keys_up));

    m_mouse_dx = 0;
    m_mouse_dy = 0;
    m_tap      = false;
    m_pinch_delta = 0;
    m_swipe_dx    = 0;
    m_swipe_dy    = 0;
    m_pause_requested = false;

    for (auto& t : m_touches) {
        if (t.just_released) t.active = false;
        t.just_released = false;
    }

    m_touch_count = 0;
    for (auto& t : m_touches)
        if (t.active) ++m_touch_count;
}

void AndroidInput::process_event(const void* ev) {
    const AInputEvent* event = static_cast<const AInputEvent*>(ev);
    int32_t type = AInputEvent_getType(event);
    switch (type) {
        case AINPUT_EVENT_TYPE_KEY:
            process_key_event(event);
            break;
        case AINPUT_EVENT_TYPE_MOTION:
            process_motion_event(event);
            break;
    }
}

void AndroidInput::process_key_event(const AInputEvent* event) {
    int32_t action = AKeyEvent_getAction(event);
    int32_t code   = AKeyEvent_getKeyCode(event);

    auto set_key = [&](Key k, bool pressed) {
        usize i = static_cast<usize>(k);
        if (i < static_cast<usize>(Key::COUNT)) {
            bool was = m_state.keys[i];
            m_state.keys[i] = pressed;
            if (pressed && !was) m_state.keys_down[i] = true;
            if (!pressed && was) m_state.keys_up[i] = true;
        }
    };

    // Back button → request pause, NOT quit
    if (code == AKEYCODE_BACK) {
        if (action == AKEY_EVENT_ACTION_DOWN) {
            m_pause_requested = true;
        }
        return;
    }

    if (code == AKEYCODE_ESCAPE) {
        if (action == AKEY_EVENT_ACTION_DOWN) {
            m_pause_requested = true;
        }
        return;
    }

    bool down = (action == AKEY_EVENT_ACTION_DOWN) || (action == AKEY_EVENT_ACTION_MULTIPLE);
    bool up   = (action == AKEY_EVENT_ACTION_UP);

    if (down && up) return; // ignore spurious events

    switch (code) {
        case AKEYCODE_W:              set_key(Key::W, down); break;
        case AKEYCODE_A:              set_key(Key::A, down); break;
        case AKEYCODE_S:              set_key(Key::S, down); break;
        case AKEYCODE_D:              set_key(Key::D, down); break;
        case AKEYCODE_Q:              set_key(Key::Q, down); break;
        case AKEYCODE_E:              set_key(Key::E, down); break;
        case AKEYCODE_R:              set_key(Key::R, down); break;
        case AKEYCODE_T:              set_key(Key::T, down); break;
        case AKEYCODE_Y:              set_key(Key::Y, down); break;
        case AKEYCODE_U:              set_key(Key::U, down); break;
        case AKEYCODE_I:              set_key(Key::I, down); break;
        case AKEYCODE_O:              set_key(Key::O, down); break;
        case AKEYCODE_P:              set_key(Key::P, down); break;
        case AKEYCODE_F:              set_key(Key::F, down); break;
        case AKEYCODE_G:              set_key(Key::G, down); break;
        case AKEYCODE_H:              set_key(Key::H, down); break;
        case AKEYCODE_J:              set_key(Key::J, down); break;
        case AKEYCODE_K:              set_key(Key::K, down); break;
        case AKEYCODE_L:              set_key(Key::L, down); break;
        case AKEYCODE_Z:              set_key(Key::Z, down); break;
        case AKEYCODE_X:              set_key(Key::X, down); break;
        case AKEYCODE_C:              set_key(Key::C, down); break;
        case AKEYCODE_V:              set_key(Key::V, down); break;
        case AKEYCODE_B:              set_key(Key::B, down); break;
        case AKEYCODE_N:              set_key(Key::N, down); break;
        case AKEYCODE_M:              set_key(Key::M, down); break;
        case AKEYCODE_0:              set_key(Key::_0, down); break;
        case AKEYCODE_1:              set_key(Key::_1, down); break;
        case AKEYCODE_2:              set_key(Key::_2, down); break;
        case AKEYCODE_3:              set_key(Key::_3, down); break;
        case AKEYCODE_4:              set_key(Key::_4, down); break;
        case AKEYCODE_5:              set_key(Key::_5, down); break;
        case AKEYCODE_6:              set_key(Key::_6, down); break;
        case AKEYCODE_7:              set_key(Key::_7, down); break;
        case AKEYCODE_8:              set_key(Key::_8, down); break;
        case AKEYCODE_9:              set_key(Key::_9, down); break;
        case AKEYCODE_SPACE:          set_key(Key::Space, down); break;
        case AKEYCODE_ENTER:          set_key(Key::Enter, down); break;
        case AKEYCODE_DEL:            set_key(Key::Backspace, down); break;
        case AKEYCODE_FORWARD_DEL:    set_key(Key::Delete, down); break;
        case AKEYCODE_TAB:            set_key(Key::Tab, down); break;
        case AKEYCODE_DPAD_UP:        set_key(Key::Up, down); m_dpad_up = down; break;
        case AKEYCODE_DPAD_DOWN:      set_key(Key::Down, down); m_dpad_down = down; break;
        case AKEYCODE_DPAD_LEFT:      set_key(Key::Left, down); m_dpad_left = down; break;
        case AKEYCODE_DPAD_RIGHT:     set_key(Key::Right, down); m_dpad_right = down; break;
        case AKEYCODE_DPAD_CENTER:    m_dpad_center = down; break;
        case AKEYCODE_SHIFT_LEFT:     set_key(Key::LShift, down); break;
        case AKEYCODE_SHIFT_RIGHT:    set_key(Key::RShift, down); break;
        case AKEYCODE_CTRL_LEFT:      set_key(Key::LCtrl, down); break;
        case AKEYCODE_CTRL_RIGHT:     set_key(Key::RCtrl, down); break;
        case AKEYCODE_ALT_LEFT:       set_key(Key::LAlt, down); break;
        case AKEYCODE_ALT_RIGHT:      set_key(Key::RAlt, down); break;
        case AKEYCODE_MINUS:          set_key(Key::Minus, down); break;
        case AKEYCODE_EQUALS:         set_key(Key::Equals, down); break;
        case AKEYCODE_COMMA:          set_key(Key::Comma, down); break;
        case AKEYCODE_PERIOD:         set_key(Key::Period, down); break;
        case AKEYCODE_SLASH:          set_key(Key::Slash, down); break;
        case AKEYCODE_SEMICOLON:      set_key(Key::Semicolon, down); break;
        case AKEYCODE_APOSTROPHE:     set_key(Key::Quote, down); break;
        case AKEYCODE_LEFT_BRACKET:   set_key(Key::LBracket, down); break;
        case AKEYCODE_RIGHT_BRACKET:  set_key(Key::RBracket, down); break;
        case AKEYCODE_GRAVE:          set_key(Key::Grave, down); break;
        case AKEYCODE_BACKSLASH:      set_key(Key::Backslash, down); break;
        default: break;
    }
}

void AndroidInput::process_motion_event(const AInputEvent* event) {
    int32_t action = AMotionEvent_getAction(event);
    int32_t action_type = action & AMOTION_EVENT_ACTION_MASK;
    int32_t pointer_idx = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                          >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

    int32_t pointer_count = AMotionEvent_getPointerCount(event);
    float x = AMotionEvent_getX(event, pointer_idx);
    float y = AMotionEvent_getY(event, pointer_idx);

    m_mouse_dx = static_cast<i32>(x) - m_mouse_x;
    m_mouse_dy = static_cast<i32>(y) - m_mouse_y;
    m_mouse_x  = static_cast<i32>(x);
    m_mouse_y  = static_cast<i32>(y);

    switch (action_type) {
        case AMOTION_EVENT_ACTION_DOWN:
        case AMOTION_EVENT_ACTION_POINTER_DOWN: {
            int32_t id = AMotionEvent_getPointerId(event, pointer_idx);
            i32 slot = alloc_touch_slot(id);
            if (slot >= 0) {
                auto& t = m_touches[slot];
                t.active      = true;
                t.x = t.start_x = x;
                t.y = t.start_y = y;
                t.down_time    = 0;
                t.pointer_id   = id;
                t.just_released = false;
            }
            break;
        }
        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_POINTER_UP: {
            int32_t id = AMotionEvent_getPointerId(event, pointer_idx);
            i32 slot = find_touch_slot(id);
            if (slot >= 0)
                finish_touch(static_cast<u32>(slot));
            break;
        }
        case AMOTION_EVENT_ACTION_MOVE: {
            for (int32_t i = 0; i < pointer_count; ++i) {
                int32_t id = AMotionEvent_getPointerId(event, i);
                i32 slot = find_touch_slot(id);
                if (slot < 0) {
                    // New pointer appeared in MOVE — slot it
                    slot = alloc_touch_slot(id);
                    if (slot < 0) continue;
                    auto& t = m_touches[slot];
                    t.active     = true;
                    t.x = t.start_x = AMotionEvent_getX(event, i);
                    t.y = t.start_y = AMotionEvent_getY(event, i);
                    t.pointer_id    = id;
                    continue;
                }

                auto& t = m_touches[slot];
                float prev_x = t.x, prev_y = t.y;
                t.x = AMotionEvent_getX(event, i);
                t.y = AMotionEvent_getY(event, i);

                m_swipe_dx = t.x - t.start_x;
                m_swipe_dy = t.y - t.start_y;
            }

            // Pinch detection
            u32 active_count = 0;
            u32 indices[2] = {};
            for (u32 i = 0; i < MAX_TOUCH && active_count < 2; ++i) {
                if (m_touches[i].active) {
                    indices[active_count] = i;
                    ++active_count;
                }
            }
            if (active_count == 2) {
                auto& a = m_touches[indices[0]];
                auto& b = m_touches[indices[1]];
                f32 dx = a.x - b.x;
                f32 dy = a.y - b.y;
                f32 dist = std::sqrt(dx*dx + dy*dy);
                // Store delta from last frame's pinch distance
                // (simplified: absolute distance as delta)
                m_pinch_delta = dist * 0.01f;
            }
            break;
        }
        case AMOTION_EVENT_ACTION_CANCEL: {
            for (auto& t : m_touches) {
                if (t.active) t.just_released = true;
            }
            break;
        }
        default:
            break;
    }
}

i32 AndroidInput::alloc_touch_slot(int32_t pointer_id) {
    // First check if already mapped
    i32 existing = find_touch_slot(pointer_id);
    if (existing >= 0) return existing;

    // Find free slot
    for (u32 i = 0; i < MAX_TOUCH; ++i) {
        if (!m_touches[i].active) return static_cast<i32>(i);
    }
    return -1;
}

i32 AndroidInput::find_touch_slot(int32_t pointer_id) const {
    for (u32 i = 0; i < MAX_TOUCH; ++i) {
        if (m_touches[i].active && m_touches[i].pointer_id == pointer_id)
            return static_cast<i32>(i);
    }
    return -1;
}

void AndroidInput::finish_touch(u32 slot) {
    if (slot >= MAX_TOUCH) return;
    auto& t = m_touches[slot];
    f32 dist = std::sqrt((t.x - t.start_x) * (t.x - t.start_x) +
                         (t.y - t.start_y) * (t.y - t.start_y));
    if (dist < 0.05f)
        m_tap = true;
    m_swipe_dx = t.x - t.start_x;
    m_swipe_dy = t.y - t.start_y;
    t.just_released = true;
}

// ─── State access methods ──────────────────────────────────

void AndroidInput::reset_state() {
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
    m_pause_requested = false;
    m_quit = false;
}

void AndroidInput::apply_state(const InputState& state) {
    m_state = state;
    // TODO: touch state mapping if needed
}

void AndroidInput::capture_state(InputState& out_state) const {
    out_state = m_state;
}

bool AndroidInput::quit_requested() const {
    return m_quit;
}

bool AndroidInput::is_key_pressed(Key k) const {
    usize i = static_cast<usize>(k);
    if (i >= static_cast<usize>(Key::COUNT)) return false;
    return m_state.keys[i];
}

bool AndroidInput::is_key_just_pressed(Key k) const {
    usize i = static_cast<usize>(k);
    if (i >= static_cast<usize>(Key::COUNT)) return false;
    return m_state.keys[i] && !m_prev.keys[i];
}

bool AndroidInput::is_key_just_released(Key k) const {
    usize i = static_cast<usize>(k);
    if (i >= static_cast<usize>(Key::COUNT)) return false;
    return !m_state.keys[i] && m_prev.keys[i];
}

bool AndroidInput::is_mouse_pressed(MouseButton b) const {
    (void)b;
    return m_touch_count > 0;
}

bool AndroidInput::is_mouse_just_pressed(MouseButton b) const {
    usize i = static_cast<usize>(b);
    if (i >= static_cast<usize>(MouseButton::COUNT)) return false;
    return m_state.mouse_buttons[i] && !m_prev.mouse_buttons[i];
}

bool AndroidInput::is_mouse_just_released(MouseButton b) const {
    usize i = static_cast<usize>(b);
    if (i >= static_cast<usize>(MouseButton::COUNT)) return false;
    return !m_state.mouse_buttons[i] && m_prev.mouse_buttons[i];
}

} // namespace pino

#endif // __ANDROID__
