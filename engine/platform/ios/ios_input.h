#pragma once
#include "engine/platform/input.h"

namespace pino {

class IOSInput final : public Input {
public:
    IOSInput();
    ~IOSInput() override;

    void begin_frame() override;
    void process_event(const void* event) override;
    bool quit_requested() const override;

    bool is_key_pressed(Key k)       const override;
    bool is_key_just_pressed(Key k)  const override;
    bool is_key_just_released(Key k) const override;

    bool is_mouse_pressed(MouseButton b)       const override;
    bool is_mouse_just_pressed(MouseButton b)  const override;
    bool is_mouse_just_released(MouseButton b) const override;

    i32  mouse_x()   const override { return m_mouse_x; }
    i32  mouse_y()   const override { return m_mouse_y; }
    i32  mouse_dx()  const override { return m_mouse_dx; }
    i32  mouse_dy()  const override { return m_mouse_dy; }
    i32  scroll_dx() const override { return m_scroll_dx; }
    i32  scroll_dy() const override { return m_scroll_dy; }

    i32  touch_count()   const override { return m_touch_count; }
    f32  swipe_delta_x() const override { return m_swipe_dx; }
    f32  swipe_delta_y() const override { return m_swipe_dy; }
    bool is_tap()        const override { return m_tap; }
    f32  pinch_delta()   const override { return m_pinch_delta; }

    void set_cursor_visible(bool) override  {}
    bool is_cursor_visible() const override { return true; }
    void set_cursor_locked(bool) override   {}
    bool is_cursor_locked() const override  { return false; }

    void reset_state() override;
    void apply_state(const InputState& state) override;
    void capture_state(InputState& out_state) const override;

    // Called from the EAGLView touch handlers
    void touch_began(f32 x, f32 y, u64 ptr_id);
    void touch_moved(f32 x, f32 y, u64 ptr_id);
    void touch_ended(f32 x, f32 y, u64 ptr_id);
    void touch_cancelled();

private:
    static constexpr u32 MAX_TOUCH = 10;

    struct TouchPt {
        bool active   = false;
        f32  x = 0, y = 0;
        f32  start_x = 0, start_y = 0;
        u64  ptr_id  = 0;
        bool just_released = false;
    };

    u32 touch_index_for_ptr(u64 ptr_id);
    void finish_touch(u32 idx);

    InputState m_state;
    InputState m_prev;

    TouchPt m_touches[MAX_TOUCH];
    u32     m_touch_count = 0;

    f32  m_swipe_dx    = 0;
    f32  m_swipe_dy    = 0;
    f32  m_pinch_delta = 0;
    bool m_tap         = false;
    bool m_quit        = false;

    i32 m_mouse_x   = 0;
    i32 m_mouse_y   = 0;
    i32 m_mouse_dx  = 0;
    i32 m_mouse_dy  = 0;
    i32 m_scroll_dx = 0;
    i32 m_scroll_dy = 0;
};

} // namespace pino
