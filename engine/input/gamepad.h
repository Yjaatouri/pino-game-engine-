#pragma once

#include "engine/core/types.h"
#include <cstring>

namespace pino {

// Named gamepad buttons (matches SDL_CONTROLLER_BUTTON_* layout).
enum class GamepadButton : u8 {
    A = 0, B = 1, X = 2, Y = 3,
    Back = 4, Guide = 5, Start = 6,
    LeftStick = 7, RightStick = 8,
    LeftShoulder = 9, RightShoulder = 10,
    DpadUp = 11, DpadDown = 12, DpadLeft = 13, DpadRight = 14,
    COUNT = 15
};

// Named gamepad axes (matches SDL_CONTROLLER_AXIS_* layout).
enum class GamepadAxis : u8 {
    LeftX = 0, LeftY = 1,
    RightX = 2, RightY = 3,
    TriggerLeft = 4, TriggerRight = 5,
    COUNT = 6
};

struct GamepadHandle {
    i32 index = -1;
    bool is_valid() const { return index >= 0; }
    explicit operator bool() const { return is_valid(); }
};

// Per-gamepad frame state (no platform dependencies).
struct GamepadState {
    float axes[static_cast<usize>(GamepadAxis::COUNT)] = {};
    u16 buttons     = 0; // current frame
    u16 buttons_prev = 0; // previous frame (for edge detection)

    bool is_down(GamepadButton b) const {
        return (buttons & (1u << static_cast<u8>(b))) != 0;
    }
    bool just_pressed(GamepadButton b) const {
        u16 mask = 1u << static_cast<u8>(b);
        return (buttons & mask) != 0 && (buttons_prev & mask) == 0;
    }
    bool just_released(GamepadButton b) const {
        u16 mask = 1u << static_cast<u8>(b);
        return (buttons & mask) == 0 && (buttons_prev & mask) != 0;
    }

    float axis(GamepadAxis a) const { return axes[static_cast<usize>(a)]; }

    // Named accessors
    bool a()              const { return is_down(GamepadButton::A); }
    bool b()              const { return is_down(GamepadButton::B); }
    bool x()              const { return is_down(GamepadButton::X); }
    bool y()              const { return is_down(GamepadButton::Y); }
    bool back()           const { return is_down(GamepadButton::Back); }
    bool start()          const { return is_down(GamepadButton::Start); }
    bool guide()          const { return is_down(GamepadButton::Guide); }
    bool left_stick()     const { return is_down(GamepadButton::LeftStick); }
    bool right_stick()    const { return is_down(GamepadButton::RightStick); }
    bool left_shoulder()  const { return is_down(GamepadButton::LeftShoulder); }
    bool right_shoulder() const { return is_down(GamepadButton::RightShoulder); }
    bool dpad_up()        const { return is_down(GamepadButton::DpadUp); }
    bool dpad_down()      const { return is_down(GamepadButton::DpadDown); }
    bool dpad_left()      const { return is_down(GamepadButton::DpadLeft); }
    bool dpad_right()     const { return is_down(GamepadButton::DpadRight); }

    float left_stick_x()  const { return axis(GamepadAxis::LeftX); }
    float left_stick_y()  const { return axis(GamepadAxis::LeftY); }
    float right_stick_x() const { return axis(GamepadAxis::RightX); }
    float right_stick_y() const { return axis(GamepadAxis::RightY); }
    float left_trigger()  const { return axis(GamepadAxis::TriggerLeft); }
    float right_trigger() const { return axis(GamepadAxis::TriggerRight); }
};

// Rumble callback type (platform provides the implementation).
using RumbleFn = void(*)(i32 slot, float low, float high, u32 duration_ms);

// Pure state manager — owns gamepad state arrays, no platform dependencies.
// Platform layer calls set_button/set_axis/on_connect/on_disconnect
// to update state; begin_frame() saves prev-state for edge detection.
class GamepadManager {
public:
    GamepadManager();
    ~GamepadManager() = default;

    GamepadManager(const GamepadManager&) = delete;
    GamepadManager& operator=(const GamepadManager&) = delete;

    // --- Frame lifecycle ---
    void begin_frame();

    // --- State updates (called by platform backend) ---
    void on_connect(GamepadHandle h);
    void on_disconnect(GamepadHandle h);
    void set_button(GamepadHandle h, GamepadButton b, bool pressed);
    void set_axis(GamepadHandle h, GamepadAxis a, float value);

    // --- Queries ---
    i32 count() const { return m_count; }
    GamepadHandle handle(i32 index) const;
    const GamepadState* get_state(GamepadHandle h) const;
    const GamepadState* get_state(i32 index) const;

    // --- Rumble (platform provides implementation) ---
    void set_rumble_fn(RumbleFn fn) { m_rumble_fn = fn; }
    void set_rumble(GamepadHandle h, float low, float high, u32 duration_ms);

    // --- Deadzone ---
    void set_deadzone(float dz) { m_deadzone = dz; }
    float deadzone() const { return m_deadzone; }
    float apply_deadzone(float value) const;

    // --- Reset ---
    void reset_state();

private:
    static constexpr i32 MAX_GAMEPADS = 4;

    struct Controller {
        bool attached = false;
        GamepadState state;
    };

    Controller   m_controllers[MAX_GAMEPADS];
    i32          m_count = 0;
    float        m_deadzone = 0.15f;
    RumbleFn     m_rumble_fn = nullptr;
};

} // namespace pino
