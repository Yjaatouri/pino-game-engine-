#pragma once

#include "engine/platform/input.h"

namespace pino {

class GamepadManager;

class Sdl2Input final : public Input {
public:
    Sdl2Input();
    ~Sdl2Input() override;

    void begin_frame() override;
    void process_event(const void* event) override;
    bool quit_requested() const override { return m_quit; }

    // Keyboard
    bool is_key_pressed(Key k)       const override;
    bool is_key_just_pressed(Key k)  const override;
    bool is_key_just_released(Key k) const override;

    // Mouse
    bool is_mouse_pressed(MouseButton b)       const override;
    bool is_mouse_just_pressed(MouseButton b)  const override;
    bool is_mouse_just_released(MouseButton b) const override;
    i32  mouse_x()   const override { return m_state.mouse_x; }
    i32  mouse_y()   const override { return m_state.mouse_y; }
    i32  mouse_dx()  const override { return m_state.mouse_dx; }
    i32  mouse_dy()  const override { return m_state.mouse_dy; }
    i32  scroll_dx() const override { return m_state.scroll_dx; }
    i32  scroll_dy() const override { return m_state.scroll_dy; }

    // Cursor
    void  set_cursor_visible(bool visible) override;
    bool  is_cursor_visible() const override { return m_cursor_visible; }
    void  set_cursor_locked(bool locked) override;
    bool  is_cursor_locked() const override { return m_cursor_locked; }

    // Touch gestures
    i32  touch_count()   const override { return m_touch_count; }
    f32  swipe_delta_x() const override { return m_swipe_dx; }
    f32  swipe_delta_y() const override { return m_swipe_dy; }
    bool is_tap()        const override { return m_tap; }
    f32  pinch_delta()   const override { return m_pinch_delta; }

    // State
    void reset_state() override;
    void apply_state(const InputState& state) override;
    void capture_state(InputState& out_state) const override;

    // Wire up the authoritative gamepad manager.
    void set_gamepad_manager(GamepadManager* mgr);

private:
    static constexpr u32 MAX_TOUCH = 10;
    static constexpr i32 MAX_GAMEPADS = 4;

    struct TouchPt {
        bool  active      = false;
        f32   x = 0, y = 0;
        f32   start_x = 0, start_y = 0;
        u64   down_time = 0;
        bool  just_released = false;
    };

    struct TouchSlot {
        i64 finger_id = 0;
        bool in_use = false;
        TouchPt pt;
    };

    // SDL gamepad handles (platform-owned, no state).
    struct SdlGamepad {
        void* controller = nullptr; // SDL_GameController*
        i32   instance_id = -1;
        i32   slot = -1;
    };
    SdlGamepad m_sdl_gamepads[MAX_GAMEPADS];

    i32 find_touch_slot(i64 id) const;
    void finish_touch(u32 idx);
    i32 sdl_find_by_instance(i32 id) const;
    i32 sdl_find_free_slot() const;

    void open_gamepad(i32 device_index);
    void close_gamepad(i32 instance_id);

    InputState m_state;
    InputState m_prev;
    bool m_quit = false;

    bool m_cursor_visible = true;
    bool m_cursor_locked  = false;
    bool m_window_focused = true;

    // Touch tracking (fixed slots indexed by finger ID)
    TouchSlot m_touch_slots[MAX_TOUCH];
    u32       m_touch_count = 0;

    // Frame gesture results
    f32  m_swipe_dx     = 0;
    f32  m_swipe_dy     = 0;
    f32  m_pinch_delta  = 0;
    bool m_tap          = false;

    GamepadManager* m_gamepad_mgr = nullptr;
};

} // namespace pino
