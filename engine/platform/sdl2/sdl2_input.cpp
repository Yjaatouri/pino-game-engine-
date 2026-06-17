#include "sdl2_input.h"
#include <SDL.h>
#include <cmath>
#include <cstring>

namespace pino {

Input* Input::s_instance = nullptr;

// -----------------------------------------------------------------------
// SDL scancode → pino Key
// -----------------------------------------------------------------------
static Key map_key(SDL_Scancode sc) {
    switch (sc) {
        case SDL_SCANCODE_A: return Key::A;
        case SDL_SCANCODE_B: return Key::B;
        case SDL_SCANCODE_C: return Key::C;
        case SDL_SCANCODE_D: return Key::D;
        case SDL_SCANCODE_E: return Key::E;
        case SDL_SCANCODE_F: return Key::F;
        case SDL_SCANCODE_G: return Key::G;
        case SDL_SCANCODE_H: return Key::H;
        case SDL_SCANCODE_I: return Key::I;
        case SDL_SCANCODE_J: return Key::J;
        case SDL_SCANCODE_K: return Key::K;
        case SDL_SCANCODE_L: return Key::L;
        case SDL_SCANCODE_M: return Key::M;
        case SDL_SCANCODE_N: return Key::N;
        case SDL_SCANCODE_O: return Key::O;
        case SDL_SCANCODE_P: return Key::P;
        case SDL_SCANCODE_Q: return Key::Q;
        case SDL_SCANCODE_R: return Key::R;
        case SDL_SCANCODE_S: return Key::S;
        case SDL_SCANCODE_T: return Key::T;
        case SDL_SCANCODE_U: return Key::U;
        case SDL_SCANCODE_V: return Key::V;
        case SDL_SCANCODE_W: return Key::W;
        case SDL_SCANCODE_X: return Key::X;
        case SDL_SCANCODE_Y: return Key::Y;
        case SDL_SCANCODE_Z: return Key::Z;
        case SDL_SCANCODE_0: return Key::_0;
        case SDL_SCANCODE_1: return Key::_1;
        case SDL_SCANCODE_2: return Key::_2;
        case SDL_SCANCODE_3: return Key::_3;
        case SDL_SCANCODE_4: return Key::_4;
        case SDL_SCANCODE_5: return Key::_5;
        case SDL_SCANCODE_6: return Key::_6;
        case SDL_SCANCODE_7: return Key::_7;
        case SDL_SCANCODE_8: return Key::_8;
        case SDL_SCANCODE_9: return Key::_9;
        case SDL_SCANCODE_F1:  return Key::F1;
        case SDL_SCANCODE_F2:  return Key::F2;
        case SDL_SCANCODE_F3:  return Key::F3;
        case SDL_SCANCODE_F4:  return Key::F4;
        case SDL_SCANCODE_F5:  return Key::F5;
        case SDL_SCANCODE_F6:  return Key::F6;
        case SDL_SCANCODE_F7:  return Key::F7;
        case SDL_SCANCODE_F8:  return Key::F8;
        case SDL_SCANCODE_F9:  return Key::F9;
        case SDL_SCANCODE_F10: return Key::F10;
        case SDL_SCANCODE_F11: return Key::F11;
        case SDL_SCANCODE_F12: return Key::F12;
        case SDL_SCANCODE_LSHIFT:  return Key::LShift;
        case SDL_SCANCODE_RSHIFT:  return Key::RShift;
        case SDL_SCANCODE_LCTRL:   return Key::LCtrl;
        case SDL_SCANCODE_RCTRL:   return Key::RCtrl;
        case SDL_SCANCODE_LALT:    return Key::LAlt;
        case SDL_SCANCODE_RALT:    return Key::RAlt;
        case SDL_SCANCODE_LGUI:    return Key::LGui;
        case SDL_SCANCODE_RGUI:    return Key::RGui;
        case SDL_SCANCODE_UP:      return Key::Up;
        case SDL_SCANCODE_DOWN:    return Key::Down;
        case SDL_SCANCODE_LEFT:    return Key::Left;
        case SDL_SCANCODE_RIGHT:   return Key::Right;
        case SDL_SCANCODE_PAGEUP:   return Key::PageUp;
        case SDL_SCANCODE_PAGEDOWN: return Key::PageDown;
        case SDL_SCANCODE_HOME:    return Key::Home;
        case SDL_SCANCODE_END:     return Key::End;
        case SDL_SCANCODE_SPACE:   return Key::Space;
        case SDL_SCANCODE_RETURN:  return Key::Enter;
        case SDL_SCANCODE_ESCAPE:  return Key::Escape;
        case SDL_SCANCODE_BACKSPACE: return Key::Backspace;
        case SDL_SCANCODE_TAB:     return Key::Tab;
        case SDL_SCANCODE_DELETE:  return Key::Delete;
        case SDL_SCANCODE_INSERT:  return Key::Insert;
        case SDL_SCANCODE_MINUS:       return Key::Minus;
        case SDL_SCANCODE_EQUALS:      return Key::Equals;
        case SDL_SCANCODE_LEFTBRACKET: return Key::LBracket;
        case SDL_SCANCODE_RIGHTBRACKET:return Key::RBracket;
        case SDL_SCANCODE_SEMICOLON:   return Key::Semicolon;
        case SDL_SCANCODE_APOSTROPHE:  return Key::Quote;
        case SDL_SCANCODE_COMMA:  return Key::Comma;
        case SDL_SCANCODE_PERIOD: return Key::Period;
        case SDL_SCANCODE_SLASH:  return Key::Slash;
        case SDL_SCANCODE_BACKSLASH:  return Key::Backslash;
        case SDL_SCANCODE_GRAVE:  return Key::Grave;
        case SDL_SCANCODE_CAPSLOCK:    return Key::CapsLock;
        case SDL_SCANCODE_NUMLOCKCLEAR:return Key::NumLock;
        case SDL_SCANCODE_SCROLLLOCK:  return Key::ScrollLock;
        case SDL_SCANCODE_PRINTSCREEN: return Key::PrintScreen;
        case SDL_SCANCODE_PAUSE:       return Key::Pause;
        case SDL_SCANCODE_KP_0:        return Key::KP_0;
        case SDL_SCANCODE_KP_1:        return Key::KP_1;
        case SDL_SCANCODE_KP_2:        return Key::KP_2;
        case SDL_SCANCODE_KP_3:        return Key::KP_3;
        case SDL_SCANCODE_KP_4:        return Key::KP_4;
        case SDL_SCANCODE_KP_5:        return Key::KP_5;
        case SDL_SCANCODE_KP_6:        return Key::KP_6;
        case SDL_SCANCODE_KP_7:        return Key::KP_7;
        case SDL_SCANCODE_KP_8:        return Key::KP_8;
        case SDL_SCANCODE_KP_9:        return Key::KP_9;
        case SDL_SCANCODE_KP_DECIMAL:  return Key::KP_Decimal;
        case SDL_SCANCODE_KP_DIVIDE:   return Key::KP_Divide;
        case SDL_SCANCODE_KP_MULTIPLY: return Key::KP_Multiply;
        case SDL_SCANCODE_KP_MINUS:    return Key::KP_Subtract;
        case SDL_SCANCODE_KP_PLUS:     return Key::KP_Add;
        case SDL_SCANCODE_KP_ENTER:    return Key::KP_Enter;
        default: return Key::Unknown;
    }
}

// -----------------------------------------------------------------------
// Sdl2Input
// -----------------------------------------------------------------------
Sdl2Input::Sdl2Input() {
    std::memset(&m_state, 0, sizeof(m_state));
    std::memset(&m_prev,  0, sizeof(m_prev));
    set_instance(this);
    SDL_GameControllerEventState(SDL_ENABLE);
}

Sdl2Input::~Sdl2Input() {
    for (i32 i = 0; i < MAX_GAMEPADS; ++i) {
        if (m_gamepads[i].controller) {
            SDL_GameControllerClose(static_cast<SDL_GameController*>(m_gamepads[i].controller));
        }
    }
    if (instance() == this) set_instance(nullptr);
}

void Sdl2Input::begin_frame() {
    m_prev  = m_state;
    m_state.mouse_dx  = 0;
    m_state.mouse_dy  = 0;
    m_state.scroll_dx = 0;
    m_state.scroll_dy = 0;

    // Save previous gamepad button states
    for (i32 g = 0; g < MAX_GAMEPADS; ++g) {
        for (i32 b = 0; b < Sdl2Input::MAX_GAMEPAD_BUTTONS; ++b)
            m_gamepads[g].buttons_prev[b] = m_gamepads[g].buttons[b];
    }

    // Clear per-frame gesture flags
    m_tap         = false;
    m_pinch_delta = 0;
    m_swipe_dx    = 0;
    m_swipe_dy    = 0;

    // Mark just_released touches as inactive
    for (auto& s : m_touch_slots) {
        if (s.in_use && s.pt.just_released) {
            s.in_use = false;
            s.pt.just_released = false;
        }
    }

    // Re-count active touches
    m_touch_count = 0;
    for (auto& s : m_touch_slots)
        if (s.in_use && s.pt.active) ++m_touch_count;
}

void Sdl2Input::reset_state() {
    std::memset(&m_state, 0, sizeof(m_state));
    std::memset(&m_prev,  0, sizeof(m_prev));
    m_touch_count = 0;
    for (auto& s : m_touch_slots) {
        s.in_use = false;
        s.pt = TouchPt{};
    }
    m_swipe_dx = m_swipe_dy = 0;
    m_pinch_delta = 0;
    m_tap = false;

    for (i32 g = 0; g < MAX_GAMEPADS; ++g) {
        for (i32 b = 0; b < Sdl2Input::MAX_GAMEPAD_BUTTONS; ++b)
            m_gamepads[g].buttons[b] = m_gamepads[g].buttons_prev[b] = false;
    }
}

void Sdl2Input::apply_state(const InputState& state) {
    m_state = state;
    m_prev  = state; // prevent just-pressed/released from triggering on first frame after apply
}

void Sdl2Input::capture_state(InputState& out_state) const {
    out_state = m_state;
}

// ── Touch slot allocation ────────────────────────────────────────

i32 Sdl2Input::find_touch_slot(i64 id) const {
    for (u32 i = 0; i < MAX_TOUCH; ++i)
        if (m_touch_slots[i].in_use && m_touch_slots[i].finger_id == id)
            return static_cast<i32>(i);
    return -1;
}

void Sdl2Input::finish_touch(u32 idx) {
    auto& t = m_touch_slots[idx].pt;
    u64 now = SDL_GetTicks64();
    f32 dist = std::sqrt((t.x - t.start_x) * (t.x - t.start_x) +
                         (t.y - t.start_y) * (t.y - t.start_y));
    if ((now - t.down_time) < 300 && dist < 0.05f)
        m_tap = true;
    m_swipe_dx = t.x - t.start_x;
    m_swipe_dy = t.y - t.start_y;
    t.just_released = true;
}

// ── Event processing ─────────────────────────────────────────────

void Sdl2Input::process_event(const void* ev) {
    const SDL_Event& e = *static_cast<const SDL_Event*>(ev);

    switch (e.type) {
        case SDL_QUIT:
            m_quit = true;
            break;

        case SDL_KEYDOWN:
        case SDL_KEYUP: {
            Key k = map_key(e.key.keysym.scancode);
            if (k != Key::Unknown) {
                bool pressed = (e.key.state == SDL_PRESSED);
                usize i = static_cast<usize>(k);
                m_state.keys[i] = pressed;
            }
            break;
        }

        case SDL_MOUSEMOTION:
            m_state.mouse_x  = e.motion.x;
            m_state.mouse_y  = e.motion.y;
            if (m_cursor_locked) {
                m_state.mouse_dx += e.motion.xrel;
                m_state.mouse_dy += e.motion.yrel;
            } else {
                m_state.mouse_dx = e.motion.xrel;
                m_state.mouse_dy = e.motion.yrel;
            }
            break;

        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
            auto idx = static_cast<usize>(e.button.button - 1);
            if (idx < static_cast<usize>(MouseButton::COUNT))
                m_state.mouse_buttons[idx] = (e.button.state == SDL_PRESSED);
            break;
        }

        case SDL_MOUSEWHEEL:
            m_state.scroll_dx = e.wheel.x;
            m_state.scroll_dy = e.wheel.y;
            break;

        // ── Touch (fixed slot map by SDL_FingerID) ───────────
        case SDL_FINGERDOWN: {
            i32 slot = find_touch_slot(e.tfinger.fingerId);
            if (slot >= 0) break; // already tracked
            // Find free slot
            for (u32 i = 0; i < MAX_TOUCH; ++i) {
                if (!m_touch_slots[i].in_use) {
                    m_touch_slots[i].in_use = true;
                    m_touch_slots[i].finger_id = e.tfinger.fingerId;
                    auto& t = m_touch_slots[i].pt;
                    t.active    = true;
                    t.x = t.start_x = e.tfinger.x;
                    t.y = t.start_y = e.tfinger.y;
                    t.down_time = SDL_GetTicks64();
                    t.just_released = false;
                    break;
                }
            }
            break;
        }
        case SDL_FINGERUP: {
            i32 slot = find_touch_slot(e.tfinger.fingerId);
            if (slot >= 0) finish_touch(static_cast<u32>(slot));
            break;
        }
        case SDL_FINGERMOTION: {
            i32 slot = find_touch_slot(e.tfinger.fingerId);
            if (slot < 0) break;
            auto& t = m_touch_slots[slot].pt;
            if (!t.active) {
                t.active = true;
                t.start_x = t.x;
                t.start_y = t.y;
                t.down_time = SDL_GetTicks64();
            }
            f32 prev_x = t.x, prev_y = t.y;
            t.x = e.tfinger.x;
            t.y = e.tfinger.y;

            m_swipe_dx = t.x - t.start_x;
            m_swipe_dy = t.y - t.start_y;

            u32 active_count = 0;
            u32 other_idx = 0;
            for (u32 i = 0; i < MAX_TOUCH; ++i) {
                if (m_touch_slots[i].in_use && m_touch_slots[i].pt.active) {
                    ++active_count;
                    if (i != static_cast<u32>(slot)) other_idx = i;
                }
            }
            if (active_count == 2) {
                auto& o = m_touch_slots[other_idx].pt;
                f32 prev_dx = prev_x - o.x;
                f32 prev_dy = prev_y - o.y;
                f32 cur_dx  = t.x - o.x;
                f32 cur_dy  = t.y - o.y;
                f32 prev_dist = std::sqrt(prev_dx * prev_dx + prev_dy * prev_dy);
                f32 cur_dist  = std::sqrt(cur_dx  * cur_dx  + cur_dy  * cur_dy);
                m_pinch_delta = cur_dist - prev_dist;
            }
            break;
        }

        // ── Window focus ─────────────────────────────────────
        case SDL_WINDOWEVENT:
            if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                m_window_focused = false;
                reset_state();
                // Sync SDL's key state so we don't miss releases
                SDL_PumpEvents();
            } else if (e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                m_window_focused = true;
                SDL_PumpEvents();
            }
            break;

