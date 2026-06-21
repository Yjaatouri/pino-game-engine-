#pragma once

#include "engine/core/types.h"

namespace pino {

// -----------------------------------------------------------------------
// Key codes — platform-independent
// -----------------------------------------------------------------------
enum class Key : u32 {
    Unknown = 0,

    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    _0, _1, _2, _3, _4, _5, _6, _7, _8, _9,

    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

    LShift, RShift, LCtrl, RCtrl, LAlt, RAlt,
    LGui,   RGui,

    Up, Down, Left, Right,
    PageUp, PageDown, Home, End,

    Space, Enter, Escape, Backspace, Tab, Delete, Insert,

    Minus, Equals, LBracket, RBracket, Semicolon, Quote,
    Comma, Period, Slash, Backslash, Grave,

    CapsLock, NumLock, ScrollLock,
    PrintScreen, Pause,

    KP_0, KP_1, KP_2, KP_3, KP_4,
    KP_5, KP_6, KP_7, KP_8, KP_9,
    KP_Decimal, KP_Divide, KP_Multiply,
    KP_Subtract, KP_Add, KP_Enter,

    COUNT,
};

// -----------------------------------------------------------------------
// Mouse buttons
// -----------------------------------------------------------------------
enum class MouseButton : u32 {
    Left   = 0,
    Middle = 1,
    Right  = 2,
    X1     = 3,
    X2     = 4,
    COUNT  = 5,
};

// -----------------------------------------------------------------------
// Per-frame input state (platform-independent)
// -----------------------------------------------------------------------
struct InputState {
    bool keys[static_cast<usize>(Key::COUNT)]               = {};
    bool keys_down[static_cast<usize>(Key::COUNT)]          = {};
    bool keys_up[static_cast<usize>(Key::COUNT)]            = {};

    i32  mouse_x        = 0;
    i32  mouse_y        = 0;
    i32  mouse_dx       = 0;
    i32  mouse_dy       = 0;
    bool mouse_buttons[static_cast<usize>(MouseButton::COUNT)] = {};
    i32  scroll_dx      = 0;
    i32  scroll_dy      = 0;
};

// -----------------------------------------------------------------------
// Input — abstract interface
// -----------------------------------------------------------------------
class Input {
public:
    virtual ~Input() = default;

    // ---- Singleton convenience ----
    static Input*        instance()       { return s_instance; }
    static void set_instance(Input* in) { s_instance = in; }

    // Call at start of each frame (saves previous state, clears deltas)
    virtual void begin_frame() = 0;

    // Feed a platform event to the input system.
    virtual void process_event(const void* event) = 0;

    // Quit signal (e.g. SDL_QUIT, Android onBackPressed)
    virtual bool quit_requested() const = 0;

    // ---- Keyboard ----
    virtual bool is_key_pressed(Key k)       const = 0;  // held this frame
    virtual bool is_key_just_pressed(Key k)  const = 0;  // transitioned down this frame
    virtual bool is_key_just_released(Key k) const = 0;  // transitioned up this frame

    // ---- Keyboard (backward-compat wrappers) ----
    bool key_down(Key k)    const { return is_key_pressed(k); }
    bool key_pressed(Key k) const { return is_key_just_pressed(k); }
    bool key_released(Key k)const { return is_key_just_released(k); }

    // ---- Mouse ----
    virtual bool is_mouse_pressed(MouseButton b)       const = 0;
    virtual bool is_mouse_just_pressed(MouseButton b)  const = 0;
    virtual bool is_mouse_just_released(MouseButton b) const = 0;

    virtual i32 mouse_x()   const = 0;
    virtual i32 mouse_y()   const = 0;
    virtual i32 mouse_dx()  const = 0;
    virtual i32 mouse_dy()  const = 0;
    virtual i32 scroll_dx() const = 0;
    virtual i32 scroll_dy() const = 0;

    // ---- Mouse (backward-compat wrappers) ----
    bool mouse_down(MouseButton b)       const { return is_mouse_pressed(b); }
    bool mouse_pressed(MouseButton b)    const { return is_mouse_just_pressed(b); }
    bool mouse_released(MouseButton b)   const { return is_mouse_just_released(b); }

    // ---- Cursor control ----
    virtual void set_cursor_visible(bool visible) = 0;
    virtual bool is_cursor_visible() const = 0;
    virtual void set_cursor_locked(bool locked) = 0;
    virtual bool is_cursor_locked() const = 0;

    // ---- Touch gestures (mobile) ----
    // Number of active touch points
    virtual i32 touch_count() const = 0;

    // Accumulated swipe delta since touch began (normalised 0-1)
    virtual f32 swipe_delta_x() const = 0;
    virtual f32 swipe_delta_y() const = 0;

    // Quick tap detected this frame
    virtual bool is_tap() const = 0;

    // Change in distance between two touch points (normalised)
    virtual f32 pinch_delta() const = 0;

    // ---- State access (for focus reset, recorder, etc.) ----
    // Reset all input state to defaults (e.g. on focus loss)
    virtual void reset_state() = 0;

    // Apply a fully-specified input state (used by playback, testing)
    virtual void apply_state(const InputState& state) = 0;

    // Capture the current raw input state
    virtual void capture_state(InputState& out_state) const = 0;

    // Platform-backend specific pause toggle (e.g. Android back button).
    // Returns true once per button press; engine should toggle pause.
    virtual bool pause_requested() const { return false; }

private:
    static Input* s_instance;
};

} // namespace pino
