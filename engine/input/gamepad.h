#pragma once

#include "engine/core/types.h"

namespace pino {

struct GamepadHandle {
    i32 index = -1;
    bool is_valid() const { return index >= 0; }
    explicit operator bool() const { return is_valid(); }
};

struct GamepadState {
    float left_stick_x  = 0;
    float left_stick_y  = 0;
    float right_stick_x = 0;
    float right_stick_y = 0;
    float left_trigger  = 0;
    float right_trigger = 0;

    bool a      = false;
    bool b      = false;
    bool x      = false;
    bool y      = false;
    bool back   = false;
    bool guide  = false;
    bool start  = false;
    bool left_stick   = false;
    bool right_stick  = false;
    bool left_shoulder  = false;
    bool right_shoulder = false;
    bool dpad_up    = false;
    bool dpad_down  = false;
    bool dpad_left  = false;
    bool dpad_right = false;
};

class GamepadManager {
public:
    GamepadManager();
    ~GamepadManager();

    GamepadManager(const GamepadManager&) = delete;
    GamepadManager& operator=(const GamepadManager&) = delete;

    void on_controller_added(i32 device_index);
    void on_controller_removed(i32 instance_id);
    void on_axis_motion(i32 instance_id, i32 axis, i16 value);
    void on_button(i32 instance_id, i32 button, bool pressed);

    void frame_advance();

    i32 count() const { return m_count; }
    GamepadHandle handle(i32 index) const;
    const GamepadState* get_state(GamepadHandle h) const;
    const GamepadState* get_state(i32 index) const;

    void set_rumble(GamepadHandle h, float low_freq, float high_freq, u32 duration_ms);

    void set_deadzone(float deadzone) { m_deadzone = deadzone; }
    float deadzone() const { return m_deadzone; }

private:
    static constexpr i32 MAX_GAMEPADS = 4;

    struct Controller {
        void* sdl_controller = nullptr; // SDL_GameController*
        i32   instance_id = -1;
        bool  attached = false;
        GamepadState state;
        GamepadState prev_state;
    };

    Controller m_controllers[MAX_GAMEPADS];
    i32        m_count = 0;
    float      m_deadzone = 0.15f;

    i32 find_by_instance(i32 id) const;
    i32 find_free_slot() const;
};

} // namespace pino