        // ── Gamepad ──────────────────────────────────────────
        case SDL_CONTROLLERDEVICEADDED:
            open_gamepad(e.cdevice.which);
            break;

        case SDL_CONTROLLERDEVICEREMOVED:
            close_gamepad(e.cdevice.which);
            break;

        case SDL_CONTROLLERAXISMOTION: {
            for (i32 i = 0; i < MAX_GAMEPADS; ++i) {
                auto& g = m_gamepads[i];
                if (!g.controller) continue;
                if (SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(static_cast<SDL_GameController*>(g.controller))) != e.caxis.which)
                    continue;
                float val = static_cast<float>(e.caxis.value) / 32767.0f;
                if (val > 1.0f) val = 1.0f;
                if (val < -1.0f) val = -1.0f;
                switch (e.caxis.axis) {
                    case SDL_CONTROLLER_AXIS_LEFTX:  g.left_stick_x  = apply_deadzone(val); break;
                    case SDL_CONTROLLER_AXIS_LEFTY:  g.left_stick_y  = apply_deadzone(val); break;
                    case SDL_CONTROLLER_AXIS_RIGHTX: g.right_stick_x = apply_deadzone(val); break;
                    case SDL_CONTROLLER_AXIS_RIGHTY: g.right_stick_y = apply_deadzone(val); break;
                    case SDL_CONTROLLER_AXIS_TRIGGERLEFT:  g.left_trigger  = (val + 1.0f) * 0.5f; break;
                    case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: g.right_trigger = (val + 1.0f) * 0.5f; break;
                }
                break;
            }
            break;
        }

        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP: {
            for (i32 i = 0; i < MAX_GAMEPADS; ++i) {
                auto& g = m_gamepads[i];
                if (!g.controller) continue;
                if (SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(static_cast<SDL_GameController*>(g.controller))) != e.cbutton.which)
                    continue;
                g.buttons[e.cbutton.button] = (e.cbutton.state == SDL_PRESSED);
                break;
            }
            break;
        }

        default:
            break;
    }
}

