#pragma once
#include "engine/platform/input.h"
#include <android/input.h>

namespace pino {

class AndroidInput final : public Input {
public:
    AndroidInput();
    ~AndroidInput() override;

    void begin_frame() override;
    void process_event(const void* event) override;
    bool quit_requested() const override;

    bool is_key_pressed(Key k)       const override;
    bool is_key_just_pressed(Key k)  const override;
    bool is_key_just_released(Key k) const override;

    bool is_mouse_pressed(MouseButton b)       const override;
    bool is_mouse_just_pressed(MouseButton b)  const override;

    i32  mouse_x()   const override { return m_mouse_x; }
    i32  mouse_y()   const override { return m_mouse_y; }
    i32  mouse_dx()  const override { return m_mouse_dx; }
    i32  mouse_dy()  const override { return m_mouse_dy; }
    i32  scroll_dx() const override { return m_state.scroll_dx; }
    i32  scroll_dy() const override { return m_state.scroll_dy; }

    i32  touch_count()   const override { return m_touch_count; }
    f32  swipe_delta_x() const override { return m_swipe_dx; }
    f32  swipe_delta_y() const override { return m_swipe_dy; }
    bool is_tap()        const override { return m_tap; }
    f32  pinch_delta()   const override { return m_pinch_delta; }

    void set_cursor_visible(bool) override  {}
    bool is_cursor_visible() const override { return true; }
    void set_cursor_locked(bool) override   {}
    bool is_cursor_locked() const override  { return false; }

    // State access (for focus reset, recorder, etc.)
    void reset_state() override;
    void apply_state(const InputState& state) override;
    void capture_state(InputState& out_state) const override;

    // Back button requested pause this frame
    bool pause_requested() const { return m_pause_requested; }

    void process_key_event(const AInputEvent* event);
    void process_motion_event(const AInputEvent* event);

private:
    static constexpr u32 MAX_TOUCH = 10;

    struct TouchPt {
        bool    active      = false;
        f32     x = 0, y = 0;
        f32     start_x = 0, start_y = 0;
        u64     down_time = 0;
        bool    just_released = false;
        int32_t pointer_id = -1;
    };

    i32 alloc_touch_slot(int32_t pointer_id);
    i32 find_touch_slot(int32_t pointer_id) const;
    void finish_touch(u32 slot);

    InputState m_state;
    InputState m_prev;
    bool m_quit            = false;
    bool m_pause_requested = false;

    TouchPt m_touches[MAX_TOUCH];
    u32     m_touch_count = 0;

    f32  m_swipe_dx    = 0;
    f32  m_swipe_dy    = 0;
    f32  m_pinch_delta = 0;
    bool m_tap         = false;

    i32 m_mouse_x  = 0;
    i32 m_mouse_y  = 0;
    i32 m_mouse_dx = 0;
    i32 m_mouse_dy = 0;

    bool m_dpad_left  = false;
    bool m_dpad_right = false;
    bool m_dpad_up    = false;
    bool m_dpad_down  = false;
    bool m_dpad_center = false;
};

} // namespace pino