// ── Cursor control ───────────────────────────────────────────────

void Sdl2Input::set_cursor_visible(bool visible) {
    m_cursor_visible = visible;
    SDL_ShowCursor(visible ? SDL_ENABLE : SDL_DISABLE);
}

void Sdl2Input::set_cursor_locked(bool locked) {
    m_cursor_locked = locked;
    SDL_SetRelativeMouseMode(locked ? SDL_TRUE : SDL_FALSE);
    if (locked) {
        SDL_ShowCursor(SDL_DISABLE);
        m_cursor_visible = false;
    } else {
        SDL_ShowCursor(SDL_ENABLE);
        m_cursor_visible = true;
    }
}

// ── Gamepad ──────────────────────────────────────────────────────

void Sdl2Input::open_gamepad(i32 device_index) {
    for (i32 i = 0; i < MAX_GAMEPADS; ++i) {
        if (!m_gamepads[i].attached) {
            m_gamepads[i].controller = SDL_GameControllerOpen(device_index);
            if (m_gamepads[i].controller) {
                m_gamepads[i].instance_id = SDL_JoystickInstanceID(
                    SDL_GameControllerGetJoystick(static_cast<SDL_GameController*>(m_gamepads[i].controller)));
                m_gamepads[i].attached = true;
                ++m_gamepad_count;
            }
            break;
        }
    }
}

void Sdl2Input::close_gamepad(i32 instance_id) {
    for (i32 i = 0; i < MAX_GAMEPADS; ++i) {
        if (m_gamepads[i].instance_id == instance_id) {
            if (m_gamepads[i].controller)
                SDL_GameControllerClose(static_cast<SDL_GameController*>(m_gamepads[i].controller));
            std::memset(&m_gamepads[i], 0, sizeof(GamepadState));
            --m_gamepad_count;
            break;
        }
    }
}

float Sdl2Input::apply_deadzone(float value, float deadzone) const {
    if (std::fabs(value) < deadzone) return 0.0f;
    return (value > 0 ? 1.0f : -1.0f) * (std::fabs(value) - deadzone) / (1.0f - deadzone);
}

// ── Keyboard queries ─────────────────────────────────────────────

bool Sdl2Input::is_key_pressed(Key k) const {
    return m_state.keys[static_cast<usize>(k)];
}
bool Sdl2Input::is_key_just_pressed(Key k) const {
    usize i = static_cast<usize>(k);
    return m_state.keys[i] && !m_prev.keys[i];
}
bool Sdl2Input::is_key_just_released(Key k) const {
    usize i = static_cast<usize>(k);
    return !m_state.keys[i] && m_prev.keys[i];
}

// ── Mouse queries ────────────────────────────────────────────────

bool Sdl2Input::is_mouse_pressed(MouseButton b) const {
    return m_state.mouse_buttons[static_cast<usize>(b)];
}
bool Sdl2Input::is_mouse_just_pressed(MouseButton b) const {
    usize i = static_cast<usize>(b);
    return m_state.mouse_buttons[i] && !m_prev.mouse_buttons[i];
}

} // namespace pino
